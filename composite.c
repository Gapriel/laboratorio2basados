/*
 * The Clear BSD License
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "fsl_port.h"
#include "fsl_gpio.h"

#include "usb_device_class.h"
#include "usb_device_hid.h"

#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"

#include "composite.h"

#include "hid_keyboard.h"
#include "hid_mouse.h"

#include "fsl_device_registers.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"

#include <stdio.h>
#include <stdlib.h>
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

#if (USB_DEVICE_CONFIG_HID < 2U)
#error USB_DEVICE_CONFIG_HID need to > 1U, Please change the MARCO USB_DEVICE_CONFIG_HID in file "usb_device_config.h".
#endif

#include "pin_mux.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void USB_DeviceClockInit(void);
void USB_DeviceIsrEnable(void);
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle);
#endif

static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event,
		void *param);
static void USB_DeviceApplicationInit(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

extern usb_device_class_struct_t g_UsbDeviceHidMouseConfig;
extern usb_device_class_struct_t g_UsbDeviceHidKeyboardConfig;

usb_device_composite_struct_t g_UsbDeviceComposite;

/* Set class configurations */
usb_device_class_config_struct_t g_CompositeClassConfig[USB_COMPOSITE_INTERFACE_COUNT] =
		{ { USB_DeviceHidKeyboardCallback, /* HID keyboard class callback pointer */
		(class_handle_t) NULL, /* The HID class handle, This field is set by USB_DeviceClassInit */
		&g_UsbDeviceHidKeyboardConfig, /* The HID keyboard configuration, including class code, subcode, and protocol,
		 class
		 type, transfer type, endpoint address, max packet size, etc.*/
		}, { USB_DeviceHidMouseCallback, /* HID mouse class callback pointer */
		(class_handle_t) NULL, /* The HID class handle, This field is set by USB_DeviceClassInit */
		&g_UsbDeviceHidMouseConfig, /* The HID mouse configuration, including class code, subcode, and protocol, class
		 type,
		 transfer type, endpoint address, max packet size, etc.*/
		} };

/* Set class configuration list */
usb_device_class_config_list_struct_t g_UsbDeviceCompositeConfigList = {
		g_CompositeClassConfig, /* Class configurations */
		USB_DeviceCallback, /* Device callback pointer */
		USB_COMPOSITE_INTERFACE_COUNT, /* Class count */
};

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U))
void USB0_IRQHandler(void) {
	USB_DeviceKhciIsrFunction(g_UsbDeviceComposite.deviceHandle);
	/* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
	 exception return operation might vector to incorrect interrupt */
	__DSB();
}
#endif
void USB_DeviceClockInit(void) {
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
	SystemCoreClockUpdate();
	CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
	/*
	 * If the SOC has USB KHCI dedicated RAM, the RAM memory needs to be clear after
	 * the KHCI clock is enabled. When the demo uses USB EHCI IP, the USB KHCI dedicated
	 * RAM can not be used and the memory can't be accessed.
	 */
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U))
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS) && (FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS > 0U))
	for (int i = 0; i < FSL_FEATURE_USB_KHCI_USB_RAM; i++)
	{
		((uint8_t *)FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS)[i] = 0x00U;
	}
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS */
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM */
#endif
}
void USB_DeviceIsrEnable(void) {
	uint8_t irqNumber;
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
	uint8_t usbDeviceKhciIrq[] = USB_IRQS;
	irqNumber = usbDeviceKhciIrq[CONTROLLER_ID - kUSB_ControllerKhci0];
#endif
	/* Install isr, set priority, and enable IRQ. */
#if defined(__GIC_PRIO_BITS)
	GIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
#else
	NVIC_SetPriority((IRQn_Type) irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
#endif
	EnableIRQ((IRQn_Type) irqNumber);
}
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle)
{
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
	USB_DeviceKhciTaskFunction(deviceHandle);
#endif
}
#endif

/* The Device callback */
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event,
		void *param) {
	usb_status_t error = kStatus_USB_Error;
	uint16_t *temp16 = (uint16_t *) param;
	uint8_t *temp8 = (uint8_t *) param;

	switch (event) {
	case kUSB_DeviceEventBusReset: {
		/* USB bus reset signal detected */
		g_UsbDeviceComposite.attach = 0U;
		error = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)) || \
    (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
		/* Get USB speed to configure the device, including max packet size and interval of the endpoints. */
		if (kStatus_USB_Success == USB_DeviceClassGetSpeed(CONTROLLER_ID, &g_UsbDeviceComposite.speed))
		{
			USB_DeviceSetSpeed(handle, g_UsbDeviceComposite.speed);
		}
#endif
	}
		break;
	case kUSB_DeviceEventSetConfiguration:
		if (param) {
			/* Set device configuration request */
			g_UsbDeviceComposite.attach = 1U;
			g_UsbDeviceComposite.currentConfiguration = *temp8;
			USB_DeviceHidMouseSetConfigure(g_UsbDeviceComposite.hidMouseHandle,
					*temp8);
			USB_DeviceHidKeyboardSetConfigure(
					g_UsbDeviceComposite.hidKeyboardHandle, *temp8);
			error = kStatus_USB_Success;
		}
		break;
	case kUSB_DeviceEventSetInterface:
		if (g_UsbDeviceComposite.attach) {
			/* Set device interface request */
			uint8_t interface = (uint8_t) ((*temp16 & 0xFF00U) >> 0x08U);
			uint8_t alternateSetting = (uint8_t) (*temp16 & 0x00FFU);
			if (interface < USB_COMPOSITE_INTERFACE_COUNT) {
				g_UsbDeviceComposite.currentInterfaceAlternateSetting[interface] =
						alternateSetting;
				USB_DeviceHidMouseSetInterface(
						g_UsbDeviceComposite.hidMouseHandle, interface,
						alternateSetting);
				USB_DeviceHidKeyboardSetInterface(
						g_UsbDeviceComposite.hidKeyboardHandle, interface,
						alternateSetting);
				error = kStatus_USB_Success;
			}
		}
		break;
	case kUSB_DeviceEventGetConfiguration:
		if (param) {
			/* Get current configuration request */
			*temp8 = g_UsbDeviceComposite.currentConfiguration;
			error = kStatus_USB_Success;
		}
		break;
	case kUSB_DeviceEventGetInterface:
		if (param) {
			/* Get current alternate setting of the interface request */
			uint8_t interface = (uint8_t) ((*temp16 & 0xFF00U) >> 0x08U);
			if (interface < USB_COMPOSITE_INTERFACE_COUNT) {
				*temp16 =
						(*temp16 & 0xFF00U)
								| g_UsbDeviceComposite.currentInterfaceAlternateSetting[interface];
				error = kStatus_USB_Success;
			} else {
				error = kStatus_USB_InvalidRequest;
			}
		}
		break;
	case kUSB_DeviceEventGetDeviceDescriptor:
		if (param) {
			/* Get device descriptor request */
			error = USB_DeviceGetDeviceDescriptor(handle,
					(usb_device_get_device_descriptor_struct_t *) param);
		}
		break;
	case kUSB_DeviceEventGetConfigurationDescriptor:
		if (param) {
			/* Get device configuration descriptor request */
			error = USB_DeviceGetConfigurationDescriptor(handle,
					(usb_device_get_configuration_descriptor_struct_t *) param);
		}
		break;
	case kUSB_DeviceEventGetStringDescriptor:
		if (param) {
			/* Get device string descriptor request */
			error = USB_DeviceGetStringDescriptor(handle,
					(usb_device_get_string_descriptor_struct_t *) param);
		}
		break;
	case kUSB_DeviceEventGetHidDescriptor:
		if (param) {
			/* Get hid descriptor request */
			error = USB_DeviceGetHidDescriptor(handle,
					(usb_device_get_hid_descriptor_struct_t *) param);
		}
		break;
	case kUSB_DeviceEventGetHidReportDescriptor:
		if (param) {
			/* Get hid report descriptor request */
			error = USB_DeviceGetHidReportDescriptor(handle,
					(usb_device_get_hid_report_descriptor_struct_t *) param);
		}
		break;
#if (defined(USB_DEVICE_CONFIG_CV_TEST) && (USB_DEVICE_CONFIG_CV_TEST > 0U))
		case kUSB_DeviceEventGetDeviceQualifierDescriptor:
		if (param)
		{
			/* Get Qualifier descriptor request */
			error = USB_DeviceGetDeviceQualifierDescriptor(
					handle, (usb_device_get_device_qualifier_descriptor_struct_t *)param);
		}
		break;
#endif
	case kUSB_DeviceEventGetHidPhysicalDescriptor:
		if (param) {
			/* Get hid physical descriptor request */
			error = USB_DeviceGetHidPhysicalDescriptor(handle,
					(usb_device_get_hid_physical_descriptor_struct_t *) param);
		}
		break;
	default:
		break;
	}

	return error;
}

/* Application initialization */
static void USB_DeviceApplicationInit(void) {
	USB_DeviceClockInit();
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
	SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

	/* Set composite device to default state */
	g_UsbDeviceComposite.speed = USB_SPEED_FULL;
	g_UsbDeviceComposite.attach = 0U;
	g_UsbDeviceComposite.hidMouseHandle = (class_handle_t) NULL;
	g_UsbDeviceComposite.hidKeyboardHandle = (class_handle_t) NULL;
	g_UsbDeviceComposite.deviceHandle = NULL;

	if (kStatus_USB_Success
			!= USB_DeviceClassInit(CONTROLLER_ID,
					&g_UsbDeviceCompositeConfigList,
					&g_UsbDeviceComposite.deviceHandle)) {
		usb_echo("USB device composite demo init failed\r\n");
		return;
	} else {
		usb_echo("USB device composite demo\r\n");
		/* Get the HID keyboard class handle */
		g_UsbDeviceComposite.hidKeyboardHandle =
				g_UsbDeviceCompositeConfigList.config[0].classHandle;
		/* Get the HID mouse class handle */
		g_UsbDeviceComposite.hidMouseHandle =
				g_UsbDeviceCompositeConfigList.config[1].classHandle;

		USB_DeviceHidKeyboardInit(&g_UsbDeviceComposite);
		USB_DeviceHidMouseInit(&g_UsbDeviceComposite);
	}

	USB_DeviceIsrEnable();

	/* Start the device function */
	USB_DeviceRun(g_UsbDeviceComposite.deviceHandle);
}

#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTask(void *handle)
{
	while (1U)
	{
		USB_DeviceTaskFn(handle);
	}
}
#endif

void APP_task(void *handle) {
	USB_DeviceApplicationInit();

#if USB_DEVICE_CONFIG_USE_TASK
	if (g_UsbDeviceComposite.deviceHandle)
	{
		if (xTaskCreate(USB_DeviceTask, /* pointer to the task */
						"usb device task", /* task name for kernel awareness debugging */
						5000L / sizeof(portSTACK_TYPE), /* task stack size */
						g_UsbDeviceComposite.deviceHandle, /* optional task startup argument */
						5U, /* initial priority */
						&g_UsbDeviceComposite.deviceTaskHandle /* optional task handle to create */
				) != pdPASS)
		{
			usb_echo("usb device task create failed!\r\n");
			return;
		}
	}
#endif

	while (1U) {
	}
}

#if defined(__CC_ARM) || defined(__GNUC__)

/**this are the shared flags between functions used for proper work, don't modify*/
uint8_t centered = 0;//this flag is used to know if the mouse has already been centered,
uint8_t paint_opened = 0;//this flag is used to know if mspaint has already been opened,
uint8_t figure_painted = 0;	//this flag is used to know if the figure has already been painted,
uint8_t notepad_opened = 0;	//this flag is used to know if both instances of notepad have been opened,
uint8_t Alternative_open = 0;//this must not be modified, as it's controlled with the button at the system begin
uint8_t Alternative_text = 0;	//specially this one, left always in 0
uint8_t cut = 0;//this flag is used to know if the text has been written and cut,
uint8_t left_moved = 0;	//this flag is used to know if the mouse has moved to the left notepad,
uint8_t pasted = 0;	//this variable is used to know if the text has been finally pasted

/**this is the IRQ handler for the start button*/
void PORTC_IRQHandler() {
	PORT_ClearPinsInterruptFlags(PORTC, 1 << 6); //SW3 pin interrupt flag clearing
	Alternative_open = 1;//if the button is pressed at system begin, the alternative open is set
}

/**this function initializes the button to be used to enable the alternative opening*/
void Button_configure(void) {

	//port C clock gating enabling
	CLOCK_EnableClock(kCLOCK_PortC);

	//the port initial configuration is set
	port_pin_config_t button_configuration = { kPORT_PullUp, kPORT_SlowSlewRate,
			kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable,
			kPORT_LowDriveStrength, kPORT_MuxAsGpio, kPORT_UnlockRegister };
	PORT_SetPinConfig(PORTC, 6, &button_configuration);

	//configuring as input
	gpio_pin_config_t sw2_config = { kGPIO_DigitalInput, 1, };
	GPIO_PinInit(PORTC, 6, &sw2_config);

	//SW2 pin configuration
	PORT_SetPinInterruptConfig(PORTC, 6, kPORT_InterruptLogicZero); /**B2 configured to interrupt when a logic 0 is read*/
	/**(as the SW2 is active-low)*/
	NVIC_EnableIRQ(PORTC_IRQn);		//NVIC interrupt enable
	NVIC_SetPriority(PORTC_IRQn, 6);	//NVIC priority set to 6
}

int main(void)
#else
void main(void)
#endif
{
	BOARD_InitPins();
	BOARD_BootClockRUN();
	BOARD_InitDebugConsole();

	Button_configure();	//calls the button configuration function

	if (xTaskCreate(APP_task, /* pointer to the task */
	"app task", /* task name for kernel awareness debugging */
	5000L / sizeof(portSTACK_TYPE), /* task stack size */
	&g_UsbDeviceComposite, /* optional task startup argument */
	4U, /* initial priority */
	&g_UsbDeviceComposite.applicationTaskHandle /* optional task handle to create */
	) != pdPASS) {
		usb_echo("app task create failed!\r\n");
#if (defined(__CC_ARM) || defined(__GNUC__))
		return 1U;
#else
		return;
#endif
	}

	vTaskStartScheduler();

#if (defined(__CC_ARM) || defined(__GNUC__))
	return 1U;
#endif
}

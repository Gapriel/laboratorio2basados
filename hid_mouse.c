/*
 * The Clear BSD License
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
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

#include "usb_device_class.h"
#include "usb_device_hid.h"

#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"

#include "composite.h"

#include "hid_mouse.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static usb_status_t USB_DeviceHidMouseAction(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_MouseBuffer[USB_HID_MOUSE_REPORT_LENGTH];
static usb_device_composite_struct_t *s_UsbDeviceComposite;
static usb_device_hid_mouse_struct_t s_UsbDeviceHidMouse;

/*******************************************************************************
 * Code
 ******************************************************************************/
extern uint8_t centered;
extern uint8_t paint_opened;
extern uint8_t figure_painted;
extern uint8_t Alternative_open;
extern uint8_t cut;
extern uint8_t left_moved;

static usb_status_t USB_center_mouse(void){
	static int16_t horizontal_axis = 0U;
	static int16_t vertical_axis = 0U;
	uint16_t Xlimit = 2000;
	uint16_t Ylimit = 500;
	uint16_t Xcenter = 1925;
	uint16_t Ycenter = 450;
	enum{
		RIGHT,
		DOWN,
		LEFT,
		UP
	};
	static uint8_t direction = RIGHT;

	switch(direction){
	case RIGHT:
		if(horizontal_axis < Xlimit){
			s_UsbDeviceHidMouse.buffer[1] = 125U;
			s_UsbDeviceHidMouse.buffer[2] = 0U;
			horizontal_axis += 125;
		}else{
			direction = DOWN;
		}
		break;
	case DOWN:
		if(vertical_axis < Ylimit){
			s_UsbDeviceHidMouse.buffer[1] = 0U;
			s_UsbDeviceHidMouse.buffer[2] = 125U;
			vertical_axis += 125;
		}else{
			direction = LEFT;
		}
		break;
	case LEFT:
		if(horizontal_axis >= Xcenter){
			s_UsbDeviceHidMouse.buffer[1] = (uint8_t)(0xF9);
			s_UsbDeviceHidMouse.buffer[2] = 0U;
			horizontal_axis-=1;
		}else{
			direction = UP;
		}
		break;
	case UP:
		if(vertical_axis >= Ycenter){
			s_UsbDeviceHidMouse.buffer[1] = 0;
			s_UsbDeviceHidMouse.buffer[2] = (uint8_t)(0xF9);
			vertical_axis-=1;
		}else{
			s_UsbDeviceHidMouse.buffer[1] = 0U;
			s_UsbDeviceHidMouse.buffer[2] = 0U;
			centered = 1;
		}
		break;
	}

	 return USB_DeviceHidSend(s_UsbDeviceComposite->hidMouseHandle, USB_HID_MOUSE_ENDPOINT_IN,
			             	  s_UsbDeviceHidMouse.buffer, USB_HID_MOUSE_REPORT_LENGTH);
}

static usb_status_t USB_paintFigure(void){
	static int16_t horizontal_axis = 0U;
	static int16_t vertical_axis = 0U;
	enum{
		RIGHT,
		DOWN,
		LEFT,
		UP
	};
	static uint8_t direction = RIGHT;

	s_UsbDeviceHidMouse.buffer[0] = 0x01;
	USB_DeviceHidSend(s_UsbDeviceComposite->hidMouseHandle, USB_HID_MOUSE_ENDPOINT_IN,
		              s_UsbDeviceHidMouse.buffer, USB_HID_MOUSE_REPORT_LENGTH);

	switch (direction)
	{
		case RIGHT:
			/* Move right. Increase X value. */
			s_UsbDeviceHidMouse.buffer[1] = 1U;
			s_UsbDeviceHidMouse.buffer[2] = 0U;
			horizontal_axis++;
			if (horizontal_axis > 99U)
			{
				direction++;
			}
	    break;
	    case DOWN:
			/* Move down. Increase Y value. */
			s_UsbDeviceHidMouse.buffer[1] = 0U;
			s_UsbDeviceHidMouse.buffer[2] = 1U;
			vertical_axis++;
			if (vertical_axis > 99U)
			{
				direction++;
			}
	     break;
	     case LEFT:
	        /* Move left. Discrease X value. */
	        s_UsbDeviceHidMouse.buffer[1] = (uint8_t)(0xFFU);
	        s_UsbDeviceHidMouse.buffer[2] = 0U;
	        horizontal_axis--;
	        if (horizontal_axis < 1U)
	        {
	        	direction++;
	        }
	     break;
	     case UP:
			 /* Move up. Discrease Y value. */
			 s_UsbDeviceHidMouse.buffer[1] = 0U;
			 s_UsbDeviceHidMouse.buffer[2] = (uint8_t)(0xFFU);
			 vertical_axis--;
			 if (vertical_axis < 1U)
			 {
				 figure_painted = 1;
				 s_UsbDeviceHidMouse.buffer[0] = 0x00;
			 }
	     break;
	     default:
	     break;
	 }
	 return USB_DeviceHidSend(s_UsbDeviceComposite->hidMouseHandle, USB_HID_MOUSE_ENDPOINT_IN,
	                          s_UsbDeviceHidMouse.buffer, USB_HID_MOUSE_REPORT_LENGTH);
}

static usb_status_t USB_moveLeft(void){
	static uint8_t mouse_leftMovement_index = 0;
	static uint8_t horizontal_axis = 0;

	switch(mouse_leftMovement_index){
	case 0:
		s_UsbDeviceHidMouse.buffer[1] = (uint8_t)(0xF9);
		s_UsbDeviceHidMouse.buffer[2] = 0U;
		horizontal_axis++;
		if(horizontal_axis >= 20){
			mouse_leftMovement_index++;
		}
		break;
	case 1:
		s_UsbDeviceHidMouse.buffer[0] = 1U;
		s_UsbDeviceHidMouse.buffer[1] = 0U;
		s_UsbDeviceHidMouse.buffer[2] = 0U;
		mouse_leftMovement_index++;
		break;
	case 2:
		s_UsbDeviceHidMouse.buffer[0] = 0U;
		s_UsbDeviceHidMouse.buffer[1] = 0U;
		s_UsbDeviceHidMouse.buffer[2] = 0U;
		left_moved = 1;
		break;
	}

	return USB_DeviceHidSend(s_UsbDeviceComposite->hidMouseHandle, USB_HID_MOUSE_ENDPOINT_IN,
            s_UsbDeviceHidMouse.buffer, USB_HID_MOUSE_REPORT_LENGTH);
}

/* Update mouse pointer location. Draw a rectangular rotation*/
static usb_status_t USB_DeviceHidMouseAction(void)
{
    if(0 == centered){
    	return USB_center_mouse();
	}
	if (0 == Alternative_open){
		if((0 == figure_painted) && (1 == paint_opened)){
			return USB_paintFigure();
		}
	}
	if( (1 == cut) && (0 == left_moved) ){
		return USB_moveLeft();
	}

    s_UsbDeviceHidMouse.buffer[0] = 0x00;
    s_UsbDeviceHidMouse.buffer[1] = 0x00;
    s_UsbDeviceHidMouse.buffer[2] = 0x00;
    return USB_DeviceHidSend(s_UsbDeviceComposite->hidMouseHandle, USB_HID_MOUSE_ENDPOINT_IN,
    		                 s_UsbDeviceHidMouse.buffer, USB_HID_MOUSE_REPORT_LENGTH);
}

/* The device HID class callback */
usb_status_t USB_DeviceHidMouseCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Error;

    switch (event)
    {
        case kUSB_DeviceHidEventSendResponse:
            if (s_UsbDeviceComposite->attach)
            {
                return USB_DeviceHidMouseAction();
            	//return USB_center_mouse();
            }
            break;
        case kUSB_DeviceHidEventGetReport:
        case kUSB_DeviceHidEventSetReport:
        case kUSB_DeviceHidEventRequestReportBuffer:
            error = kStatus_USB_InvalidRequest;
            break;
        case kUSB_DeviceHidEventGetIdle:
        case kUSB_DeviceHidEventGetProtocol:
        case kUSB_DeviceHidEventSetIdle:
        case kUSB_DeviceHidEventSetProtocol:
            break;
        default:
            break;
    }

    return error;
}

/* The device callback */
usb_status_t USB_DeviceHidMouseSetConfigure(class_handle_t handle, uint8_t configure)
{
    if (USB_COMPOSITE_CONFIGURE_INDEX == configure)
    {
        return USB_DeviceHidMouseAction(); /* run the cursor movement code */
    }
    return kStatus_USB_Error;
}

/* Set interface */
usb_status_t USB_DeviceHidMouseSetInterface(class_handle_t handle, uint8_t interface, uint8_t alternateSetting)
{
    if (USB_HID_KEYBOARD_INTERFACE_INDEX == interface)
    {
        return USB_DeviceHidMouseAction(); /* run the cursor movement code */
    }
    return kStatus_USB_Error;
}

/* Initialize the HID mouse */
usb_status_t USB_DeviceHidMouseInit(usb_device_composite_struct_t *deviceComposite)
{
    s_UsbDeviceComposite = deviceComposite;
    s_UsbDeviceHidMouse.buffer = s_MouseBuffer;
    return kStatus_USB_Success;
}

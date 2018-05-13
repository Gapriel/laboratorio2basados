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

#include "hid_keyboard.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static usb_status_t USB_DeviceHidKeyboardAction(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_KeyboardBuffer[USB_HID_KEYBOARD_REPORT_LENGTH];
static usb_device_composite_struct_t *s_UsbDeviceComposite;
static usb_device_hid_keyboard_struct_t s_UsbDeviceHidKeyboard;

/*******************************************************************************
 * Code
 ******************************************************************************/
extern uint8_t centered;
extern uint8_t paint_opened;
extern uint8_t figure_painted;
extern uint8_t notepad_opened;
extern uint8_t Alternative_open;

static usb_status_t USB_openPaint(void){

	static uint8_t openPaint_state = 0;
	static uint8_t wait = 0;
	static uint8_t program_name_length;
	static uint8_t program_to_be_opened[10];
	if(1 == Alternative_open){
		program_to_be_opened[0] = KEY_C;
		program_to_be_opened[1] = KEY_H;
		program_to_be_opened[2] = KEY_R;
		program_to_be_opened[3] = KEY_O;
		program_to_be_opened[4] = KEY_M;
		program_to_be_opened[5] = KEY_E;
		program_to_be_opened[6] = KEY_ENTER;
		program_name_length = 7;
	}else{
		program_to_be_opened[0] = KEY_M;
		program_to_be_opened[1] = KEY_S;
		program_to_be_opened[2] = KEY_P;
		program_to_be_opened[3] = KEY_A;
		program_to_be_opened[4] = KEY_I;
		program_to_be_opened[5] = KEY_N;
		program_to_be_opened[6] = KEY_T;
		program_to_be_opened[7] = KEY_ENTER;
		program_name_length = 8;
	}
	static uint8_t program_to_be_opened_index = 0;

	switch(openPaint_state){
		case 0:
			wait++;
			if(wait > 200){
				s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
				s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
				openPaint_state++;
				wait = 0;
			}
			break;
		case 1:
			wait++;
			if(wait > 200){
				s_UsbDeviceHidKeyboard.buffer[2] = KEY_RIGHT_GUI;
				s_UsbDeviceHidKeyboard.buffer[3] = KEY_R;
				openPaint_state++;
				wait = 0;
			}
			break;
		case 2:
			wait++;
			if(wait > 200){
				s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
				s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
				openPaint_state++;
				wait = 0;
			}
			break;
		case 3:
			wait++;
			if((wait > 10))
			{
				s_UsbDeviceHidKeyboard.buffer[2] = program_to_be_opened[program_to_be_opened_index];
				program_to_be_opened_index++;
				wait = 0;
			}
			if(program_name_length < program_to_be_opened_index)
			{
				s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
				s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
				openPaint_state++;
			}
			break;
		case 4:
			wait++;
			if(wait > 200){
				s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
				s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
				uint32_t count = 0;
					while(15000000 > count){
						count++;
					};
					paint_opened = 1;
			}
			break;
	}

	return USB_DeviceHidSend(s_UsbDeviceComposite->hidKeyboardHandle, USB_HID_KEYBOARD_ENDPOINT_IN,
            				 s_UsbDeviceHidKeyboard.buffer, USB_HID_KEYBOARD_REPORT_LENGTH);
}

static usb_status_t USB_openNotepad(void) {

	static uint8_t openNotepad_state = 0;
	static uint8_t wait = 0;
	static uint8_t program_to_be_opened[] = {KEY_N,KEY_O,KEY_T,KEY_E,KEY_P,KEY_A,KEY_D,KEY_ENTER};
	static uint8_t program_name_length = 8;
	static uint8_t program_to_be_opened_index = 0;

	switch (openNotepad_state) {
	case 0:
		wait++;
		if (wait > 200) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
			s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 1:
		wait++;
		if (wait > 200) {
			s_UsbDeviceHidKeyboard.buffer[2] = KEY_RIGHT_GUI;
			s_UsbDeviceHidKeyboard.buffer[3] = KEY_R;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 2:
		wait++;
		if (wait > 200) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
			s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 3:
		wait++;
		if ((wait > 10)) {
			s_UsbDeviceHidKeyboard.buffer[2] =
					program_to_be_opened[program_to_be_opened_index];
			program_to_be_opened_index++;
			wait = 0;
		}
		if (program_name_length < program_to_be_opened_index) {
			    s_UsbDeviceHidKeyboard.buffer[2] = 0;
			    s_UsbDeviceHidKeyboard.buffer[3] = 0;
				openNotepad_state = 5;
				program_to_be_opened_index = 0;
				wait = 0;
		}
		break;
	case 5:
		wait++;
		if(wait > 200){
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 6:
		wait++;
		if (wait > 100) {
			s_UsbDeviceHidKeyboard.buffer[2] = KEY_RIGHT_GUI;
			s_UsbDeviceHidKeyboard.buffer[3] = KEY_KEYPAD_4_LEFT_ARROW;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 7:
		wait++;
		if(wait > 200){
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 8:
		wait++;
		if(wait > 200){
			s_UsbDeviceHidKeyboard.buffer[2] = KEY_RIGHT_GUI;
			s_UsbDeviceHidKeyboard.buffer[3] = KEY_R;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 9:
		wait++;
		if(wait > 200){
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 10:
		wait++;
		if ((wait > 10)) {
			s_UsbDeviceHidKeyboard.buffer[2] =
			program_to_be_opened[program_to_be_opened_index];
			program_to_be_opened_index++;
			wait = 0;
		}
		if (program_name_length < program_to_be_opened_index) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			openNotepad_state = 11;
			program_to_be_opened_index = 0;
			wait = 0;
		}
	    break;
	case 11:
		wait++;
		if((wait > 200)){
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 12:
		wait++;
		if (wait > 100) {
			s_UsbDeviceHidKeyboard.buffer[2] = KEY_RIGHT_GUI;
			s_UsbDeviceHidKeyboard.buffer[3] = KEY_KEYPAD_6_RIGHT_ARROW;
			openNotepad_state++;
			wait = 0;
		}
		break;
	case 13:
		wait++;
		if(wait > 200){
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			wait = 0;
			notepad_opened = 1;
		}
		break;
	}

	return USB_DeviceHidSend(s_UsbDeviceComposite->hidKeyboardHandle,
					USB_HID_KEYBOARD_ENDPOINT_IN, s_UsbDeviceHidKeyboard.buffer,
					USB_HID_KEYBOARD_REPORT_LENGTH);
}

static usb_status_t USB_openWebsite(void) {
	//static uint8_t website_to_be_opened[] = { KEY_B, KEY_B, KEY_A, KEY_A,
	//		KEY_DOT_GREATER, KEY_C, KEY_O, KEY_M, KEY_ENTER };
	//static uint8_t website_name_length = 9;
	static uint8_t website_to_be_opened[] = {KEY_Y,KEY_O,KEY_U,KEY_T,KEY_U,KEY_B,KEY_E,KEY_DOT_GREATER,KEY_C,KEY_O,KEY_M,KEY_ENTER};
	static uint8_t website_name_length = 12;
	static uint8_t website_to_be_opened_index = 0;

	static uint8_t wait = 0;
	static uint8_t website_state = 0;

	switch (website_state) {
	case 0:
		wait++;
		if (wait < 200) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
			s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
			website_state++;
			wait = 0;
		}
		break;
	case 1:
		wait++;
		if ((wait > 65)) {
			s_UsbDeviceHidKeyboard.buffer[2] =
					website_to_be_opened[website_to_be_opened_index];
			website_to_be_opened_index++;
			wait = 0;
		}
		if (website_name_length < website_to_be_opened_index) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0;
			s_UsbDeviceHidKeyboard.buffer[3] = 0;
			website_to_be_opened_index = 0;
			website_state++;
			wait = 0;
		}
		break;
	case 2:
		wait++;
		if (wait < 200) {
			s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
			s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
			website_state++;
			wait = 0;
		}
		break;
	}

	return USB_DeviceHidSend(s_UsbDeviceComposite->hidKeyboardHandle,
			USB_HID_KEYBOARD_ENDPOINT_IN, s_UsbDeviceHidKeyboard.buffer,
			USB_HID_KEYBOARD_REPORT_LENGTH);
}

static usb_status_t USB_DeviceHidKeyboardAction(void)
{
    if((0 == paint_opened) && (1 == centered)){
    	return USB_openPaint();
    }
	if (0 == Alternative_open){
		if( (1 == figure_painted) && (0 == notepad_opened) ){
			return USB_openNotepad();
		}
	}else if (1 == Alternative_open){
		if(1 == paint_opened){
			return USB_openWebsite();
		}
	}


    s_UsbDeviceHidKeyboard.buffer[2] = 0x00U;
    s_UsbDeviceHidKeyboard.buffer[3] = 0x00U;
    return USB_DeviceHidSend(s_UsbDeviceComposite->hidKeyboardHandle, USB_HID_KEYBOARD_ENDPOINT_IN,
                				 s_UsbDeviceHidKeyboard.buffer, USB_HID_KEYBOARD_REPORT_LENGTH);
}

usb_status_t USB_DeviceHidKeyboardCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Error;

    switch (event)
    {
        case kUSB_DeviceHidEventSendResponse:
            if (s_UsbDeviceComposite->attach)
            {
                return USB_DeviceHidKeyboardAction();
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

usb_status_t USB_DeviceHidKeyboardSetConfigure(class_handle_t handle, uint8_t configure)
{
    if (USB_COMPOSITE_CONFIGURE_INDEX == configure)
    {
        return USB_DeviceHidKeyboardAction(); /* run the cursor movement code */
    }
    return kStatus_USB_Error;
}

usb_status_t USB_DeviceHidKeyboardSetInterface(class_handle_t handle, uint8_t interface, uint8_t alternateSetting)
{
    if (USB_HID_KEYBOARD_INTERFACE_INDEX == interface)
    {
        return USB_DeviceHidKeyboardAction(); /* run the cursor movement code */
    }
    return kStatus_USB_Error;
}

usb_status_t USB_DeviceHidKeyboardInit(usb_device_composite_struct_t *deviceComposite)
{
    s_UsbDeviceComposite = deviceComposite;
    s_UsbDeviceHidKeyboard.buffer = s_KeyboardBuffer;
    return kStatus_USB_Success;
}

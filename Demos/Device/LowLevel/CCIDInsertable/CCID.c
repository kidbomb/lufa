/*
             LUFA Library
     Copyright (C) Dean Camera, 2018.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2018  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2018  Filipe Rodrigues (filipepazrodrigues [at] gmail [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the CCID demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 *
 *  \warning
 *  LUFA is not a secure USB stack, and has not undergone, not is it expected to pass, any
 *  form of security audit. The CCID class here is presented as-is and is intended for
 *  research purposes only, and *should not* be used in a security critical application
 *  under any circumstances.
 *
 *  \warning
 *  This code is not production ready and should not by any means be considered safe.
 *  If you plan to integrate it into your application, you should seriously consider strong
 *  encryption algorithms or a secure microprocessor. Since Atmel AVR microprocessors do not
 *  have any security requirement (therefore they don't offer any known protection against
 *  side channel attacks or fault injection) a secure microprocessor is the best option.
 */

#define  INCLUDE_FROM_CCID_C
#include "CCID.h"

bool Aborted;
uint8_t AbortedSeq;

bool IsCardPresent;
bool CardStateChanged;

uint8_t BlockBuffer[0x20];


/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
	SetupHardware();

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	GlobalInterruptEnable();

	CCID_Initialize();

	for (;;)
	{
		USB_USBTask();
		CCID_Task();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
	LEDs_Init();
	USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup CCID Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CCID_IN_EPADDR,  EP_TYPE_BULK, CCID_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CCID_OUT_EPADDR, EP_TYPE_BULK, CCID_EPSIZE, 1);

	ConfigSuccess &= Endpoint_ConfigureEndpoint(CCID_INTERRUPT_EPADDR, EP_TYPE_INTERRUPT, CCID_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	switch (USB_ControlRequest.bRequest)
	{
		case CCID_ABORT:
		{
			// initiates the abort process
			// the host should send 2 messages in the following order:
			//  - CCID_ABORT control request
			//  - CCID_PC_t_PCo_RDR_Abort command
			//
			// If the device is still processing a mesage, it should fail it until receiving a CCIRPC_to_RDR_Abort
			// command
			//
			// When the device receives the CCIRPC_to_RDR_Abort message, it replies with  RDR_to_PC_SlotStatus 
			//and the abort process ends
			
			// The  wValue  field  contains the slot number (bSlot) in the low byte and the sequence number (bSeq) in 
			// the high  byte
			uint8_t Slot = USB_ControlRequest.wValue & 0xFF;
			uint8_t Seq = USB_ControlRequest.wValue  >> 8;

			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE) && Slot == 0)
			{

				Endpoint_ClearSETUP();

				Aborted = true;
				AbortedSeq = Seq;

				Endpoint_ClearOUT();
			}

			break;
		}
		case CCID_GET_CLOCK_FREQUENCIES:
		{
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{

				Endpoint_ClearSETUP();
				Endpoint_Write_8(0); //not supported
				Endpoint_ClearOUT();
			}
			break;
		}
		case CCID_GET_DATA_RATES:
		{
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{

				Endpoint_ClearSETUP();
				Endpoint_Write_8(0); //not supported
				Endpoint_ClearOUT();
			}
			break;
		}
	}
}

/** Event handler for the CCID_PC_to_RDR_IccPowerOn message. This message is sent to the device
 *  whenever an application at the host wants to send a power off signal to a slot.
 *  THe slot must reply back with a recognizable ATR (answer to reset) 
 */
uint8_t CCID_IccPowerOn(uint8_t slot,
						 uint8_t *atr,
						 uint8_t * atrLength,
                         uint8_t *error)
{
	if (slot == 0)
	{
		Iso7816_CreateSimpleAtr(atr, atrLength);

		*error = CCID_ERROR_NO_ERROR;
		return CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;
	}
	else
	{
		*error = CCID_ERROR_SLOT_NOT_FOUND;
		return CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_NOICCPRESENT;
	}
}


/** Event handler for the CCID_PC_to_RDR_IccPowerOff message. This message is sent to the device
 *  whenever an application at the host wants to send a power off signal to a slot.
 */
uint8_t CCID_IccPowerOff(uint8_t slot,
                         uint8_t *error)
{
	if (slot == 0)
	{
		*error = CCID_ERROR_NO_ERROR;
		return CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_NOICCPRESENT;
	}
	else
	{
		*error = CCID_ERROR_SLOT_NOT_FOUND;
		return CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_NOICCPRESENT;
	}
}

/** Event handler for the CCID_PC_to_RDR_GetSlotStatus. THis message is sent to the device
 *  whenever an application at the host wants to the get the current slot status
 *
 */
uint8_t CCID_GetSlotStatus(uint8_t slot,
                           uint8_t *error)
{
	if (slot == 0)
	{
		*error = CCID_ERROR_NO_ERROR;
		return CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;
	}
	else
	{
		*error = CCID_ERROR_SLOT_NOT_FOUND;
		return CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_NOICCPRESENT;
	}
}

/** Event handler for the CCID_PC_to_RDR_ABort message. This message is sent to the device
 *  whenever an application wants to abort the current operation.
 * A previous CCID_ABORT control message has to be sent before this one in order to
 * start the abort operation
 */
uint8_t CCID_Abort(uint8_t slot,
                         uint8_t seq,
                         uint8_t *error)
{
	if(Aborted && slot == 0 && AbortedSeq == seq) {
		Aborted = false;
		AbortedSeq = -1;
		*error = CCID_ERROR_NO_ERROR;
		return CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;
	} else if(!Aborted) {
		*error = CCID_ERROR_CMD_NOT_ABORTED;
		return CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;                  
	} else if(slot != 0){
		*error = CCID_ERROR_SLOT_NOT_FOUND;
		return CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_NOICCPRESENT;
	} else {
		*error = CCID_ERROR_NOT_SUPPORTED;
		return CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_NOICCPRESENT;
	}
}

/** Gets and status and verifies whether an error ocurred
 *
 */
bool CCID_CheckStatusNoError(int status)
{
	return (status && 0xC0) == 0x0;
}

uint8_t CCID_GetSlotICCState(uint8_t slot, bool IsCardPresent)
{
	uint8_t SlotICCState = (1 << 1) | (IsCardPresent << 0)
	return (SlotICCState << (slot * 2));
}

void CCID_Initialize(void)
{
	Aborted = false;
	AbortedSeq = -1;

	IsCardPresent = false;
	CardStateChanged = false;
}

void CCID_ChangeCardState()
{
	CardStateChanged = true;
	IsCardPresent = !IsCardPresent;
}

/** Main task for USB loop
 *
 */
void CCID_Task(void)
{
	Endpoint_SelectEndpoint(CCID_INTERRUPT_EPADDR);

	if(CardStateChanged && Endpoint_IsReadWriteAllowed())
	{
		CardStateChanged = false;	

		USB_CCID_RDR_to_PC_NotifySlotChange_t* Notification = (USB_CCID_RDR_to_PC_NotifySlotChange_t) &BlockBuffer;
		Notification->MessageType = CCID_RDR_to_PC_NotifySlotChange;

		Notification->SlotICCState[0] = CCID_GetSlotICCState(0, IsCardPresent);

		Endpoint_Write_Stream_LE(Notification, sizeof(USB_CCID_RDR_to_PC_NotifySlotChange_t) + sizeof(uint8_t), NULL);
		Endpoint_ClearIN();
	}
	else if (Endpoint_IsOUTReceived())
	{
		Endpoint_SelectEndpoint(CCID_OUT_EPADDR);

		USB_CCID_BulkMessage_Header_t CCIDHeader;
		CCIDHeader.MessageType = Endpoint_Read_8();
		CCIDHeader.Length      = Endpoint_Read_32_LE();
		CCIDHeader.Slot        = Endpoint_Read_8();
		CCIDHeader.Seq         = Endpoint_Read_8();

		uint8_t Status;
		uint8_t Error = CCID_ERROR_NO_ERROR;

		switch (CCIDHeader.MessageType)
		{
			case CCID_PC_to_RDR_IccPowerOn:
			{
				uint8_t  AtrLength;
				USB_CCID_RDR_to_PC_DataBlock_t* ResponseATR = (USB_CCID_RDR_to_PC_DataBlock_t*)&BlockBuffer;

				ResponseATR->CCIDHeader.MessageType = CCID_RDR_to_PC_DataBlock;
				ResponseATR->CCIDHeader.Slot        = CCIDHeader.Slot;
				ResponseATR->CCIDHeader.Seq         = CCIDHeader.Seq;
				ResponseATR->ChainParam             = 0;

				Status = CCID_IccPowerOn(ResponseATR->CCIDHeader.Slot, (uint8_t *)ResponseATR->Data, &AtrLength, &Error);

				if (CCID_CheckStatusNoError(Status) && !Aborted)
				{
					ResponseATR->CCIDHeader.Length = AtrLength;
				}
				else if(Aborted)
				{
					Status = CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_PRESENTANDACTIVE;
					Error =  CCID_ERROR_CMD_ABORTED;
					AtrLength = 0;
				}
				else
				{
					AtrLength = 0;
				}

				ResponseATR->Status = Status;
				ResponseATR->Error  = Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(ResponseATR, sizeof(USB_CCID_RDR_to_PC_DataBlock_t) + AtrLength, NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_IccPowerOff:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponsePowerOff =  (USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponsePowerOff->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponsePowerOff->CCIDHeader.Length      = 0;
				ResponsePowerOff->CCIDHeader.Slot        = CCIDHeader.Slot;
				ResponsePowerOff->CCIDHeader.Seq         = CCIDHeader.Seq;

				ResponsePowerOff->ClockStatus = 0;

				Status = CCID_IccPowerOff(CCIDHeader.Slot, &Error);

				ResponsePowerOff->Status = Status;
				ResponsePowerOff->Error  = Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(ResponsePowerOff, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_GetSlotStatus:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponseSlotStatus = (USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponseSlotStatus->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponseSlotStatus->CCIDHeader.Length      = 0;
				ResponseSlotStatus->CCIDHeader.Slot        = CCIDHeader.Slot;
				ResponseSlotStatus->CCIDHeader.Seq         = CCIDHeader.Seq;

				ResponseSlotStatus->ClockStatus = 0;

				Status = CCID_GetSlotStatus(CCIDHeader.Slot, &Error);

				ResponseSlotStatus->Status = Status;
				ResponseSlotStatus->Error  = Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(ResponseSlotStatus, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_XfrBlock:
			{
				uint8_t  Bwi            = Endpoint_Read_8();
				uint16_t LevelParameter = Endpoint_Read_16_LE();
				uint8_t  ReceivedBuffer[0x4];

				(void)Bwi;
				(void)LevelParameter;

				Endpoint_Read_Stream_LE(ReceivedBuffer, sizeof(ReceivedBuffer), NULL);

				uint8_t  SendBuffer[0x2] = {0x90, 0x00};
				uint8_t  SendLength      = sizeof(SendBuffer);

				USB_CCID_RDR_to_PC_DataBlock_t* ResponseBlock = (USB_CCID_RDR_to_PC_DataBlock_t*)&BlockBuffer;
				ResponseBlock->CCIDHeader.MessageType = CCID_RDR_to_PC_DataBlock;
				ResponseBlock->CCIDHeader.Slot        = CCIDHeader.Slot;
				ResponseBlock->CCIDHeader.Seq         = CCIDHeader.Seq;

				ResponseBlock->ChainParam = 0;

				//TODO: callback
				Status = CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;

				if (CCID_CheckStatusNoError(Status) && !Aborted)
				{
					ResponseBlock->CCIDHeader.Length = SendLength;
					memcpy(&ResponseBlock->Data, SendBuffer, SendLength);
				}
				else if(Aborted)
				{
					Status = CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_PRESENTANDACTIVE;
					Error =  CCID_ERROR_CMD_ABORTED;
					SendLength = 0;
				}
				else
				{
					SendLength = 0;
				}

				ResponseBlock->Status = Status;
				ResponseBlock->Error  = Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(ResponseBlock, sizeof(USB_CCID_RDR_to_PC_DataBlock_t) + SendLength, NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_Abort:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponseAbort =  (USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponseAbort->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponseAbort->CCIDHeader.Length      = 0;
				ResponseAbort->CCIDHeader.Slot        = CCIDHeader.Slot;
				ResponseAbort->CCIDHeader.Seq         = CCIDHeader.Seq;

				ResponseAbort->ClockStatus = 0;

				Status = CCID_Abort(CCIDHeader.Slot, CCIDHeader.Seq, &Error);

				ResponseAbort->Status = Status;
				ResponseAbort->Error  = Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(ResponseAbort, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}
			default:
			{
				memset(BlockBuffer, 0x00, sizeof(BlockBuffer));

				Endpoint_SelectEndpoint(CCID_IN_EPADDR);
				Endpoint_Write_Stream_LE(BlockBuffer, sizeof(BlockBuffer), NULL);
				Endpoint_ClearIN();
			}
		}
	}
}

/*
						 LUFA Library
		 Copyright (C) Dean Camera, 2018.

	dean [at] fourwalledcubicle [dot] com
					 www.lufa-lib.org
*/

/*
	Copyright 2018	Dean Camera (dean [at] fourwalledcubicle [dot] com)

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
	and fitness.	In no event shall the author be liable for any
	special, indirect or consequential damages or any damages
	whatsoever resulting from loss of use, data or profits, whether
	in an action of contract, negligence or other tortious action,
	arising out of or in connection with the use or performance of
	this software.
*/

#define	__INCLUDE_FROM_USB_DRIVER
#include "../../Core/USBMode.h"

#if defined(USB_CAN_BE_DEVICE)

#define	__INCLUDE_FROM_CCID_DRIVER
#define	__INCLUDE_FROM_CCID_DEVICE_C
#include "CCIDClassDevice.h"

uint8_t BlockBuffer[0x20];

bool CCID_CheckStatusNoError(int status) {
	return (status && 0xC0) == 0x0;
}

uint8_t CCID_GetSlotICCState(uint8_t slot, bool IsCardPresent)
{
	uint8_t SlotICCState = (1 << 1) | (IsCardPresent << 0);
	return (SlotICCState << (slot * 2));
}

uint8_t CCID_SetSlotICCState(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo, uint8_t slot, bool IsCardPresent)
{
	uint8_t index = (slot/4) -1;
	CCIDInterfaceInfo->Status.SlotICC[index] = CCID_GetSlotICCState(slot, IsCardPresent);
}

bool CCID_Device_Supports_Interrupt(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo)
{
	return (CCIDInterfaceInfo->Config.InterruptInEndpoint.Address > 0) 
	&& (CCIDInterfaceInfo->Config.InterruptInEndpoint.Size > 0) 
	&& (CCIDInterfaceInfo->Config.InterruptInEndpoint.Banks > 0);
}

void CCID_ChangeCardState()
{
	CardStateChanged = true;
	IsCardPresent = !IsCardPresent;
}

void CCID_Device_ProcessControlRequest(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo)
{
	if (!(Endpoint_IsSETUPReceived()))
		return;

	if (USB_ControlRequest.wIndex != CCIDInterfaceInfo->Config.InterfaceNumber)
		return;

	switch (USB_ControlRequest.bRequest)
	{
		case CCID_ABORT:
		{
			// initiates the abort process
			// the host should send 2 messages in the following order:
			//	- CCID_ABORT control request
			//	- CCID_PC_t_PCo_RDR_Abort command
			//
			// If the device is still processing a mesage, it should fail it until receiving a CCIRPC_to_RDR_Abort
			// command
			//
			// When the device receives the CCIRPC_to_RDR_Abort message, it replies with	RDR_to_PC_SlotStatus 
			//and the abort process ends
			
			// The	wValue	field	contains the slot number (bSlot) in the low byte and the sequence number (bSeq) in 
			// the high	byte
			uint8_t Slot = USB_ControlRequest.wValue & 0xFF;
			uint8_t Seq = USB_ControlRequest.wValue	>> 8;

			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE) && Slot == 0)
			{

				Endpoint_ClearSETUP();

				CCIDInterfaceInfo->State.Aborted = true;
				CCIDInterfaceInfo->State.AbortedSeq = Seq;

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

bool CCID_Device_ConfigureEndpoints(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo)
{

	CCIDInterfaceInfo->Config.DataINEndpoint.Type	= EP_TYPE_BULK;
	CCIDInterfaceInfo->Config.DataOUTEndpoint.Type = EP_TYPE_BULK;

	if(CCID_Device_Supports_Interrupt(CCIDInterfaceInfo))
	{
		CCIDInterfaceInfo->Config.InterruptInEndpoint.Type = EP_TYPE_INTERRUPT;
	}


	if (!(Endpoint_ConfigureEndpointTable(&CCIDInterfaceInfo->Config.DataINEndpoint, 1)))
		return false;

	if (!(Endpoint_ConfigureEndpointTable(&CCIDInterfaceInfo->Config.DataOUTEndpoint, 1)))
		return false;

	if(CCID_Device_Supports_Interrupt(CCIDInterfaceInfo) && !(Endpoint_ConfigureEndpointTable(&CCIDInterfaceInfo->Config.InterruptInEndpoint, 1)))
		return false;

	return true;
}

void CCID_Device_Initialize(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo)
{
	//or memset to zero
	CCIDInterfaceInfo->State.Aborted = false;
	CCIDInterfaceInfo->State.AbortedSeq = -1;

	for(uint8_t i = 0; i < CCIDInterfaceInfo->Config.TotalSlots / 4; i ++)
	{
		CCIDInterfaceInfo->State.SlotICC[i] = 0; 
	}
}

void CCID_Device_USBTask(USB_ClassInfo_CCID_Device_t* const CCIDInterfaceInfo) {

	if(CCID_Device_Supports_Interrupt(CCIDInterfaceInfo)) {
		
		Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.InterruptInEndpoint.Address);

		if(CardStateChanged && Endpoint_IsReadWriteAllowed())
		{
			CardStateChanged = false;	

			USB_CCID_RDR_to_PC_NotifySlotChange_t* Notification = (USB_CCID_RDR_to_PC_NotifySlotChange_t) &BlockBuffer;
			Notification->MessageType = CCID_RDR_to_PC_NotifySlotChange;

			for(uint8_t i = 0; i < CCIDInterfaceInfo->Config.TotalSlots / 4; i ++) //each 2 bits is a slot
			{
				Notification->SlotICCState[i] = CCIDInterfaceInfo->State.SlotICC[i]; 
			}

			Endpoint_Write_Stream_LE(Notification, sizeof(USB_CCID_RDR_to_PC_NotifySlotChange_t) + sizeof(uint8_t), NULL);
			Endpoint_ClearIN();
		}
	}
	else if (Endpoint_IsOUTReceived())
	{

		Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataOUTEndpoint.Address);

		USB_CCID_BulkMessage_Header_t CCIDHeader;
		CCIDHeader.MessageType = Endpoint_Read_8();
		CCIDHeader.Length			= Endpoint_Read_32_LE();
		CCIDHeader.Slot				= Endpoint_Read_8();
		CCIDHeader.Seq				 = Endpoint_Read_8();

		uint8_t Status;
		uint8_t Error = CCID_ERROR_NO_ERROR;

		switch (CCIDHeader.MessageType)
		{
			case CCID_PC_to_RDR_IccPowerOn:
			{
				uint8_t AtrLength;
				USB_CCID_RDR_to_PC_DataBlock_t* ResponseATR = (USB_CCID_RDR_to_PC_DataBlock_t*)&BlockBuffer;

				ResponseATR->CCIDHeader.MessageType = CCID_RDR_to_PC_DataBlock;
				ResponseATR->CCIDHeader.Slot				= CCIDHeader.Slot;
				ResponseATR->CCIDHeader.Seq				 = CCIDHeader.Seq;
				ResponseATR->ChainParam						 = 0;

				Status = CALLBACK_CCID_IccPowerOn(ResponseATR->CCIDHeader.Slot, (uint8_t *)ResponseATR->Data, &AtrLength, &Error);

				if (CCID_CheckStatusNoError(Status) && !CCIDInterfaceInfo->State.Aborted)
				{
					ResponseATR->CCIDHeader.Length = AtrLength;
				}
				else if(CCIDInterfaceInfo->State.Aborted)
				{
					Status = CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_PRESENTANDACTIVE;
					Error =	CCID_ERROR_CMD_ABORTED;
					AtrLength = 0;
				}
				else
				{
					AtrLength = 0;
				}

				ResponseATR->Status = Status;
				ResponseATR->Error	= Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(ResponseATR, sizeof(USB_CCID_RDR_to_PC_DataBlock_t) + AtrLength, NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_IccPowerOff:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponsePowerOff =	(USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponsePowerOff->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponsePowerOff->CCIDHeader.Length			= 0;
				ResponsePowerOff->CCIDHeader.Slot				= CCIDHeader.Slot;
				ResponsePowerOff->CCIDHeader.Seq				 = CCIDHeader.Seq;

				ResponsePowerOff->ClockStatus = 0;

				Status = CALLBACK_CCID_IccPowerOff(CCIDHeader.Slot, &Error);

				ResponsePowerOff->Status = Status;
				ResponsePowerOff->Error	= Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(ResponsePowerOff, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_GetSlotStatus:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponseSlotStatus = (USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponseSlotStatus->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponseSlotStatus->CCIDHeader.Length			= 0;
				ResponseSlotStatus->CCIDHeader.Slot				= CCIDHeader.Slot;
				ResponseSlotStatus->CCIDHeader.Seq				 = CCIDHeader.Seq;

				ResponseSlotStatus->ClockStatus = 0;

				Status = CALLBACK_CCID_GetSlotStatus(CCIDHeader.Slot, &Error);

				ResponseSlotStatus->Status = Status;
				ResponseSlotStatus->Error	= Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(ResponseSlotStatus, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_XfrBlock:
			{
				uint8_t	Bwi						= Endpoint_Read_8();
				uint16_t LevelParameter = Endpoint_Read_16_LE();
				uint8_t	ReceivedBuffer[0x4];

				(void)Bwi;
				(void)LevelParameter;

				Endpoint_Read_Stream_LE(ReceivedBuffer, sizeof(ReceivedBuffer), NULL);

				uint8_t	SendBuffer[0x2] = {0x90, 0x00};
				uint8_t	SendLength			= sizeof(SendBuffer);

				USB_CCID_RDR_to_PC_DataBlock_t* ResponseBlock = (USB_CCID_RDR_to_PC_DataBlock_t*)&BlockBuffer;
				ResponseBlock->CCIDHeader.MessageType = CCID_RDR_to_PC_DataBlock;
				ResponseBlock->CCIDHeader.Slot				= CCIDHeader.Slot;
				ResponseBlock->CCIDHeader.Seq				 = CCIDHeader.Seq;

				ResponseBlock->ChainParam = 0;

				//TODO: callback
				Status = CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR | CCID_ICCSTATUS_PRESENTANDACTIVE;

				if (CCID_CheckStatusNoError(Status) && !CCIDInterfaceInfo->State.Aborted)
				{
					ResponseBlock->CCIDHeader.Length = SendLength;
					memcpy(&ResponseBlock->Data, SendBuffer, SendLength);
				}
				else if(CCIDInterfaceInfo->State.Aborted)
				{
					Status = CCID_COMMANDSTATUS_FAILED | CCID_ICCSTATUS_PRESENTANDACTIVE;
					Error =	CCID_ERROR_CMD_ABORTED;
					SendLength = 0;
				}
				else
				{
					SendLength = 0;
				}

				ResponseBlock->Status = Status;
				ResponseBlock->Error	= Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(ResponseBlock, sizeof(USB_CCID_RDR_to_PC_DataBlock_t) + SendLength, NULL);
				Endpoint_ClearIN();
				break;
			}

			case CCID_PC_to_RDR_Abort:
			{
				USB_CCID_RDR_to_PC_SlotStatus_t* ResponseAbort =	(USB_CCID_RDR_to_PC_SlotStatus_t*)&BlockBuffer;
				ResponseAbort->CCIDHeader.MessageType = CCID_RDR_to_PC_SlotStatus;
				ResponseAbort->CCIDHeader.Length			= 0;
				ResponseAbort->CCIDHeader.Slot				= CCIDHeader.Slot;
				ResponseAbort->CCIDHeader.Seq				 = CCIDHeader.Seq;

				ResponseAbort->ClockStatus = 0;

				Status = CALLBACK_CCID_Abort(CCIDHeader.Slot, CCIDHeader.Seq, &Error);

				ResponseAbort->Status = Status;
				ResponseAbort->Error	= Error;

				Endpoint_ClearOUT();

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(ResponseAbort, sizeof(USB_CCID_RDR_to_PC_SlotStatus_t), NULL);
				Endpoint_ClearIN();
				break;
			}
			default: //TODO
			{
				memset(BlockBuffer, 0x00, sizeof(BlockBuffer));

				Endpoint_SelectEndpoint(CCIDInterfaceInfo->Config.DataINEndpoint.Address);
				Endpoint_Write_Stream_LE(BlockBuffer, sizeof(BlockBuffer), NULL);
				Endpoint_ClearIN();
			}
		}
	}
}

#endif


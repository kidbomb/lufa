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
#include "Iso7816.h"

//design decisions:
// - allocate minimum memory as possible: try to work with previously allocated memory buffers
// - this layer does not know about CCID errors. At some point, CCID has to convert the Iso7816 erros to CCID errors

void ISO7816_Atr_SetTS(uint8_t* atr, uint8_t bitOrderPolarity)
{
  atr[ISO7816_ATR_TS] = bitOrderPolarity; 
}

void ISO7816_Atr_SetT0(uint8_t* atr, uint8_t interfaceBytesPresence, uint8_t historicalBytesLength)
{
	atr[ISO7816_ATR_T0] = (interfaceBytesPresence << 4) + historicalBytesLength;
}

void ISO7816_Atr_SetHistoricalBytes(uint8_t* atr,
	uint8_t *currentAtrIndex,
	uint8_t* historicalBytes, 
	uint8_t historicalBytesLength)
{
	memcpy(atr + *currentAtrIndex + 1, historicalBytes, historicalBytesLength);
	*currentAtrIndex += historicalBytesLength;
}

//creates and ATR with no interface bytes
void ISO7816_CreateAtr(uint8_t* const atr, 
	uint8_t* const atrLength, 
	uint8_t* historicalBytes, 
	uint8_t historicalBytesLength) 
{
	if(historicalBytesLength <= 15) {
		uint8_t currentAtrIndex = 0;

		ISO7816_Atr_SetTS(atr, ISO7816_Atr_BitOrderPolarity_DirectConvention);
		ISO7816_Atr_SetT0(atr, ISO7816_Atr_InterfaceBytesPresence_None, historicalBytesLength);
		currentAtrIndex = ISO7816_ATR_T0;
		ISO7816_Atr_SetHistoricalBytes(atr, &currentAtrIndex, historicalBytes, historicalBytesLength);

		*atrLength = currentAtrIndex + 1;
	}
}

	//https://github.com/philipWendland/IsoApplet/blob/master/src/net/pwendland/javacard/pki/isoapplet/IsoFileSystem.java
	//https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/iso7816.c

void ISO7816_CreateMF(ISO7816_DedicatedFile * mf) {
	mf->FileId[0] = 0x3F;
	mf->FileId[1] = 0x00;
}

void ISO7816_CreateDF(ISO7816_DedicatedFile * df, 
	uint8_t * fileId, 
	uint8_t* fileName, 
	uint8_t fileNameLen, 
	ISO7816_DedicatedFile * mf) {

}
//pass the start of buffer as parameter
//assume filled buffer  = 2 + valueLen
//only accepts arrays < 0xFF
void ISO7816_SetSimpleTLV_Array(uint8_t tag, uint8_t valueLen, uint8_t * valueArray, uint8_t* tlvBuffer)
{
	tlvBuffer[0] = tag;
	tlvBuffer[1] = valueLen;
	memcpy(tlvBuffer + 2, valueArray, valueLen);
}

//pass the start of buffer as parameter
//copyes one value to another
//assume filled buffer = 3 tag = 1, length = 1, value = 1
void ISO7816_SetSimpleTLV_Integer(uint8_t tag, uint8_t valueInteger, uint8_t* tlvBuffer)
{
	tlvBuffer[0] = tag;
	tlvBuffer[1] = 1;
	tlvBuffer[2] = valueInteger;
}

//pass start of buffer as parameter
uint8_t ISO7816_AddFCPData_ToBuffer(ISO7816_DedicatedFile const * df, uint8_t* buffer)
{
	uint8_t currentBufferIndex = 0;

	//set file descriptor to DF
	ISO7816_SetSimpleTLV_Integer(ISO7816_Tag_FileControlParameter_FileDescriptorByte, ISO7816_FileDescriptorByte_DF, buffer);
	currentBufferIndex +=3; //integer tlv has size 1

	//set file identifier
	ISO7816_SetSimpleTLV_Array(ISO7816_Tag_FileControlParameter_FileIdentifier, sizeof(df->FileId), (uint8_t*) df->FileId, buffer + currentBufferIndex);
	currentBufferIndex = currentBufferIndex + 2 + sizeof(df->FileId);

	return currentBufferIndex;
}

uint8_t ISO7816_GetFCI(const ISO7816_DedicatedFile * df, uint8_t* fciBuffer) {

	uint8_t ContainerLength = 0;

	fciBuffer[0] = ISO7816_Tag_FileControlInfo_FCITemplate;

	ContainerLength = ISO7816_AddFCPData_ToBuffer(df, fciBuffer+2);

	fciBuffer[1] = ContainerLength; // we can also infer data size?

	return ContainerLength + 2;
}

void ISO7816_AddStatusBytes(uint8_t sw1, uint8_t sw2, uint8_t* response, uint8_t* responseLength)
{
	response[*responseLength] = sw1;
	response[*responseLength + 1] = sw2;
	*responseLength += 2;
}

void ISO7816_SelectFile_ByFileId(uint8_t fileId[2],
	uint8_t* response, 
	uint8_t* responseLength,
	ISO7816_DedicatedFile * df)
{
  //compare df file id
  //loop through each child, try to find it
	//return fci bytes
	
}

//every response from this function assumes a response buffer with enough space to fill it
//it returns the total response len to the responseLen byte
//selects a file
uint16_t ISO7816_HandleSelect(ISO7816_Command_Header_t * header,
	uint8_t lc,
	uint8_t* data, 
	uint8_t le, uint8_t * response,
	uint8_t * responseLen,
	ISO7816_DedicatedFile *mf)
{

	uint16_t Status = ISO7816_Status_Ok;

	switch(header->P1)
	{
		case ISO7816_P1_Select_ByFileIdentifier:
			if(header->P2 == ISO7816_P2_FIRST_OR_ONLY_OCURRENCE)
			{	
				if(lc == 0) //no data, MF is being selected
				{
					
					//response data
					//TODO: double check if MF is really MF
					uint8_t fciSize =  ISO7816_GetFCI(mf, response);
					*responseLen = fciSize;
					Status = ISO7816_Status_Ok;
					
				}
				else if(lc == 2)
				{
					if(mf->FileId[0] == 0x3f && mf->FileId[1] == 0x00) //is MF file?
					{
						uint8_t fciSize =  ISO7816_GetFCI(mf, response);
						*responseLen = fciSize;
						Status = ISO7816_Status_Ok;	
					}
					else
					{
						Status = ISO7816_Status_Error_FileNotFound;			
					}	
				}
				else
				{
					//File ids can only have 2 bytes
					Status = ISO7816_Status_Error_WrongLength;
				}
				
			}
			else 
			{
				Status = ISO7816_Status_Error_NotImplemented;
			}
			break;
		default:
			Status = ISO7816_Status_Error_NotImplemented;
			break;
	}

	
	return Status;
}

bool ISO7816_ParseAPDU(uint8_t* lcDataLe, uint8_t lcDataLeLength, uint8_t* lc, uint8_t* le, uint8_t** data)
{
	if(lcDataLeLength > 0 )
	{
		*lc = lcDataLe[0];

		if(*lc == lcDataLeLength - 2)
		{
			
			*le = lcDataLe[lcDataLeLength - 1];
			*data  = (uint8_t*) lcDataLe + 1;
			return true;
		}
		else if(*lc == lcDataLeLength - 1)
		{
			*le = 0;
			*data  = (uint8_t*) lcDataLe + 1;
			return true;
		}
		else
		{
			//error: device is sending a packet with a wrong lc
			*lc = 0;
			*le = 0;
			*data = 0;
			return false;
		}
	}
	else
	{
		return true;
	} 
}

uint8_t ISO7816_HandleCommand(
	uint8_t* request,
	uint8_t requestLength,
	uint8_t* response, 
	uint8_t* responseLength, 
	ISO7816_DedicatedFile * mf)
{
	uint16_t Status = ISO7816_Status_Ok;

	ISO7816_Command_Header_t * CommandHeader = (ISO7816_Command_Header_t *) request;

	if(CommandHeader->Cla == (ISO7816_Cla_CommandChaining_Last | ISO7816_Cla_SecureMessaging_NoSM | ISO7816_CLA_LOGICAL_CHANNEL_BASE))
	{
		
		ISO7816_Command_t * Command = (ISO7816_Command_t*) request;

		uint8_t LcDataLeLength = requestLength - sizeof(ISO7816_Command_Header_t);

		uint8_t LC = 0;
		uint8_t LE = 0;
		uint8_t* Data;
		
		bool ParseOK = ISO7816_ParseAPDU(Command->LcDataLe, LcDataLeLength, &LC, &LE, &Data);

		if(ParseOK)
		{
			switch(CommandHeader->Ins)
			{
				case ISO7816_Ins_Select:
					Status = ISO7816_HandleSelect(CommandHeader, LC, Data, LE, response, responseLength, mf);
					break;

				default:
					Status = ISO7816_Status_Error_InstructionNotSupported;
					break;
			}
		}
		else 
		{
			Status = ISO7816_Status_Error_WrongLength;
		}
	}
	else
	{
		Status = ISO7816_Status_Error_ClassNotSupported;
	}

	//TODO: for error, double check there is no data
	//Add response trailer
	switch(Status)
	{
		case ISO7816_Status_Ok:
			ISO7816_AddStatusBytes(0x90, 0x00, response, responseLength);
			break;
		case ISO7816_Status_Error_ClassNotSupported:
			ISO7816_AddStatusBytes(0x6E, 0x00, response, responseLength);
			break;
		case ISO7816_Status_Error_InstructionNotSupported:
			ISO7816_AddStatusBytes(0x6D, 0x00, response, responseLength);
			break;
		case ISO7816_Status_Error_WrongLength:
			ISO7816_AddStatusBytes(0x67, 0x00, response, responseLength);
			break;
		case ISO7816_Status_Error_NotImplemented:
			ISO7816_AddStatusBytes(0xDE, 0xAD, response, responseLength);
			break;

	}

	return Status;
}

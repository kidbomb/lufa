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

void Iso7816_Atr_SetTS(uint8_t* atr, uint8_t bitOrderPolarity)
{
  atr[ISO7816_ATR_TS] = bitOrderPolarity; 
}

void Iso7816_Atr_SetT0(uint8_t* atr, uint8_t interfaceBytesPresence, uint8_t historicalBytesLength)
{
	atr[ISO7816_ATR_T0] = (interfaceBytesPresence << 4) + historicalBytesLength;
}

void Iso7816_Atr_SetHistoricalBytes(uint8_t* atr,
							uint8_t *currentAtrIndex,
							uint8_t* historicalBytes, 
							uint8_t historicalBytesLength)
{
	memcpy(atr + *currentAtrIndex + 1, historicalBytes, historicalBytesLength);
	*currentAtrIndex += historicalBytesLength;
}

//creates and ATR with no interface bytes
void Iso7816_CreateAtr(uint8_t* const atr, 
							uint8_t* const atrLength, 
							uint8_t* historicalBytes, 
							uint8_t historicalBytesLength) 
{
  if(historicalBytesLength <= 15) {
	uint8_t currentAtrIndex = 0;

	Iso7816_Atr_SetTS(atr, ISO7816_Atr_BitOrderPolarity_DirectConvention);
	Iso7816_Atr_SetT0(atr, ISO7816_Atr_InterfaceBytesPresence_None, historicalBytesLength);
	currentAtrIndex = ISO7816_ATR_T0;
	Iso7816_Atr_SetHistoricalBytes(atr, &currentAtrIndex, historicalBytes, historicalBytesLength);
	
	*atrLength = currentAtrIndex + 1;
  }
}

	//https://github.com/philipWendland/IsoApplet/blob/master/src/net/pwendland/javacard/pki/isoapplet/IsoFileSystem.java
	//https://github.com/OpenSC/OpenSC/blob/master/src/libopensc/iso7816.c

void Iso7816_CreateMF(Iso7816_DedicatedFile * mf) {
  mf->FileId[0] = 0x3F;
  mf->FileId[1] = 0x00;
}

void Iso7816_CreateDF(Iso7816_DedicatedFile * df, 
					  uint8_t * fileId, 
					  uint8_t* fileName, 
					  uint8_t fileNameLen, 
					  Iso7816_DedicatedFile * mf) {

}

void Iso7816_SetTLV_Array(uint8_t tag, uint8_t valueLen, uint8_t * valueArray, uint8_t* tlvBuffer, uint8_t *tlvBufferIndex)
{
  tlvBuffer[*tlvBufferIndex++] = tag;
  tlvBuffer[*tlvBufferIndex++] = valueLen;
  memcpy(tlvBuffer, valueArray, valueLen);
  *tlvBufferIndex += valueLen;
}

void Iso7816_SetTLV_Integer(uint8_t tag, uint8_t valueInteger, uint8_t* tlvBuffer, uint8_t *tlvBufferIndex)
{
  tlvBuffer[*tlvBufferIndex++] = tag;
  tlvBuffer[*tlvBufferIndex++] = 1;
  tlvBuffer[*tlvBufferIndex++] = valueInteger;
}

void Iso7816_GetContainer(Iso7816_DedicatedFile const * df, uint8_t* containerBuffer, uint8_t *containerLen)
{
  *containerLen = 0;
  //set file descriptor to DF
  Iso7816_SetTLV_Integer(ISO7816_FileControlParameter_FileDescriptorByte, ISO7816_FileDescriptorByte_DF, containerBuffer, containerLen);
  //set file identifier
  Iso7816_SetTLV_Array(ISO7816_FileControlParameter_FileIdentifier, sizeof(df->FileId), (uint8_t*)df->FileId, containerBuffer, containerLen);
  //set filename, if any
  if(strlen(df->FileName) > 0) {
	Iso7816_SetTLV_Array(0x84, strlen(df->FileName), df->FileName, containerBuffer, containerLen);
  }
}

void Iso7816_GetFCI(const Iso7816_DedicatedFile * df, uint8_t* fciBuffer, uint8_t * fciLen) {

  uint8_t ContainerBuffer[32];
  uint8_t ContainerLen;
  Iso7816_GetContainer(df, ContainerBuffer, &ContainerLen);

  Iso7816_SetTLV_Array(0x6F, ContainerLen, ContainerBuffer, fciBuffer, fciLen);

	  //EXAMPLE
	  //new byte[] {(byte)0x6F, (byte)0x07, // FCI, Length 7.
		  //(byte)0x82, (byte)0x01, (byte)0x38, // File descriptor byte.
		  //  bin(00111000) = 0x38 (DF) (file descriptor byte)
		  //(byte)0x83, (byte)0x02, (byte)0x3F, (byte)0x00
	  // 0x84 - file name
	  
}

void Iso7816_SelectFile_ByName(uint8_t* fileName, uint8_t fileNameLen, Iso7816_DedicatedFile * mf)
{
  //loop through childs, try to find file
  //if find, return FCI bytes
  //it not, return error
}

void Iso7816_SelectFile_ByFileId(uint8_t fileId[2], Iso7816_DedicatedFile * df)
{
  //compare df file id
  //loop through each child, try to find it
}


void Iso7816_HandleCommand(uint8_t* request,
						   uint8_t requestLength,
						   uint8_t* response, 
						   uint8_t* responseLength)
{

}

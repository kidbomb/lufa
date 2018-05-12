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

#ifndef _ISO7816_H_
#define _ISO7816_H_
  /* Includes: */
	#include <avr/io.h>
	#include <avr/wdt.h>
	#include <avr/power.h>
	#include <avr/interrupt.h>
	#include <stdlib.h>
	#include <string.h>
  /* Macros: */

	#define ISO7816_ATR_TS              0x00
	#define ISO7816_ATR_T0              0x01

	#define ISO7816_CLA_LOGICAL_CHANNEL_BASE    0x00

	#define ISO7816_P1_SELECT_BY_FILE_IDENTIFIER  0x00
	#define ISO7816_P1_SELECT_BY_DF_NAME      0x04
	#define ISO7816_P1_SELECT_BY_PATH_FROM_MF   0x08
	#define ISO7816_P1_SELECT_BY_PATH_FROM_DF   0x09

	#define ISO7816_P1_FIRST_OR_ONLY_OCURRENCE    0x00

  /* Enums: */
	enum ISO7816_Atr_BitOrderPolarity_t
	{
	  ISO7816_Atr_BitOrderPolarity_DirectConvention = 0x3B,
	  ISO7816_Atr_BitOrderPolarity_InverseConvention  = 0x3F,
	};

	enum ISO7816_Atr_InterfaceBytesPresence_t
	{
	  ISO7816_Atr_InterfaceBytesPresence_None = 0x00,
	  ISO7816_Atr_InterfaceBytesPresence_TA = 0x10,
	  ISO7816_Atr_InterfaceBytesPresence_TB = 0x20,
	  ISO7816_Atr_InterfaceBytesPresence_TC = 0x40,
	  ISO7816_Atr_InterfaceBytesPresence_TD = 0x80,
	};

	enum ISO7816_FileControlParameter_t
	{
	  ISO7816_FileControlParameter_FileDescriptorByte = 0x82,
	  ISO7816_FileControlParameter_FileIdentifier = 0x83,
	  ISO7816_FileControlParameter_DFName = 0x84
	};

	enum ISO7816_FileDescriptorByte_t
	{
	  ISO7816_FileDescriptorByte_DF = 0x38
	};

	/** Class
	 */
	enum ISO7816_Cla_CommandChaining_t
	{
	  ISO7816_Cla_CommandChaining_Last  = 0,
	  ISO7816_Cla_CommandChaining_NotLast = (1 << 4),
	};

	enum ISO7816_Cla_SecureMessaging_t
	{
	  ISO7816_Cla_SecureMessaging_NoSM              = 0,
	  ISO7816_Cla_SecureMessaging_ProprietarySMFormat       = (1 << 2),
	  ISO7816_Cla_SecureMessaging_SM_CommandHeaderNotProcessed  = (1 << 3),
	  ISO7816_Cla_SecureMessaging_SM_CommandHeaderAuthenticated = (1 << 3) | (1 << 2),
	};

	enum ISO7816_Ins_t
	{
	  ISO7816_Ins_DeactivateFile      = 0x04,
	  ISO7816_Ins_EraseRecord       = 0x0C,
	  ISO7816_Ins_EraseBinary       = 0x0E,
	  ISO7816_Ins_Select          = 0xA4,
	  ISO7816_Ins_ReadBinary        = 0xB0,
	  ISO7816_Ins_ReadRecord        = 0xB2,
	  ISO7816_Ins_WriteBinary       = 0xD0,
	  ISO7816_Ins_WriteRecord       = 0xD2,
	  ISO7816_Ins_UpdateBinary      = 0xD6,
	  ISO7816_Ins_AppendRecord      = 0xE2
	};

  /* Function Prototypes: */
	void Iso7816_CreateAtr(uint8_t* const atr, 
							uint8_t* const atrLength, 
							uint8_t* historicalBytes, 
							uint8_t historicalBytesLength);

	/* Type Defines: */
	typedef struct
	{
	  uint8_t Cla;
	  uint8_t Ins;
	  uint8_t P1;
	  uint8_t P2;
	  uint8_t Data[0];
	} Iso7816_Command_t;

	typedef struct
	{
	  uint8_t Sw1;
	  uint8_t Sw2;
	} Iso7816_Response_t;

	typedef struct
	{
	  uint8_t FileId[2];
	  uint8_t FileName[16];

	  //TODO: add children
	} Iso7816_DedicatedFile;
  /* Function Prototypes: */
	voidISO7816_CreateAtr(uint8_t* const atr, 
				uint8_t* const atrLength, 
				uint8_t* historicalBytes, 
				uint8_t historicalBytesLength);
#endif

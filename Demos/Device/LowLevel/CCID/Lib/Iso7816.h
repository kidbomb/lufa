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
		#include <stdbool.h>
	/* Macros: */

		#define ISO7816_ATR_TS							0x00
		#define ISO7816_ATR_T0							0x01

		#define ISO7816_CLA_LOGICAL_CHANNEL_BASE		0x00

		#define ISO7816_P2_FIRST_OR_ONLY_OCURRENCE		0x00

	/* Enums: */

		enum ISO7816_Atr_BitOrderPolarity_t
		{
			ISO7816_Atr_BitOrderPolarity_DirectConvention	= 0x3B,
			ISO7816_Atr_BitOrderPolarity_InverseConvention	= 0x3F,
		};

		enum ISO7816_Atr_InterfaceBytesPresence_t
		{
			ISO7816_Atr_InterfaceBytesPresence_None	= 0x00,
			ISO7816_Atr_InterfaceBytesPresence_TA	= 0x10,
			ISO7816_Atr_InterfaceBytesPresence_TB	= 0x20,
			ISO7816_Atr_InterfaceBytesPresence_TC	= 0x40,
			ISO7816_Atr_InterfaceBytesPresence_TD	= 0x80,
		};

		//Interindustry templates for file control information
		enum ISO7816_Tag_FileControlInfo_t
		{
			
			ISO7816_Tag_FileControlInfo_FCPTemplate = 0x62,
			ISO7816_Tag_FileControlInfo_FMDTemplate = 0x64,
			ISO7816_Tag_FileControlInfo_FCITemplate = 0x6F
		};

		//File control parameter data objects
		enum ISO7816_Tag_FileControlParameter_t
		{
			ISO7816_Tag_FileControlParameter_FileDescriptorByte = 0x82,
			ISO7816_Tag_FileControlParameter_FileIdentifier = 0x83,
			ISO7816_Tag_FileControlParameter_DFName = 0x84
		};

		enum ISO7816_FileDescriptorByte_t
		{
			ISO7816_FileDescriptorByte_DF = 0x38
		};

		/** Class
		 */
		enum ISO7816_Cla_CommandChaining_t
		{
			ISO7816_Cla_CommandChaining_Last	= 0,
			ISO7816_Cla_CommandChaining_NotLast	= (1 << 4),
		};

		enum ISO7816_Cla_SecureMessaging_t
		{
			ISO7816_Cla_SecureMessaging_NoSM							= 0,
			ISO7816_Cla_SecureMessaging_ProprietarySMFormat				= (1 << 2),
			ISO7816_Cla_SecureMessaging_SM_CommandHeaderNotProcessed	= (1 << 3),
			ISO7816_Cla_SecureMessaging_SM_CommandHeaderAuthenticated	= (1 << 3) | (1 << 2),
		};

		enum ISO7816_P1_Select_t
		{
			ISO7816_P1_Select_ByFileIdentifier 	= 0x00,
			ISO7816_P1_Select_ByDFName 			= 0x04,
			ISO7816_P1_Select_ByPathFromMf 		= 0x08,
			ISO7816_P1_Select_ByPathFromDf 		= 0x09
		};

		enum ISO7816_Ins_t
		{
			ISO7816_Ins_DeactivateFile			= 0x04,
			ISO7816_Ins_EraseRecord				= 0x0C,
			ISO7816_Ins_EraseBinary				= 0x0E,
			ISO7816_Ins_Select					= 0xA4,
			ISO7816_Ins_ReadBinary				= 0xB0,
			ISO7816_Ins_ReadRecord				= 0xB2,
			ISO7816_Ins_WriteBinary				= 0xD0,
			ISO7816_Ins_WriteRecord				= 0xD2,
			ISO7816_Ins_UpdateBinary			= 0xD6,
			ISO7816_Ins_AppendRecord			= 0xE2
		};

		enum ISO7816_Status_t
		{
			ISO7816_Status_Ok 								= 0x9000,
			ISO7816_Status_Error_WrongLength 				= 0x6700,
			ISO7816_Status_Error_FileNotFound 				= 0x6A82,
			ISO7816_Status_Error_InstructionNotSupported 	= 0x6D00,
			ISO7816_Status_Error_ClassNotSupported 			= 0x6E00,
			ISO7816_Status_Error_NoPreciseDiagnostics 		= 0x6F00,
			ISO7816_Status_Error_UnknownError 				= 0x6F00,
			ISO7816_Status_Error_NotImplemented 			= 0xDEAD //not actually part of the standard
		};

		/* Type Defines: */
		typedef struct
        {
                uint8_t Cla;
                uint8_t Ins;
                uint8_t P1;
                uint8_t P2;
        } ISO7816_Command_Header_t;

		typedef struct
		{
			ISO7816_Command_Header_t Header;
			uint8_t LcDataLe[0];
		} ISO7816_Command_t;

		typedef struct
		{
			uint8_t FileId[2];
			uint8_t DfName[16];
			uint8_t* Children[0];
		} ISO7816_DedicatedFile;

	/* Function Prototypes: */
		void ISO7816_CreateAtr(uint8_t* const atr, 
							  uint8_t* const atrLength, 
							  uint8_t* historicalBytes, 
							  uint8_t historicalBytesLength);

		uint8_t ISO7816_HandleCommand(uint8_t* request,
									uint8_t requestLength,
									uint8_t* response, 
									uint8_t* responseLength, 
									ISO7816_DedicatedFile * mf);

#endif

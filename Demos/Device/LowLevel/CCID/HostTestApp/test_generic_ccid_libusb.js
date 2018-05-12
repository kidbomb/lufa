#!/usr/bin/env node

//             LUFA Library
//     Copyright (C) Dean Camera, 2018.
//
//  dean [at] fourwalledcubicle [dot] com
//
//           www.lufa-lib.org

// Copyright 2018  Filipe Rodrigues (filipepazrodrigues [at] gmail [dot] com)
//
// LUFA Generic CCID device demo host test script. This script test multiple
// kinds of CCID messages and shows the result to the console
//
// You have to install the usb and async modules prior to executing this script:
// apt-get install libusb-1.0-0-dev
// npm install usb async sprintf

var usb = require('usb');
var async = require('async');
var sprintf = require('sprintf');

var deviceVid = 0x03EB;
var devicePid = 0x206E;

var CCID_PC_to_RDR_IccPowerOn           = 0x62;
var CCID_PC_to_RDR_IccPowerOff          = 0x63;
var CCID_PC_to_RDR_GetSlotStatus        = 0x65;
var CCID_PC_to_RDR_XfrBlock         = 0x6f;

function getAndInitCcidDeviceAndInterface()
{
    device = usb.findByIds(deviceVid, devicePid);
    if (!device) {
        console.log('No device found');
        process.exit(1);
    }
    device.open();

    var ccidInterface = device.interface(0);
    if (ccidInterface.isKernelDriverActive()) {
        ccidInterface.detachKernelDriver();
    }
    ccidInterface.claim();

    return {ccidDevice:device, ccidInterface:ccidInterface};
}

function read(ccidInterface, responseSize,  callback)
{
    inEndpoint = ccidInterface.endpoints[0];
    inEndpoint.transfer(responseSize, function(error, data) {
        if (error) {
            console.log(error)
        } else {
            console.log("Received data:", data);
        }
        callback();
    });
}

function write(ccidInterface, message, callback)
{
    outEndpoint = ccidInterface.endpoints[1]
    outEndpoint.transfer(    // Send a Set Report control request
        message,                  // message to be sent
        function(error) {   // callback to be executed upon finishing the transfer
            callback();
        }
    );
}

//CCID functions

function IccPowerOnMessage(slot, seq)
{
    return [
        CCID_PC_to_RDR_IccPowerOn, //message type
        0, 0, 0, 0, //length
        slot,
        seq,
        0, //power select: automatic
        0, 0 //RFU
    ];
}

function IccPowerOffMessage(slot, seq)
{
    return [
        CCID_PC_to_RDR_IccPowerOff, //message type
        0, 0, 0, 0, //length
        slot,
        seq,
        0, 0, 0 //RFU
    ];
}

function GetSlotStatusMessage(slot, seq)
{
    return [
        CCID_PC_to_RDR_GetSlotStatus, //message type
        0, 0, 0, 0, //length
        slot,
        seq,
        0, 0, 0 //RFU
    ];
}

function XfrBlockMessage(slot, seq, apdu)
{
    return [
        CCID_PC_to_RDR_XfrBlock, //message type
        apdu.length, 0, 0, 0, //length: only for < 0xFF
        slot,
        seq,
        0, //BWI 
        0, 0 //level parameter
    ].concat(apdu);

}

function startTest()
{
    async.series([
        function(callback) {
            write(ccidInterface, new Buffer(IccPowerOnMessage(0, 1)), callback);
        },
        function(callback) {
            read(ccidInterface, 10 + 17, callback);
        },
        function(callback) {
            write(ccidInterface, new Buffer(IccPowerOffMessage(0, 2)), callback);
        },
        function(callback) {
            read(ccidInterface, 10, callback);
        },
        function(callback) {
            write(ccidInterface, new Buffer(GetSlotStatusMessage(0, 3)), callback);
        },
        function(callback) {
            read(ccidInterface, 10, callback);
        },
        function(callback) {
            //ISO 7816 test: unknown CLA (0xFF)
            write(ccidInterface, new Buffer(XfrBlockMessage(0, 4, [0xFF, 0x00, 0x0, 0x0, 0x0])), callback);
        },
        function(callback) {
            //must return 80 02 00 00 00 00 04 02 80 00 6e 00
            //80: Status
            //00: Error
            //6e 00: Class not supported error
            read(ccidInterface, 10 + 2, callback);
         },
         function(callback) {
            //ISO 7816 test: unknown INS (0xFF)
            write(ccidInterface, new Buffer(XfrBlockMessage(0, 5, [0x0, 0xFF, 0x0, 0x0, 0x0])), callback);
        },
        function(callback) {
            //must return 80 02 00 00 00 00 04 02 80 00 6d 00
            //80: Status
            //00: Error
            //6e 00: Instruction not supported error
            read(ccidInterface, 10 + 2, callback);
         },
        function(callback) {
            //ISO 7816 test: Select MF (0xA4)
            write(ccidInterface, new Buffer(XfrBlockMessage(0, 6, [0x0, 0xA4, 0x0, 0x0])), callback);
        },
        function(callback) {
            //must return 80 0b 00 00 00 00 06 02 80 00 6f 07 82 01 38 83 02 3f 00 90 00
            //80: Status
            //00: Error

            //new byte[] {(byte)0x6F, (byte)0x07, // FCI, Length 7.
            //(byte)0x82, (byte)0x01, (byte)0x38, // File descriptor byte.
            //  bin(00111000) = 0x38 (DF) (file descriptor byte)
            //(byte)0x83, (byte)0x02, (byte)0x3F, (byte)0x00
            // 0x84 - file name

            // 90 00: no error
            read(ccidInterface, 31, callback);
         },
        function(callback) {
            //ISO 7816 test: Select MF (0xA4)
            //MF has file if = 0x3f00
            write(ccidInterface, new Buffer(XfrBlockMessage(0, 6, [0x0, 0xA4, 0x0, 0x0, 0x02, 0x3F, 0x00])), callback);
        },
        function(callback) {
            //must return 80 0b 00 00 00 00 06 02 80 00 6f 07 82 01 38 83 02 3f 00 90 00
            //80: Status
            //00: Error

            //new byte[] {(byte)0x6F, (byte)0x07, // FCI, Length 7.
            //(byte)0x82, (byte)0x01, (byte)0x38, // File descriptor byte.
            //  bin(00111000) = 0x38 (DF) (file descriptor byte)
            //(byte)0x83, (byte)0x02, (byte)0x3F, (byte)0x00
            // 0x84 - file name
            
            // 90 00: no error
            read(ccidInterface, 31, callback);
         }
        ]);
}

var ccidDeviceAndInterface = getAndInitCcidDeviceAndInterface();
var ccidDevice = ccidDeviceAndInterface.ccidDevice
var ccidInterface = ccidDeviceAndInterface.ccidInterface;

console.log(sprintf("Connected to device 0x%04X/0x%04X - %s [%s]",
            ccidDevice.deviceDescriptor.idVendor,
            ccidDevice.deviceDescriptor.idProduct,
            ccidDevice.deviceDescriptor.iProduct,
            ccidDevice.deviceDescriptor.iManufacturer));

startTest();

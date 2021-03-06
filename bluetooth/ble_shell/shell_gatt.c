/*! *********************************************************************************
 * \addtogroup SHELL GATT
 * @{
 ********************************************************************************** */
/*!
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 *
 * \file
 *
 * This file is the source file for the GATT Shell module
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
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

/************************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ************************************************************************************/
/* Framework / Drivers */
#include "TimersManager.h"
#include "FunctionLib.h"
#include "fsl_os_abstraction.h"
#include "shell.h"
#include "Panic.h"
#include "MemManager.h"
#include "board.h"

#include "shell_gatt.h"
#include "shell_gap.h"

#include <stdlib.h>
#include <string.h>

#include "ble_shell.h"
#include "cmd_ble.h"

/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/
#define mShellGattCmdsCount_c               6

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/
typedef struct gattCmds_tag
{
    char       *name;
    int8_t (*cmd)(uint8_t argc, char *argv[]);
} gattCmds_t;

typedef enum gattNotifyState_tag
{
    gGetCccd_c,
    gGetNotificationStatus_c
} gattNotifyState_t;

/************************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/
/* Shell API Functions */
static int8_t ShellGatt_Discover(uint8_t argc, char *argv[]);
static int8_t ShellGatt_Read(uint8_t argc, char *argv[]);
static int8_t ShellGatt_Write(uint8_t argc, char *argv[], bool_t fNoRsp);
static int8_t ShellGatt_WriteCmd(uint8_t argc, char *argv[]);
static int8_t ShellGatt_WriteRsp(uint8_t argc, char *argv[]);
static void ShellGatt_NotifySm(void *param);
//static int8_t ShellGatt_Indicate(uint8_t argc, char *argv[]);

//static void ShellGatt_DiscoveryFinished(void);
//static void ShellGatt_DiscoveryHandler
//(
//    deviceId_t serverDeviceId,
//    gattProcedureType_t procedureType
//);
//static bool_t ShellGatt_CheckProcedureFinished(void);
//static void ShellGatt_PrintIndexedService(uint8_t index);
//static void ShellGatt_PrintIndexedCharacteristic(gattService_t *pService, uint8_t index);

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
const gattCmds_t mGattShellCmds[mShellGattCmdsCount_c] =
{
    {"discover",    ShellGatt_Discover},
    {"read",        ShellGatt_Read},
    {"write",       ShellGatt_WriteRsp},
    {"writecmd",    ShellGatt_WriteCmd},
    {"notify",      ShellGatt_Notify},
    {"indicate",    ShellGatt_Indicate}
};

const gattUuidNames_t mGattServices[] =
{
    {  0x1800  , "Generic Access"},
    {  0x1801  , "Generic Attribute"},
    {  0x1802  , "Immediate Alert"},
    {  0x1803  , "Link Loss"},
    {  0x1804  , "Tx Power"},
    {  0x1805  , "Current Time Service"},
    {  0x1806  , "Reference Time Update Service"},
    {  0x1807  , "Next DST Change Service"},
    {  0x1808  , "Glucose"},
    {  0x1809  , "Health Thermometer"},
    {  0x180A  , "Device Information"},
    {  0x180D  , "Heart Rate"},
    {  0x180E  , "Phone Alert Status Service"},
    {  0x180F  , "Battery Service"},
    {  0x1810  , "Blood Pressure"},
    {  0x1811  , "Alert Notification Service"},
    {  0x1812  , "Human Interface Device"},
    {  0x1813  , "Scan Parameters"},
    {  0x1814  , "Running Speed and Cadence"},
    {  0x1815  , "Automation IO"},
    {  0x1816  , "Cycling Speed and Cadence"},
    {  0x1818  , "Cycling Power"},
    {  0x1819  , "Location and Navigation"},
    {  0x181A  , "Environmental Sensing"},
    {  0x181B  , "Body Composition"},
    {  0x181C  , "User Data"},
    {  0x181D  , "Weight Scale"},
    {  0x181E  , "Bond Management"},
    {  0x181F  , "Continuous Glucose Monitoring"},
    {  0x1820  , "Internet Protocol Support"},
    {  0x1821  , "Indoor Positioning"},
    {  0x1822  , "Pulse Oximeter"},
    {  0x1823  , "HTTP Proxy"},
    {  0x1824  , "Transport Discovery"},
    {  0x1825  , "Object Transfer"}
};

const gattUuidNames_t mGattChars[] =
{
    {  0x2A00  , "Device Name"},
    {  0x2A01  , "Appearance"},
    {  0x2A02  , "Peripheral Privacy Flag"},
    {  0x2A03  , "Reconnection Address"},
    {  0x2A04  , "Peripheral Preferred Connection Parameters"},
    {  0x2A05  , "Service Changed"},
    {  0x2A06  , "Alert Level"},
    {  0x2A07  , "Tx Power Level"},
    {  0x2A08  , "Date Time"},
    {  0x2A09  , "Day of Week"},
    {  0x2A0A  , "Day Date Time"},
    {  0x2A0C  , "Exact Time 256"},
    {  0x2A0D  , "DST Offset"},
    {  0x2A0E  , "Time Zone"},
    {  0x2A0F  , "Local Time Information"},
    {  0x2A11  , "Time with DST"},
    {  0x2A12  , "Time Accuracy"},
    {  0x2A13  , "Time Source"},
    {  0x2A14  , "Reference Time Information"},
    {  0x2A16  , "Time Update Control Point"},
    {  0x2A17  , "Time Update State"},
    {  0x2A18  , "Glucose Measurement"},
    {  0x2A19  , "Battery Level"},
    {  0x2A1C  , "Temperature Measurement"},
    {  0x2A1D  , "Temperature Type"},
    {  0x2A1E  , "Intermediate Temperature"},
    {  0x2A21  , "Measurement Interval"},
    {  0x2A22  , "Boot Keyboard Input Report"},
    {  0x2A23  , "System ID"},
    {  0x2A24  , "Model Number String"},
    {  0x2A25  , "Serial Number String"},
    {  0x2A26  , "Firmware Revision String"},
    {  0x2A27  , "Hardware Revision String"},
    {  0x2A28  , "Software Revision String"},
    {  0x2A29  , "Manufacturer Name String"},
    {  0x2A2A  , "IEEE 11073-20601 Regulatory Certification Data List"},
    {  0x2A2B  , "Current Time"},
    {  0x2A2C  , "Magnetic Declination"},
    {  0x2A31  , "Scan Refresh"},
    {  0x2A32  , "Boot Keyboard Output Report"},
    {  0x2A33  , "Boot Mouse Input Report"},
    {  0x2A34  , "Glucose Measurement Context"},
    {  0x2A35  , "Blood Pressure Measurement"},
    {  0x2A36  , "Intermediate Cuff Pressure"},
    {  0x2A37  , "Heart Rate Measurement"},
    {  0x2A38  , "Body Sensor Location"},
    {  0x2A39  , "Heart Rate Control Point"},
    {  0x2A3F  , "Alert Status"},
    {  0x2A40  , "Ringer Control Point"},
    {  0x2A41  , "Ringer Setting"},
    {  0x2A42  , "Alert Category ID Bit Mask"},
    {  0x2A43  , "Alert Category ID"},
    {  0x2A44  , "Alert Notification Control Point"},
    {  0x2A45  , "Unread Alert Status"},
    {  0x2A46  , "New Alert"},
    {  0x2A47  , "Supported New Alert Category"},
    {  0x2A48  , "Supported Unread Alert Category"},
    {  0x2A49  , "Blood Pressure Feature"},
    {  0x2A4A  , "HID Information"},
    {  0x2A4B  , "Report Map"},
    {  0x2A4C  , "HID Control Point"},
    {  0x2A4D  , "Report"},
    {  0x2A4E  , "Protocol Mode"},
    {  0x2A4F  , "Scan Interval Window"},
    {  0x2A50  , "PnP ID"},
    {  0x2A51  , "Glucose Feature"},
    {  0x2A52  , "Record Access Control Point"},
    {  0x2A53  , "RSC Measurement"},
    {  0x2A54  , "RSC Feature"},
    {  0x2A55  , "SC Control Point"},
    {  0x2A56  , "Digital"},
    {  0x2A58  , "Analog"},
    {  0x2A5A  , "Aggregate"},
    {  0x2A5B  , "CSC Measurement"},
    {  0x2A5C  , "CSC Feature"},
    {  0x2A5D  , "Sensor Location"},
    {  0x2A5E  , "PLX Spot-Check Measurement"},
    {  0x2A5F  , "PLX Continuous Measurement"},
    {  0x2A60  , "PLX Features"},
    {  0x2A63  , "Cycling Power Measurement"},
    {  0x2A64  , "Cycling Power Vector"},
    {  0x2A65  , "Cycling Power Feature"},
    {  0x2A66  , "Cycling Power Control Point"},
    {  0x2A67  , "Location and Speed"},
    {  0x2A68  , "Navigation"},
    {  0x2A69  , "Position Quality"},
    {  0x2A6A  , "LN Feature"},
    {  0x2A6B  , "LN Control Point"},
    {  0x2A6C  , "Elevation"},
    {  0x2A6D  , "Pressure"},
    {  0x2A6E  , "Temperature"},
    {  0x2A6F  , "Humidity"},
    {  0x2A70  , "True Wind Speed"},
    {  0x2A71  , "True Wind Direction"},
    {  0x2A72  , "Apparent Wind Speed"},
    {  0x2A73  , "Apparent Wind Direction�"},
    {  0x2A74  , "Gust Factor"},
    {  0x2A75  , "Pollen Concentration"},
    {  0x2A76  , "UV Index"},
    {  0x2A77  , "Irradiance"},
    {  0x2A78  , "Rainfall"},
    {  0x2A79  , "Wind Chill"},
    {  0x2A7A  , "Heat Index"},
    {  0x2A7B  , "Dew Point"},
    {  0x2A7D  , "Descriptor Value Changed"},
    {  0x2A7E  , "Aerobic Heart Rate Lower Limit"},
    {  0x2A7F  , "Aerobic Threshold"},
    {  0x2A80  , "Age"},
    {  0x2A81  , "Anaerobic Heart Rate Lower Limit"},
    {  0x2A82  , "Anaerobic Heart Rate Upper Limit"},
    {  0x2A83  , "Anaerobic Threshold"},
    {  0x2A84  , "Aerobic Heart Rate Upper Limit"},
    {  0x2A85  , "Date of Birth"},
    {  0x2A86  , "Date of Threshold Assessment"},
    {  0x2A87  , "Email Address"},
    {  0x2A88  , "Fat Burn Heart Rate Lower Limit"},
    {  0x2A89  , "Fat Burn Heart Rate Upper Limit"},
    {  0x2A8A  , "First Name"},
    {  0x2A8B  , "Five Zone Heart Rate Limits"},
    {  0x2A8C  , "Gender"},
    {  0x2A8D  , "Heart Rate Max"},
    {  0x2A8E  , "Height"},
    {  0x2A8F  , "Hip Circumference"},
    {  0x2A90  , "Last Name"},
    {  0x2A91  , "Maximum Recommended Heart Rate"},
    {  0x2A92  , "Resting Heart Rate"},
    {  0x2A93  , "Sport Type for Aerobic and Anaerobic Thresholds"},
    {  0x2A94  , "Three Zone Heart Rate Limits"},
    {  0x2A95  , "Two Zone Heart Rate Limit"},
    {  0x2A96  , "VO2 Max"},
    {  0x2A97  , "Waist Circumference"},
    {  0x2A98  , "Weight"},
    {  0x2A99  , "Database Change Increment"},
    {  0x2A9A  , "User Index"},
    {  0x2A9B  , "Body Composition Feature"},
    {  0x2A9C  , "Body Composition Measurement"},
    {  0x2A9D  , "Weight Measurement"},
    {  0x2A9E  , "Weight Scale Feature"},
    {  0x2A9F  , "User Control Point"},
    {  0x2AA0  , "Magnetic Flux Density - 2D"},
    {  0x2AA1  , "Magnetic Flux Density - 3D"},
    {  0x2AA2  , "Language"},
    {  0x2AA3  , "Barometric Pressure Trend"},
    {  0x2AA4  , "Bond Management Control Point"},
    {  0x2AA5  , "Bond Management Feature"},
    {  0x2AA6  , "Central Address Resolution"},
    {  0x2AA7  , "CGM Measurement"},
    {  0x2AA8  , "CGM Feature"},
    {  0x2AA9  , "CGM Status"},
    {  0x2AAA  , "CGM Session Start Time"},
    {  0x2AAB  , "CGM Session Run Time"},
    {  0x2AAC  , "CGM Specific Ops Control Point"},
    {  0x2AAD  , "Indoor Positioning Configuration"},
    {  0x2AAE  , "Latitude"},
    {  0x2AAF  , "Longitude"},
    {  0x2AB0  , "Local North Coordinate"},
    {  0x2AB1  , "Local East Coordinate"},
    {  0x2AB2  , "Floor Number"},
    {  0x2AB3  , "Altitude"},
    {  0x2AB4  , "Uncertainty"},
    {  0x2AB5  , "Location Name"},
    {  0x2AB6  , "URI"},
    {  0x2AB7  , "HTTP Headers"},
    {  0x2AB8  , "HTTP Status Code"},
    {  0x2AB9  , "HTTP Entity Body"},
    {  0x2ABA  , "HTTP Control Point"},
    {  0x2ABB  , "HTTPS Security"},
    {  0x2ABC  , "TDS Control Point"},
    {  0x2ABD  , "OTS Feature"},
    {  0x2ABE  , "Object Name"},
    {  0x2ABF  , "Object Type"},
    {  0x2AC0  , "Object Size"},
    {  0x2AC1  , "Object First-Created"},
    {  0x2AC2  , "Object Last-Modified"},
    {  0x2AC3  , "Object ID"},
    {  0x2AC4  , "Object Properties"},
    {  0x2AC5  , "Object Action Control Point"},
    {  0x2AC6  , "Object List Control Point"},
    {  0x2AC7  , "Object List Filter"},
    {  0x2AC8  , "Object Changed"}
};

const gattUuidNames_t mGattDescriptors[] =
{
    {0x2900, "Characteristic Extended Properties"},
    {0x2901, "Characteristic User Description"},
    {0x2902, "Client Characteristic Configuration"},
    {0x2903, "Server Characteristic Configuration"},
    {0x2904, "Characteristic Presentation Format"},
    {0x2905, "Characteristic Aggregate Format"},
    {0x2906, "Valid Range"},
    {0x2907, "External Report Reference"},
    {0x2908, "Report Reference"},
    {0x2909, "Number of Digitals"},
    {0x290A, "Value Trigger Setting"},
    {0x290B, "Environmental Sensing Configuration"},
    {0x290C, "Environmental Sensing Measurement"},
    {0x290D, "Environmental Sensing Trigger Setting"},
    {0x290E, "Time Trigger Setting"}
};

//const char* mGattStatus[] = {
//    "NoError",
//    "InvalidHandle",
//    "ReadNotPermitted",
//    "WriteNotPermitted",
//    "InvalidPdu",
//    "InsufficientAuthentication",
//    "RequestNotSupported",
//    "InvalidOffset",
//    "InsufficientAuthorization",
//    "PrepareQueueFull",
//    "AttributeNotFound",
//    "AttributeNotLong",
//    "InsufficientEncryptionKeySize",
//    "InvalidAttributeValueLength",
//    "UnlikelyError",
//    "InsufficientEncryption",
//    "UnsupportedGroupType",
//    "InsufficientResources",
//};
//
///* Buffer used for Service Discovery */
//static gattService_t *mpServiceDiscoveryBuffer = NULL;
//static uint8_t  mcPrimaryServices = 0;
//
///* Buffer used for Characteristic Discovery */
//static gattCharacteristic_t *mpCharBuffer = NULL;
//static uint8_t mCurrentServiceInDiscoveryIndex;
//static uint8_t mCurrentCharInDiscoveryIndex;
//static uint8_t  mcCharacteristics = 0;
//
///* Buffer used for Characteristic Descriptor Discovery */
//static gattAttribute_t *mpCharDescriptorBuffer = NULL;
//

static tmrTimerID_t mDelayTimerID = gTmrInvalidTimerID_c;
static uint16_t mNotifyHandle = INVALID_HANDLE;
static bool_t bIndicate = FALSE;

/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/
extern deviceId_t   gPeerDeviceId;

uint16_t gCccdHandle = INVALID_HANDLE;
bool_t gIsNotificationActive = FALSE;
bool_t gIsIndicationActive = FALSE;

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
int8_t ShellGatt_Command(uint8_t argc, char *argv[])
{
    uint8_t i;

    if (argc < 2)
    {
        return CMD_RET_USAGE;
    }

    for (i = 0; i < mShellGattCmdsCount_c; i++)
    {
        if (!strcmp((char *)argv[1], mGattShellCmds[i].name))
        {
            return mGattShellCmds[i].cmd(argc - 2, (char **)(&argv[2]));
        }
    }

    return CMD_RET_USAGE;
}

///*! *********************************************************************************
// * \brief        Handles GATT client callback from host stack.
// *
// * \param[in]    serverDeviceId      Server peer device ID.
// * \param[in]    procedureType       GATT procedure type.
// * \param[in]    procedureType       GATT procedure result.
// * \param[in]    error               Result.
// ********************************************************************************** */
//void ShellGatt_ClientCallback (
//                                       deviceId_t serverDeviceId,
//                                       gattProcedureType_t procedureType,
//                                       gattProcedureResult_t procedureResult,
//                                       bleResult_t error)
//{
//    if (procedureResult == gGattProcError_c)
//    {
//        uint8_t attError = (uint8_t) (error & 0xFF);
//        shell_write("\n\r-->  GATT Event: Procedure Error ");
//        shell_write((char*)mGattStatus[attError]);
//        SHELL_NEWLINE();
//
//        ShellGatt_DiscoveryFinished();
//    }
//    else if (procedureResult == gGattProcSuccess_c)
//    {
//        switch(procedureType)
//        {
//            case gGattProcExchangeMtu_c:
//            {
//                shell_write("\n\r-->  MTU Exchanged.");
//                shell_cmd_finished();
//            }
//            break;
//
//            case gGattProcDiscoverAllCharacteristics_c:
//            case gGattProcDiscoverAllCharacteristicDescriptors_c:
//            case gGattProcDiscoverAllPrimaryServices_c:
//            {
//                ShellGatt_DiscoveryHandler(serverDeviceId, procedureType);
//            }
//            break;
//
//            case gGattProcDiscoverPrimaryServicesByUuid_c:
//            {
//                uint8_t i = 0;
//                shell_write("\n\r--> Discovered primary services: ");
//                shell_writeDec(mcPrimaryServices);
//
//                while (i < mcPrimaryServices)
//                {
//                    ShellGatt_PrintIndexedService(i);
//                    i++;
//                }
//                ShellGatt_DiscoveryFinished();
//            }
//            break;
//
//            case gGattProcDiscoverCharacteristicByUuid_c:
//            {
//                uint8_t i = 0;
//                shell_write("\n\r--> Discovered characteristics: ");
//                shell_writeDec(mcCharacteristics);
//
//                while (i < mcCharacteristics)
//                {
//                    ShellGatt_PrintIndexedService(i);
//                    i++;
//                }
//                ShellGatt_DiscoveryFinished();
//            }
//            break;
//
//            case gGattProcReadCharacteristicValue_c:
//            {
//                shell_write("\n\r-->  GATT Event: Characteristic Value Read ");
//                shell_write("\n\r     Value: ");
//                shell_writeHexLe(mpCharBuffer->value.paValue, mpCharBuffer->value.valueLength);
//                shell_cmd_finished();
//
//                MEM_BufferFree(mpCharBuffer->value.paValue);
//                MEM_BufferFree(mpCharBuffer);
//                mpCharBuffer = NULL;
//            }
//            break;
//
//            case gGattProcWriteCharacteristicValue_c:
//            {
//                shell_write("\n\r-->  GATT Event: Characteristic Value Written!");
//                shell_cmd_finished();
//            }
//            break;
//
//            default:
//                shell_cmd_finished();
//                break;
//            }
//    }
//
//}
//
//
//
///*! *********************************************************************************
// * \brief        Handles GATT server callback from host stack.
// *
// * \param[in]    deviceId        Client peer device ID.
// * \param[in]    pServerEvent    Pointer to gattServerEvent_t.
// ********************************************************************************** */
//void ShellGatt_ServerCallback (
//                                       deviceId_t deviceId,
//                                       gattServerEvent_t* pServerEvent)
//{
//
//}
//
//void ShellGatt_NotificationCallback
//(
//    deviceId_t          serverDeviceId,
//    uint16_t            characteristicValueHandle,
//    uint8_t*            aValue,
//    uint16_t            valueLength
//)
//{
//    shell_write("\n\r-->  GATT Event: Received Notification ");
//    shell_write("\n\r     Handle: ");
//    shell_writeDec(characteristicValueHandle);
//    shell_write("\n\r     Value: ");
//    shell_writeHexLe(aValue, valueLength);
//    SHELL_NEWLINE();
//}
//
//void ShellGatt_IndicationCallback
//(
//    deviceId_t          serverDeviceId,
//    uint16_t            characteristicValueHandle,
//    uint8_t*            aValue,
//    uint16_t            valueLength
//)
//{
//    shell_write("\n\r-->  GATT Event: Received Indication ");
//    shell_write("\n\r     Handle: ");
//    shell_writeDec(characteristicValueHandle);
//    shell_write("\n\r     Value: ");
//    shell_writeHexLe(aValue, valueLength);
//    SHELL_NEWLINE();
//}

char *ShellGatt_GetServiceName(uint16_t uuid16)
{
    for (uint8_t i = 0; i < NumberOfElements(mGattServices); i++)
    {
        if (mGattServices[i].uuid == uuid16)
        {
            return mGattServices[i].name;
        }
    }

    return "N/A";
}

char *ShellGatt_GetCharacteristicName(uint16_t uuid16)
{
    for (uint8_t i = 0; i < NumberOfElements(mGattChars); i++)
    {
        if (mGattChars[i].uuid == uuid16)
        {
            return mGattChars[i].name;
        }
    }

    return "N/A";
}

char *ShellGatt_GetDescriptorName(uint16_t uuid16)
{
    for (uint8_t i = 0; i < NumberOfElements(mGattDescriptors); i++)
    {
        if (mGattDescriptors[i].uuid == uuid16)
        {
            return mGattDescriptors[i].name;
        }
    }

    return "N/A";
}

int8_t ShellGatt_Notify(uint8_t argc, char *argv[])
{
    GATTDBFindCccdHandleForCharValueHandleRequest_t req;

    if (argc != 1)
    {
        return CMD_RET_USAGE;
    }

    if (gPeerDeviceId == gInvalidDeviceId_c)
    {
        shell_write("\n\r-->  Please connect the node first...\n\r");
        return CMD_RET_FAILURE;
    }


    mNotifyHandle = atoi(argv[0]);
    /* Get handle of CCCD */
    req.CharValueHandle = mNotifyHandle;
    GATTDBFindCccdHandleForCharValueHandleRequest(&req, BLE_FSCI_IF);

    if (mDelayTimerID == gTmrInvalidTimerID_c)
    {
        mDelayTimerID = TMR_AllocateTimer();
    }

    TMR_StartSingleShotTimer(mDelayTimerID, 50, ShellGatt_NotifySm, (void *)gGetCccd_c);

    return CMD_RET_SUCCESS;
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/
static int8_t ShellGatt_Discover(uint8_t argc, char *argv[])
{
    if (gPeerDeviceId == gInvalidDeviceId_c)
    {
        shell_write("\n\r-->  Please connect the node first...");
        return CMD_RET_FAILURE;
    }

    /* Check if another procedure has finished */
    // if (!ShellGatt_CheckProcedureFinished())
    // {
    //     return CMD_RET_FAILURE;
    // }

    switch (argc)
    {
        case 1:
        {
            if (!strcmp((char *)argv[0], "-all"))
            {
                /* Allocate memory for Service Discovery */
                // mpServiceDiscoveryBuffer = MEM_BufferAlloc(sizeof(gattService_t) * mMaxServicesCount_d);
                // mpCharBuffer = MEM_BufferAlloc(sizeof(gattCharacteristic_t) * mMaxServiceCharCount_d);
                // mpCharDescriptorBuffer = MEM_BufferAlloc(sizeof(gattAttribute_t) * mMaxCharDescriptorsCount_d);

                // if (!mpServiceDiscoveryBuffer || !mpCharBuffer || !mpCharDescriptorBuffer)
                // {
                //     return CMD_RET_FAILURE;
                // }

                /* Start Service Discovery*/
                // GattClient_DiscoverAllPrimaryServices(
                //     gPeerDeviceId,
                //     mpServiceDiscoveryBuffer,
                //     mMaxServicesCount_d,
                //     &mcPrimaryServices);

                GATTClientDiscoverAllPrimaryServicesRequest_t req;
                req.DeviceId = gPeerDeviceId;
                req.MaxNbOfServices = mMaxServicesCount_d;
                GATTClientDiscoverAllPrimaryServicesRequest(&req, BLE_FSCI_IF);

                return CMD_RET_SUCCESS;
            }
        }
        break;

        case 2:
        {
            if (!strcmp((char *)argv[0], "-service"))
            {
                uint16_t uuid16 = strtoul(argv[1], NULL, 16);

                // /* Allocate memory for Service Discovery */
                // mpServiceDiscoveryBuffer = MEM_BufferAlloc(sizeof(gattService_t) * mMaxServicesCount_d);

                // if (!mpServiceDiscoveryBuffer)
                // {
                //     return CMD_RET_FAILURE;
                // }

                GATTClientDiscoverPrimaryServicesByUuidRequest_t req;
                req.DeviceId = gPeerDeviceId;
                req.UuidType = Uuid16Bits;
                UuidToArray(req.Uuid.Uuid16Bits, uuid16);
                req.MaxNbOfServices = mMaxServicesCount_d;

                GATTClientDiscoverPrimaryServicesByUuidRequest(&req, BLE_FSCI_IF);

                return CMD_RET_SUCCESS;
            }
        }
        break;

        default:
            break;
    }

    return CMD_RET_USAGE;
}

static int8_t ShellGatt_Read(uint8_t argc, char *argv[])
{
    GATTClientReadCharacteristicValueRequest_t req = { 0 };

    if (gPeerDeviceId == gInvalidDeviceId_c)
    {
        shell_write("\n\r-->  Please connect the node first...");
        return CMD_RET_FAILURE;
    }

    if (argc != 1)
    {
        return CMD_RET_USAGE;
    }

    req.DeviceId = gPeerDeviceId;
    req.Characteristic.Value.Handle = atoi(argv[0]);
    req.MaxReadBytes = 50;

    GATTClientReadCharacteristicValueRequest(&req, BLE_FSCI_IF);

    return CMD_RET_SUCCESS;
}

static int8_t ShellGatt_Write(uint8_t argc, char *argv[], bool_t fNoRsp)
{
    uint8_t length = 0;
    GATTClientWriteCharacteristicValueRequest_t req = { 0 };

    if (gPeerDeviceId == gInvalidDeviceId_c)
    {
        shell_write("\n\r-->  Please connect the node first...");
        return CMD_RET_FAILURE;
    }

    if (argc != 2)
    {
        return CMD_RET_USAGE;
    }

    length = BleApp_ParseHexValue(argv[1]);

    if (length > mMaxCharValueLength_d)
    {
        shell_write("\n\r-->  Variable length exceeds maximum!");
        return CMD_RET_FAILURE;
    }

    req.DeviceId = gPeerDeviceId;
    req.Characteristic.Value.Handle = atoi(argv[0]);
    req.Characteristic.Value.UuidType = Uuid16Bits;
    req.ValueLength = length;
    req.Value = (uint8_t *)argv[1];

    req.WithoutResponse = fNoRsp;
    req.SignedWrite = FALSE;
    req.ReliableLongCharWrites = FALSE;

    GATTClientWriteCharacteristicValueRequest(&req, BLE_FSCI_IF);
    GATTClientRegisterNotificationCallbackRequest(BLE_FSCI_IF);

    return CMD_RET_SUCCESS;
}

static int8_t ShellGatt_WriteCmd(uint8_t argc, char *argv[])
{
    return ShellGatt_Write(argc, argv, TRUE);
}

static int8_t ShellGatt_WriteRsp(uint8_t argc, char *argv[])
{
    return ShellGatt_Write(argc, argv, FALSE);
}

static void ShellGatt_NotifySm(void *param)
{
    if ((gattNotifyState_t)((uint32_t)param) == gGetCccd_c)
    {
        if (gCccdHandle == INVALID_HANDLE)
        {
//            shell_write("\r\n-->  No CCCD found!\r\n");
        }
        else  if (!bIndicate)
        {
            GAPCheckNotificationStatusRequest_t req;
            req.DeviceId = gPeerDeviceId;
            req.Handle = gCccdHandle;
            GAPCheckNotificationStatusRequest(&req, BLE_FSCI_IF);

            gCccdHandle = INVALID_HANDLE;

            // get to next state
            TMR_StartSingleShotTimer(mDelayTimerID, 50, ShellGatt_NotifySm, (void *)gGetNotificationStatus_c);
        }
        else
        {
            GAPCheckIndicationStatusRequest_t req;
            req.DeviceId = gPeerDeviceId;
            req.Handle = gCccdHandle;
            GAPCheckIndicationStatusRequest(&req, BLE_FSCI_IF);

            gCccdHandle = INVALID_HANDLE;

            // get to next state
            TMR_StartSingleShotTimer(mDelayTimerID, 50, ShellGatt_NotifySm, (void *)gGetNotificationStatus_c);
        }
    }

    else if ((gattNotifyState_t)((uint32_t)param) == gGetNotificationStatus_c)
    {
        if (gIsNotificationActive || gIsIndicationActive )
        {
            if (!bIndicate)
            {
                GATTServerSendNotificationRequest_t req;
                req.DeviceId = gPeerDeviceId;
                req.Handle = mNotifyHandle;
                GATTServerSendNotificationRequest(&req, BLE_FSCI_IF);
            }
            else
            {
                GATTServerSendIndicationRequest_t req;
                req.DeviceId = gPeerDeviceId;
                req.Handle = mNotifyHandle;
                GATTServerSendIndicationRequest(&req, BLE_FSCI_IF);

                bIndicate = FALSE;
            }

            gIsNotificationActive = FALSE;
        }
        else
        {
//            shell_write("\r\n-->  CCCD is not set!\r\n");
        }
    }
}

int8_t ShellGatt_Indicate(uint8_t argc, char *argv[])
{
    bIndicate = TRUE;
    return ShellGatt_Notify(argc, argv);
}

//static void ShellGatt_PrintIndexedService(uint8_t index)
//{
//    gattService_t        *pCurrentService = mpServiceDiscoveryBuffer + index;
//    uint8_t              i = 0;
//
//    SHELL_NEWLINE();
//    shell_write("\n\r  --> ");
//    shell_write(ShellGatt_GetServiceName(pCurrentService->uuid.uuid16));
//
//    shell_write("  Start Handle: ");
//    shell_writeDec(pCurrentService->startHandle);
//    shell_write("  End Handle: ");
//    shell_writeDec(pCurrentService->endHandle);
//
//    for(i=0; i < pCurrentService->cNumCharacteristics; i++)
//    {
//        ShellGatt_PrintIndexedCharacteristic(pCurrentService, i);
//    }
//}
//
//static void ShellGatt_PrintIndexedCharacteristic(gattService_t *pService, uint8_t index)
//{
//    uint8_t i;
//    gattCharacteristic_t *pCurrentChar =  pService->aCharacteristics + index;
//
//    shell_write("\n\r     - ");
//    shell_write(ShellGatt_GetCharacteristicName(pCurrentChar->value.uuid.uuid16));
//
//    shell_write("  Value Handle: ");
//    shell_writeDec(pCurrentChar->value.handle);
//
//
//    for(i=0; i < pCurrentChar->cNumDescriptors; i++)
//    {
//        shell_write("\n\r       - ");
//        shell_write(ShellGatt_GetDescriptorName(pCurrentChar->aDescriptors[i].uuid.uuid16));
//        shell_write("  Descriptor Handle: ");
//        shell_writeDec(pCurrentChar->aDescriptors[i].handle);
//    }
//}
//
//static void ShellGatt_DiscoveryHandler
//(
//    deviceId_t serverDeviceId,
//    gattProcedureType_t procedureType
//)
//{
//    switch (procedureType)
//    {
//        case gGattProcDiscoverAllPrimaryServices_c:
//        {
//            shell_write("\n\r--> Discovered primary services: ");
//            shell_writeDec(mcPrimaryServices);
//
//            /* We found at least one service. Move on to characteristic discovey */
//            if (mcPrimaryServices)
//            {
//                /* Start characteristic discovery with first service*/
//                mCurrentServiceInDiscoveryIndex = 0;
//                mCurrentCharInDiscoveryIndex = 0;
//
//                mpServiceDiscoveryBuffer->aCharacteristics = mpCharBuffer;
//
//                /* Start Characteristic Discovery for current service */
//                GattClient_DiscoverAllCharacteristicsOfService(
//                                            serverDeviceId,
//                                            mpServiceDiscoveryBuffer,
//                                            mMaxServiceCharCount_d);
//            }
//            else
//            {
//                shell_write("\n\r--> Found primary services: 0");
//                ShellGatt_DiscoveryFinished();
//            }
//        }
//        break;
//
//        case gGattProcDiscoverAllCharacteristicDescriptors_c:
//        {
//                /* Move on to the next characteristic */
//                mCurrentCharInDiscoveryIndex++;
//
//                /* Fallthrough */
//        }
//
//        case gGattProcDiscoverAllCharacteristics_c:
//        {
//            gattService_t        *pCurrentService = mpServiceDiscoveryBuffer + mCurrentServiceInDiscoveryIndex;
//            gattCharacteristic_t *pCurrentChar = pCurrentService->aCharacteristics + mCurrentCharInDiscoveryIndex;
//
//            /* Check if we finished with the current service */
//            if (mCurrentCharInDiscoveryIndex < pCurrentService->cNumCharacteristics)
//            {
//                /* Find next characteristic with descriptors*/
//                while (mCurrentCharInDiscoveryIndex < pCurrentService->cNumCharacteristics - 1)
//                {
//                    /* Check if we have handles available between adjacent characteristics */
//                    if (pCurrentChar->value.handle + 2 < (pCurrentChar + 1)->value.handle)
//                    {
//                        pCurrentChar->aDescriptors = mpCharDescriptorBuffer;
//                        GattClient_DiscoverAllCharacteristicDescriptors(serverDeviceId,
//                                                pCurrentChar,
//                                                (pCurrentChar + 1)->value.handle - 2,
//                                                mMaxCharDescriptorsCount_d);
//                        return;
//                    }
//
//                    mCurrentCharInDiscoveryIndex++;
//                    pCurrentChar = pCurrentService->aCharacteristics + mCurrentCharInDiscoveryIndex;
//                }
//
//                /* Made it to the last characteristic. Chack against service end handle*/
//                if (pCurrentChar->value.handle < pCurrentService->endHandle)
//                {
//                    pCurrentChar->aDescriptors = mpCharDescriptorBuffer;
//                    GattClient_DiscoverAllCharacteristicDescriptors(serverDeviceId,
//                                                pCurrentChar,
//                                                pCurrentService->endHandle,
//                                                mMaxCharDescriptorsCount_d);
//                     return;
//                }
//            }
//
//            ShellGatt_PrintIndexedService(mCurrentServiceInDiscoveryIndex);
//
//            /* Move on to the next service */
//            mCurrentServiceInDiscoveryIndex++;
//
//            /* Reset characteristic discovery */
//            mCurrentCharInDiscoveryIndex = 0;
//            FLib_MemSet(mpCharBuffer, 0, sizeof(gattCharacteristic_t) * mMaxServiceCharCount_d);
//
//            if (mCurrentServiceInDiscoveryIndex < mcPrimaryServices)
//            {
//                /* Allocate memory for Char Discovery */
//                (mpServiceDiscoveryBuffer + mCurrentServiceInDiscoveryIndex)->aCharacteristics = mpCharBuffer;
//
//                 /* Start Characteristic Discovery for current service */
//                GattClient_DiscoverAllCharacteristicsOfService(serverDeviceId,
//                                            mpServiceDiscoveryBuffer + mCurrentServiceInDiscoveryIndex,
//                                            mMaxServiceCharCount_d);
//            }
//            else
//            {
//                ShellGatt_DiscoveryFinished();
//            }
//        }
//        break;
//
//    default:
//      break;
//    }
//}

/*! *********************************************************************************
* @}
********************************************************************************** */

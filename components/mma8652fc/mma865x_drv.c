/*
 * The Clear BSD License
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
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

/**
 * @file  mma865x_drv.c
 * @brief The mma865x_drv.c file implements the mma865x functional interface.
 */

//-----------------------------------------------------------------------
// ISSDK Includes
//-----------------------------------------------------------------------
#include "mma865x_drv.h"

//-----------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------
int32_t MMA865x_I2C_Initialize(
    mma865x_i2c_sensorhandle_t *pSensorHandle, ARM_DRIVER_I2C *pBus, uint8_t index, uint16_t sAddress, uint8_t whoAmi)
{
    int32_t status;
    uint8_t reg;

    /*! Check the input parameters. */
    if ((pSensorHandle == NULL) || (pBus == NULL))
    {
        return SENSOR_ERROR_INVALID_PARAM;
    }

    pSensorHandle->deviceInfo.deviceInstance = index;
    pSensorHandle->deviceInfo.functionParam = NULL;
    pSensorHandle->deviceInfo.idleFunction = NULL;

    /*!  Read and store the device's WHO_AM_I.*/
    status = Register_I2C_Read(pBus, &pSensorHandle->deviceInfo, sAddress, MMA865x_WHO_AM_I, 1, &reg);
    if ((ARM_DRIVER_OK != status) || (whoAmi != reg))
    {
        pSensorHandle->isInitialized = false;
        return SENSOR_ERROR_INIT;
    }

    /*! Initialize the sensor handle. */
    pSensorHandle->pCommDrv = pBus;
    pSensorHandle->slaveAddress = sAddress;
    pSensorHandle->isInitialized = true;
    return SENSOR_ERROR_NONE;
}

void MMA865x_I2C_SetIdleTask(mma865x_i2c_sensorhandle_t *pSensorHandle,
                             registeridlefunction_t idleTask,
                             void *userParam)
{
    pSensorHandle->deviceInfo.functionParam = userParam;
    pSensorHandle->deviceInfo.idleFunction = idleTask;
}

int32_t MMA865x_I2C_Configure(mma865x_i2c_sensorhandle_t *pSensorHandle, const registerwritelist_t *pRegWriteList)
{
    int32_t status;

    /*! Validate for the correct handle and register write list.*/
    if ((pSensorHandle == NULL) || (pRegWriteList == NULL))
    {
        return SENSOR_ERROR_INVALID_PARAM;
    }

    /*! Check whether sensor handle is initialized before applying configuration.*/
    if (pSensorHandle->isInitialized != true)
    {
        return SENSOR_ERROR_INIT;
    }

    /* Put the device into standby mode so that configuration can be applied. */
    status =
        Register_I2C_Write(pSensorHandle->pCommDrv, &pSensorHandle->deviceInfo, pSensorHandle->slaveAddress,
                           MMA865x_CTRL_REG1, MMA865x_CTRL_REG1_ACTIVE_STANDBY, MMA865x_CTRL_REG1_ACTIVE_MASK, false);
    if (ARM_DRIVER_OK != status)
    {
        return SENSOR_ERROR_WRITE;
    }

    /*! Apply the Sensor Configuration based on the Register Write List */
    status = Sensor_I2C_Write(pSensorHandle->pCommDrv, &pSensorHandle->deviceInfo, pSensorHandle->slaveAddress,
                              pRegWriteList);
    if (ARM_DRIVER_OK != status)
    {
        return SENSOR_ERROR_WRITE;
    }

    /*! Put the device into active mode and ready for reading data.*/
    status =
        Register_I2C_Write(pSensorHandle->pCommDrv, &pSensorHandle->deviceInfo, pSensorHandle->slaveAddress,
                           MMA865x_CTRL_REG1, MMA865x_CTRL_REG1_ACTIVE_ACTIVATED, MMA865x_CTRL_REG1_ACTIVE_MASK, false);
    if (ARM_DRIVER_OK != status)
    {
        return SENSOR_ERROR_WRITE;
    }

    return SENSOR_ERROR_NONE;
}

int32_t MMA865x_I2C_ReadData(mma865x_i2c_sensorhandle_t *pSensorHandle,
                             const registerreadlist_t *pReadList,
                             uint8_t *pBuffer)
{
    int32_t status;

    /*! Validate for the correct handle and register read list.*/
    if ((pSensorHandle == NULL) || (pReadList == NULL) || (pBuffer == NULL))
    {
        return SENSOR_ERROR_INVALID_PARAM;
    }

    /*! Check whether sensor handle is initialized before reading sensor data.*/
    if (pSensorHandle->isInitialized != true)
    {
        return SENSOR_ERROR_INIT;
    }

    /*! Parse through the read list and read the data one by one. */
    status = Sensor_I2C_Read(pSensorHandle->pCommDrv, &pSensorHandle->deviceInfo, pSensorHandle->slaveAddress,
                             pReadList, pBuffer);
    if (ARM_DRIVER_OK != status)
    {
        return SENSOR_ERROR_READ;
    }

    return SENSOR_ERROR_NONE;
}

int32_t MMA865x_I2C_DeInit(mma865x_i2c_sensorhandle_t *pSensorHandle)
{
    int32_t status;

    if (pSensorHandle == NULL)
    {
        return SENSOR_ERROR_INVALID_PARAM;
    }

    /*! Check whether sensor handle is initialized before triggering sensor reset.*/
    if (pSensorHandle->isInitialized != true)
    {
        return SENSOR_ERROR_INIT;
    }

    /*! Trigger sensor device reset.*/
    status = Register_I2C_Write(pSensorHandle->pCommDrv, &pSensorHandle->deviceInfo, pSensorHandle->slaveAddress,
                                MMA865x_CTRL_REG2, MMA865x_CTRL_REG2_RST_EN, MMA865x_CTRL_REG2_RST_MASK, false);
    if (ARM_DRIVER_OK != status)
    {
        return SENSOR_ERROR_WRITE;
    }
    else
    {
        /*! De-initialize sensor handle. */
        pSensorHandle->isInitialized = false;
    }

    return SENSOR_ERROR_NONE;
}

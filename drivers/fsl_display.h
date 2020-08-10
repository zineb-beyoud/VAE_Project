/*
 * The Clear BSD License
 * Copyright (c) 2017, NXP Semiconductors, Inc.
 * All rights reserved.
 *
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 *  that the following conditions are met:
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

#ifndef _FSL_DISPLAY_H_
#define _FSL_DISPLAY_H_

#include "fsl_video_common.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief Display control flags. */
enum _display_flags
{
    kDISPLAY_VsyncActiveLow = 0U,                 /*!< VSYNC active low. */
    kDISPLAY_VsyncActiveHigh = (1U << 0U),        /*!< VSYNC active high. */
    kDISPLAY_HsyncActiveLow = 0U,                 /*!< HSYNC active low. */
    kDISPLAY_HsyncActiveHigh = (1U << 1U),        /*!< HSYNC active high. */
    kDISPLAY_DataEnableActiveHigh = 0U,           /*!< Data enable line active high. */
    kDISPLAY_DataEnableActiveLow = (1U << 2U),    /*!< Data enable line active low. */
    kDISPLAY_DataLatchOnRisingEdge = 0U,          /*!< Latch data on rising clock edge. */
    kDISPLAY_DataLatchOnFallingEdge = (1U << 3U), /*!< Latch data on falling clock edge. */
};

/*! @brief Display configuration. */
typedef struct _display_config
{
    uint32_t resolution;   /*!< Resolution, see @ref video_resolution_t and @ref FSL_VIDEO_RESOLUTION. */
    uint16_t hsw;          /*!< HSYNC pulse width. */
    uint16_t hfp;          /*!< Horizontal front porch. */
    uint16_t hbp;          /*!< Horizontal back porch. */
    uint16_t vsw;          /*!< VSYNC pulse width. */
    uint16_t vfp;          /*!< Vrtical front porch. */
    uint16_t vbp;          /*!< Vertical back porch. */
    uint32_t controlFlags; /*!< Control flags, OR'ed value of @ref _display_flags. */
    uint8_t dsiLanes;      /*!< MIPI DSI data lanes number. */
} display_config_t;

typedef struct _display_handle display_handle_t;

/*! @brief Display device operations. */
typedef struct _display_operations
{
    status_t (*init)(display_handle_t *handle, const display_config_t *config); /*!< Init the device. */
    status_t (*deinit)(display_handle_t *handle);                               /*!< Deinit the device. */
    status_t (*start)(display_handle_t *handle);                                /*!< Start the device. */
    status_t (*stop)(display_handle_t *handle);                                 /*!< Stop the device. */
} display_operations_t;

/*! @brief Display handle. */
struct _display_handle
{
    const void *resource;
    const display_operations_t *ops;
};

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Initializes the display device with user defined configuration.
 *
 * @param handle Display device handle.
 * @param config Pointer to the user-defined configuration structure.
 * @return Returns @ref kStatus_Success if initialize success, otherwise returns
 * error code.
 */
static inline status_t DISPLAY_Init(display_handle_t *handle, const display_config_t *config)
{
    return handle->ops->init(handle, config);
}

/*!
 * @brief Deinitialize the display device.
 *
 * @param handle Display device handle.
 * @return Returns @ref kStatus_Success if success, otherwise returns error code.
 */
static inline status_t DISPLAY_Deinit(display_handle_t *handle)
{
    return handle->ops->deinit(handle);
}

/*!
 * @brief Start the display device.
 *
 * @param handle Display device handle.
 * @return Returns @ref kStatus_Success if success, otherwise returns error code.
 */
static inline status_t DISPLAY_Start(display_handle_t *handle)
{
    return handle->ops->start(handle);
}

/*!
 * @brief Stop the display device.
 *
 * @param handle Display device handle.
 * @return Returns @ref kStatus_Success if success, otherwise returns error code.
 */
static inline status_t DISPLAY_Stop(display_handle_t *handle)
{
    return handle->ops->stop(handle);
}

#if defined(__cplusplus)
}
#endif

#endif /* _FSL_DISPLAY_H_ */

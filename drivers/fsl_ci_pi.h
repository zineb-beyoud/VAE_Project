/*
 * The Clear BSD License
 * Copyright 2017 NXP
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

#ifndef _FSL_CI_PI_H_
#define _FSL_CI_PI_H_

#include "fsl_common.h"

/*!
 * @addtogroup ci_pi
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @name Driver version */
/*@{*/
/*! @brief CI_PI driver version 2.0.0. */
#define FSL_CI_PI_DRIVER_VERSION (MAKE_VERSION(2, 0, 0))
/*@}*/

/*!
 * @brief CI_PI status flags.
 */
enum _ci_pi_flags
{
    kCI_PI_ChangeOfFieldFlag = CI_PI_CSI_STATUS_FIELD_TOGGLE_MASK, /*!< Change of field. */
    kCI_PI_EccErrorFlag = CI_PI_CSI_STATUS_ECC_ERROR_MASK,         /*!< ECC error detected. */
};

/*!
 * @brief Input data format.
 */
typedef enum _ci_pi_input_format
{
    kCI_PI_InputUYVY8888_8BitBus = 0x0,      /*!< UYVY, every component is 8bit, data bus is 8bit */
    kCI_PI_InputUYVY10101010_10BitBus = 0x1, /*!< UYVY, every component is 10bit, data bus is 10bit. */
    kCI_PI_InputRGB888_8BitBus = 0x2,        /*!< RGB, every component is 8bit, data bus is 8bit. */
    kCI_PI_InputBGR888_8BitBus = 0x3,        /*!< BGR, every component is 8bit, data bus is 8bit. */
    kCI_PI_InputRGB888_24BitBus = 0x4,       /*!< RGB, every component is 8bit, data bus is 24 bit. */
    kCI_PI_InputYVYU8888_8BitBus = 0x5,      /*!< YVYU, every component is 8bit, data bus is 8bit */
    kCI_PI_InputYUV888_8BitBus = 0x6,        /*!< YUV, every component is 8bit, data bus is 8bit. */
    kCI_PI_InputYVYU8888_16BitBus = 0x7,     /*!< YVYU, every component is 8bit, data bus 16bit. */
    kCI_PI_InputYUV888_24BitBus = 0x8,       /*!< YUV, every component is 8bit, data bus is 24bit. */
    kCI_PI_InputBayer8_8BitBus = 0x9,        /*!< Bayer 8bit */
    kCI_PI_InputBayer10_10BitBus = 0xa,      /*!< Bayer 10bit */
    kCI_PI_InputBayer12_12BitBus = 0xb,      /*!< Bayer 12bit */
    kCI_PI_InputBayer16_16BitBus = 0xc,      /*!< Bayer 16bit */
} ci_pi_input_format_t;

/*! @brief CI_PI signal polarity. */
enum _ci_pi_polarity_flags
{
    kCI_PI_HsyncActiveLow = 0U,                              /*!< HSYNC is active low. */
    kCI_PI_HsyncActiveHigh = CI_PI_CSI_CTRL0_HSYNC_POL_MASK, /*!< HSYNC is active high. */
    kCI_PI_DataLatchOnRisingEdge = 0,                        /*!< Pixel data latched at rising edge of pixel clock. */
    kCI_PI_DataLatchOnFallingEdge =
        CI_PI_CSI_CTRL0_PIXEL_CLK_POL_MASK,                   /*!< Pixel data latched at falling edge of pixel clock. */
    kCI_PI_DataEnableActiveHigh = 0U,                         /*!< Data enable signal (DE) is active high. */
    kCI_PI_DataEnableActiveLow = CI_PI_CSI_CTRL0_DE_POL_MASK, /*!< Data enable signal (DE) is active low. */
    kCI_PI_VsyncActiveHigh = CI_PI_CSI_CTRL0_VSYNC_POL_MASK,  /*!< VSYNC is active high. */
    kCI_PI_VsyncActiveLow = 0,                                /*!< VSYNC is active low. */
};

/*!
 * @brief CI_PI work mode.
 *
 * The CCIR656 interlace mode is not supported currently.
 */
typedef enum _ci_pi_work_mode
{
    kCI_PI_GatedClockMode = CI_PI_CSI_CTRL0_GCLK_MODE_EN_MASK, /*!< HSYNC, VSYNC, and PIXCLK signals are used. */
    kCI_PI_GatedClockDataEnableMode = CI_PI_CSI_CTRL0_GCLK_MODE_EN_MASK |
                                      CI_PI_CSI_CTRL0_VALID_SEL_MASK, /*!< DE, VSYNC, and PIXCLK signals are used. */
    kCI_PI_NonGatedClockMode = 0U,                                    /*!< VSYNC, and PIXCLK signals are used. */
    kCI_PI_CCIR656ProgressiveMode = CI_PI_CSI_CTRL0_CCIR_EN_MASK,     /*!< CCIR656 progressive mode. */
} ci_pi_work_mode_t;

typedef struct _ci_pi_config
{
    uint16_t width;
    uint16_t vsyncWidth;
    uint16_t hsyncWidth;
    uint32_t polarityFlags; /*!< Timing signal polarity flags, OR'ed value of @ref _ci_pi_polarity_flags. */
    uint8_t pixelLinkAddr;
    ci_pi_input_format_t inputFormat;

    ci_pi_work_mode_t workMode; /*!< Work mode. */
    bool useExtVsync;           /*!< In CCIR656 progressive mode, set true to use external VSYNC signal, set false
                                  to use internal VSYNC signal decoded from SOF. */
    bool swapUV;                /*!< Swap UV. */
} ci_pi_config_t;

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cIFusIFus)
extern "C" {
#endif

/*!
 * @brief Enables and configures the CI_PI peripheral module.
 *
 * @param base CI_PI peripheral address.
 * @param config CI_PI module configuration structure.
 */
void CI_PI_Init(CI_PI_Type *base, const ci_pi_config_t *config);

/*!
 * @brief Disables the CI_PI peripheral module.
 *
 * @param base CI_PI peripheral address.
 */
void CI_PI_Deinit(CI_PI_Type *base);

/*!
 * @brief Get the default configuration to initialize CI_PI.
 *
 * The default configuration value is:
 *
 * @code
    config->width = 0;
    config->vsyncWidth = 3U;
    config->hsyncWidth = 2U;
    config->polarityFlags = 0;
    config->pixelLinkAddr = 0;
    config->inputFormat = kCI_PI_InputUYVY8888_8BitBus;
    config->workMode = kCI_PI_NonGatedClockMode;
    config->useExtVsync = false;
    config->swapUV = false;
    @endcode
 *
 * @param config Pointer to the configuration.
 */
void CI_PI_GetDefaultConfig(ci_pi_config_t *config);

/*!
 * @brief Resets the CI_PI peripheral module.
 *
 * @param base CI_PI peripheral address.
 */
static inline void CI_PI_Reset(CI_PI_Type *base)
{
    uint32_t i = 0;

    base->CSI_CTRL0_SET = CI_PI_CSI_CTRL0_SOFTRST_MASK;

    i = 0x10;
    while (i--)
    {
        __NOP();
    }

    base->CSI_CTRL0_CLR = CI_PI_CSI_CTRL0_SOFTRST_MASK;
}

/*!
 * @brief Starts the CI_PI peripheral module to output captured frame.
 *
 * @param base CI_PI peripheral address.
 */
void CI_PI_Start(CI_PI_Type *base);

/*!
 * @brief Stops the CI_PI peripheral module.
 *
 * @param base CI_PI peripheral address.
 */
void CI_PI_Stop(CI_PI_Type *base);

/*!
 * @brief Gets the CI_PI peripheral module status.
 *
 * @param base CI_PI peripheral address.
 * @return Status returned as the OR'ed value of @ref _ci_pi_flags.
 */
static inline uint32_t CI_PI_GetStatus(CI_PI_Type *base)
{
    return base->CSI_STATUS;
}

#if defined(__cIFusIFus)
}
#endif
/*!
 * @}
 */
#endif /* _FSL_CI_PI_H_ */

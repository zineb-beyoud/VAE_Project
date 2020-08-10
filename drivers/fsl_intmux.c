/*
 * The Clear BSD License
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
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

#include "fsl_intmux.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.intmux"
#endif


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief Get instance number for INTMUX.
 *
 * @param base INTMUX peripheral base address.
 */
static uint32_t INTMUX_GetInstance(INTMUX_Type *base);

#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
/*!
 * @brief Handle INTMUX all channels IRQ.
 *
 * The handler reads the INTMUX channel's active vector register. This returns the offset
 * from the start of the vector table to the vector for the INTMUX channel's highest priority
 * pending source interrupt. After a check for spurious interrupts (an offset of 0), the
 * function address at the vector offset is read and jumped to.
 *
 * @param base INTMUX peripheral base address.
 * @param channel INTMUX channel number.
 */
static void INTMUX_CommonIRQHandler(INTMUX_Type *intmuxBase, uint32_t channel);
#endif

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*! @brief Array to map INTMUX instance number to base pointer. */
static INTMUX_Type *const s_intmuxBases[] = INTMUX_BASE_PTRS;

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/*! @brief Array to map INTMUX instance number to clock name. */
static const clock_ip_name_t s_intmuxClockName[] = INTMUX_CLOCKS;
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
/*! @brief Array to map INTMUX instance number to IRQ number. */
static const IRQn_Type s_intmuxIRQNumber[][FSL_FEATURE_INTMUX_CHANNEL_COUNT] = INTMUX_IRQS;
#endif /* FSL_FEATURE_INTMUX_DIRECTION_OUT */

/*******************************************************************************
 * Code
 ******************************************************************************/

static uint32_t INTMUX_GetInstance(INTMUX_Type *base)
{
    uint32_t instance;

    /* Find the instance index from base address mappings. */
    for (instance = 0; instance < ARRAY_SIZE(s_intmuxBases); instance++)
    {
        if (s_intmuxBases[instance] == base)
        {
            break;
        }
    }

    assert(instance < ARRAY_SIZE(s_intmuxBases));

    return instance;
}

#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
static void INTMUX_CommonIRQHandler(INTMUX_Type *intmuxBase, uint32_t channel)
{
    uint32_t pendingIrqOffset;

    pendingIrqOffset = intmuxBase->CHANNEL[channel].CHn_VEC;
    if (pendingIrqOffset)
    {
        uint32_t isr = *(uint32_t *)(SCB->VTOR + pendingIrqOffset);
        ((void (*)(void))isr)();
    }
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
      exception return operation might vector to incorrect interrupt */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}
#endif /* FSL_FEATURE_INTMUX_DIRECTION_OUT */

void INTMUX_Init(INTMUX_Type *base)
{
    uint32_t channel;
    uint32_t instance = INTMUX_GetInstance(base);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Enable clock gate. */
    CLOCK_EnableClock(s_intmuxClockName[instance]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
    /* Reset all channels and enable NVIC vectors for all INTMUX channels. */
    for (channel = 0; channel < FSL_FEATURE_INTMUX_CHANNEL_COUNT; channel++)
    {
        INTMUX_ResetChannel(base, channel);
#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
        NVIC_EnableIRQ(s_intmuxIRQNumber[instance][channel]);
#endif /* FSL_FEATURE_INTMUX_DIRECTION_OUT */
    }
}

void INTMUX_Deinit(INTMUX_Type *base)
{
    uint32_t channel;
    uint32_t instance = INTMUX_GetInstance(base);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Disable clock gate. */
    CLOCK_DisableClock(s_intmuxClockName[instance]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
    /* Disable NVIC vectors for all of the INTMUX channels. */
    for (channel = 0; channel < FSL_FEATURE_INTMUX_CHANNEL_COUNT; channel++)
    {
#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
        NVIC_DisableIRQ(s_intmuxIRQNumber[instance][channel]);
#endif /* FSL_FEATURE_INTMUX_DIRECTION_OUT */
    }
}

#if !(defined(FSL_FEATURE_INTMUX_DIRECTION_OUT) && FSL_FEATURE_INTMUX_DIRECTION_OUT)
#if defined(INTMUX0)
void INTMUX0_0_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 0);
}

void INTMUX0_1_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 1);
}

void INTMUX0_2_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 2);
}

void INTMUX0_3_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 3);
}

#if defined(FSL_FEATURE_INTMUX_CHANNEL_COUNT) && (FSL_FEATURE_INTMUX_CHANNEL_COUNT > 4U)
void INTMUX0_4_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 4);
}

void INTMUX0_5_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 5);
}

void INTMUX0_6_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 6);
}

void INTMUX0_7_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX0, 7);
}
#endif /* FSL_FEATURE_INTMUX_CHANNEL_COUNT */

#endif

#if defined(INTMUX1)
void INTMUX1_0_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 0);
}

void INTMUX1_1_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 1);
}

void INTMUX1_2_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 2);
}

void INTMUX1_3_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 3);
}

#if defined(FSL_FEATURE_INTMUX_CHANNEL_COUNT) && (FSL_FEATURE_INTMUX_CHANNEL_COUNT > 4U)
void INTMUX1_4_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 4);
}

void INTMUX1_5_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 5);
}

void INTMUX1_6_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 6);
}

void INTMUX1_7_DriverIRQHandler(void)
{
    INTMUX_CommonIRQHandler(INTMUX1, 7);
}
#endif /* FSL_FEATURE_INTMUX_CHANNEL_COUNT */
#endif /* INTMUX1 */
#endif /* FSL_FEATURE_INTMUX_DIRECTION_OUT */

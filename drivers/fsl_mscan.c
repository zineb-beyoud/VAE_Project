/*
 * The Clear BSD License
 * Copyright 2017 NXP
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

#include "fsl_mscan.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.mscan"
#endif


#define MSCAN_TIME_QUANTA_NUM (8)

/*! @brief MSCAN Internal State. */
enum _mscan_state
{
    kMSCAN_StateIdle = 0x0,     /*!< MB/RxFIFO idle.*/
    kMSCAN_StateRxData = 0x1,   /*!< MB receiving.*/
    kMSCAN_StateRxRemote = 0x2, /*!< MB receiving remote reply.*/
    kMSCAN_StateTxData = 0x3,   /*!< MB transmitting.*/
    kMSCAN_StateTxRemote = 0x4, /*!< MB transmitting remote request.*/
    kMSCAN_StateRxFifo = 0x5,   /*!< RxFIFO receiving.*/
};

/*! @brief MSCAN message buffer CODE for Rx buffers. */
enum _mscan_mb_code_rx
{
    kMSCAN_RxMbInactive = 0x0, /*!< MB is not active.*/
    kMSCAN_RxMbFull = 0x2,     /*!< MB is full.*/
    kMSCAN_RxMbEmpty = 0x4,    /*!< MB is active and empty.*/
    kMSCAN_RxMbOverrun = 0x6,  /*!< MB is overwritten into a full buffer.*/
    kMSCAN_RxMbBusy = 0x8,     /*!< FlexCAN is updating the contents of the MB.*/
                                 /*!  The CPU must not access the MB.*/
    kMSCAN_RxMbRanswer = 0xA,  /*!< A frame was configured to recognize a Remote Request Frame */
                                 /*!  and transmit a Response Frame in return.*/
    kMSCAN_RxMbNotUsed = 0xF,  /*!< Not used.*/
};

/*! @brief FlexCAN message buffer CODE FOR Tx buffers. */
enum _mscan_mb_code_tx
{
    kFLEXCAN_TxMbInactive = 0x8,     /*!< MB is not active.*/
    kFLEXCAN_TxMbAbort = 0x9,        /*!< MB is aborted.*/
    kFLEXCAN_TxMbDataOrRemote = 0xC, /*!< MB is a TX Data Frame(when MB RTR = 0) or */
                                     /*!< MB is a TX Remote Request Frame (when MB RTR = 1).*/
    kFLEXCAN_TxMbTanswer = 0xE,      /*!< MB is a TX Response Request Frame from */
                                     /*!  an incoming Remote Request Frame.*/
    kFLEXCAN_TxMbNotUsed = 0xF,      /*!< Not used.*/
};

/* Typedef for interrupt handler. */
typedef void (*mscan_isr_t)(MSCAN_Type *base, mscan_handle_t *handle);

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief Get the MsCAN instance from peripheral base address.
 *
 * @param base MsCAN peripheral base address.
 * @return MsCAN instance.
 */
static uint32_t MSCAN_GetInstance(MSCAN_Type *base);

/*!
 * @brief Enter MsCAN Initial Mode.
 *
 * This function makes the MsCAN work under Initial Mode.
 *
 * @param base MsCAN peripheral base address.
 */
static void MSCAN_EnterInitMode(MSCAN_Type *base);

/*!
 * @brief Exit MsCAN Initial Mode.
 *
 * This function makes the MsCAN leave Initial Mode.
 *
 * @param base MsCAN peripheral base address.
 */
static void MSCAN_ExitInitMode(MSCAN_Type *base);

/*!
 * @brief Set Baud Rate of MsCAN.
 *
 * This function set the baud rate of MsCAN.
 *
 * @param base MsCAN peripheral base address.
 * @param sourceClock_Hz Source Clock in Hz.
 * @param baudRate_Bps Baud Rate in Bps.
 */
static void MSCAN_SetBaudRate(MSCAN_Type *base, uint32_t sourceClock_Hz, uint32_t baudRate_Bps);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* Array of MsCAN peripheral base address. */
static MSCAN_Type *const s_mscanBases[] = MSCAN_BASE_PTRS;

/* Array of MSCAN IRQ number. */
static const IRQn_Type s_mscanRxWarningIRQ[] = MSCAN_RX_IRQS;
static const IRQn_Type s_mscanTxWarningIRQ[] = MSCAN_TX_IRQS;
static const IRQn_Type s_mscanWakeUpIRQ[] = MSCAN_WAKE_UP_IRQS;
static const IRQn_Type s_mscanErrorIRQ[] = MSCAN_ERR_IRQS;

/* Array of MsCAN handle. */
static mscan_handle_t *s_mscanHandle[ARRAY_SIZE(s_mscanBases)];

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/* Array of MsCAN clock name. */
static const clock_ip_name_t s_mscanClock[] = MSCAN_CLOCKS;
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

/* MsCAN ISR for transactional APIs. */
static mscan_isr_t s_mscanIsr;

/*******************************************************************************
 * Code
 ******************************************************************************/

static uint32_t MSCAN_GetInstance(MSCAN_Type *base)
{
    uint32_t instance;

    /* Find the instance index from base address mappings. */
    for (instance = 0; instance < ARRAY_SIZE(s_mscanBases); instance++)
    {
        if (s_mscanBases[instance] == base)
        {
            break;
        }
    }

    assert(instance < ARRAY_SIZE(s_mscanBases));

    return instance;
}

static void MSCAN_EnterInitMode(MSCAN_Type *base)
{
    /* Set initial request bit. */
    base->CANCTL0 |= MSCAN_CANCTL0_INITRQ_MASK;

    /* Wait until the MsCAN Module enters initial mode. */
    while (!(base->CANCTL1 & MSCAN_CANCTL1_INITAK_MASK))
    {
    }
}

static void MSCAN_ExitInitMode(MSCAN_Type *base)
{
    /* Clear initial request bit. */
    base->CANCTL0 &= ~MSCAN_CANCTL0_INITRQ_MASK;

    /* Wait until the MsCAN Module exits initial mode. */
    while (base->CANCTL1 & MSCAN_CANCTL1_INITAK_MASK)
    {
    }
}

static void MSCAN_SetBaudRate(MSCAN_Type *base, uint32_t sourceClock_Hz, uint32_t baudRate_Bps)
{
    mscan_timing_config_t timingConfig;
    uint32_t priDiv = baudRate_Bps * MSCAN_TIME_QUANTA_NUM;

    /* Assertion: Desired baud rate is too high. */
    assert(baudRate_Bps <= 1000000U);
    /* Assertion: Source clock should greater than baud rate * MSCAN_TIME_QUANTA_NUM. */
    assert(priDiv <= sourceClock_Hz);

    if (0 == priDiv)
    {
        priDiv = 1;
    }

    priDiv = (sourceClock_Hz / priDiv) - 1;

    /* Desired baud rate is too low. */
    if (priDiv > 0x3F)
    {
        priDiv = 0x3F;
    }

    /* MsCAN timing setting formula:
     * MSCAN_TIME_QUANTA_NUM = 1 + (TSEG1 + 1) + (TSEG2 + 1);
     */
    timingConfig.priDiv = priDiv;
    timingConfig.timeSeg1 = 3;
    timingConfig.timeSeg2 = 2;
    timingConfig.samp = 0;
    timingConfig.sJumpwidth = 0;

    /* Update actual timing characteristic. */
    MSCAN_SetTimingConfig(base, &timingConfig);
}

void MSCAN_Init(MSCAN_Type *base, const mscan_config_t *config, uint32_t sourceClock_Hz)
{
    uint8_t ctl0Temp, ctl1Temp;
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    uint32_t instance;
#endif

    /* Assertion. */
    assert(config);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    instance = MSCAN_GetInstance(base);
    /* Enable MsCAN clock. */
    CLOCK_EnableClock(s_mscanClock[instance]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

    /* Enable MsCAN Module for configuartion. */
    MSCAN_Enable(base, true);

    /* Enter initialization mode for MSCAN configuration. */
    MSCAN_EnterInitMode(base);

    ctl0Temp = base->CANCTL0;
    ctl1Temp = base->CANCTL1;
    /* Enable Timer? */
    ctl0Temp = (config->enableTimer) ? ctl0Temp | MSCAN_CANCTL0_TIME_MASK : ctl0Temp & ~MSCAN_CANCTL0_TIME_MASK;
    /* Enable Self Wake Up Mode? */
    ctl0Temp = (config->enableWakeup) ? ctl0Temp | MSCAN_CANCTL0_WUPE_MASK : ctl0Temp & ~MSCAN_CANCTL0_WUPE_MASK;
    /* Enable Loop Back Mode? */
    ctl1Temp = (config->enableLoopBack) ? ctl1Temp | MSCAN_CANCTL1_LOOPB_MASK : ctl1Temp & ~MSCAN_CANCTL1_LOOPB_MASK;
    /* Enable Listen Mode? */
    ctl1Temp = (config->enableListen) ? ctl1Temp | MSCAN_CANCTL1_LISTEN_MASK : ctl1Temp & ~MSCAN_CANCTL1_LISTEN_MASK;
    /* Clock source selection? */
    ctl1Temp = (config->clkSrc) ? ctl1Temp | MSCAN_CANCTL1_CLKSRC_MASK : ctl1Temp & ~MSCAN_CANCTL1_CLKSRC_MASK;

    /* Save CTLx Configuation. */
    base->CANCTL0 = ctl0Temp;
    base->CANCTL1 = ctl1Temp;

    /* Configure ID acceptance filter setting. */
    MSCAN_SetIDFilterMode(base, config->filterConfig.filterMode);
    MSCAN_WriteIDAR0(base, (uint8_t *)&(config->filterConfig.u32IDAR0));
    MSCAN_WriteIDMR0(base, (uint8_t *)&(config->filterConfig.u32IDMR0));
    MSCAN_WriteIDAR1(base, (uint8_t *)&(config->filterConfig.u32IDAR1));
    MSCAN_WriteIDMR1(base, (uint8_t *)&(config->filterConfig.u32IDMR1));

    /* Baud Rate Configuration.*/
    MSCAN_SetBaudRate(base, sourceClock_Hz, config->baudRate);

    /* Enter normal mode. */
    MSCAN_ExitInitMode(base);
}

void MSCAN_Deinit(MSCAN_Type *base)
{
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    uint32_t instance;
#endif

    /* Disable MsCAN module. */
    MSCAN_Enable(base, false);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    instance = MSCAN_GetInstance(base);
    /* Disable MsCAN clock. */
    CLOCK_DisableClock(s_mscanClock[instance]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
}

void MSCAN_GetDefaultConfig(mscan_config_t *config)
{
    /* Assertion. */
    assert(config);

    /* Initialize MsCAN Module config struct with default value. */
    config->baudRate = 1000000U;
    config->enableTimer = false;
    config->enableWakeup = false;
    config->clkSrc = kMSCAN_ClkSrcOsc;
    config->enableLoopBack = false;
    config->enableListen = false;
    config->busoffrecMode = kMSCAN_BusoffrecAuto;
    config->filterConfig.filterMode = kMSCAN_Filter32Bit;
}

void MSCAN_SetTimingConfig(MSCAN_Type *base, const mscan_timing_config_t *config)
{
    /* Assertion. */
    assert(config);

    /* Enter Inialization Mode. */
    MSCAN_EnterInitMode(base);

    /* Cleaning previous Timing Setting. */
    base->CANBTR0 &= ~(MSCAN_CANBTR0_BRP_MASK | MSCAN_CANBTR0_SJW_MASK);
    base->CANBTR1 &= ~(MSCAN_CANBTR1_TSEG1_MASK | MSCAN_CANBTR1_TSEG2_MASK | MSCAN_CANBTR1_SAMP_MASK);

    /* Updating Timing Setting according to configuration structure. */
    base->CANBTR0 |= (MSCAN_CANBTR0_BRP(config->priDiv) | MSCAN_CANBTR0_SJW(config->sJumpwidth));
    base->CANBTR1 |= (MSCAN_CANBTR1_TSEG1(config->timeSeg1) | MSCAN_CANBTR1_TSEG2(config->timeSeg2) | MSCAN_CANBTR1_SAMP(config->samp));

    /* Exit Inialization Mode. */
    MSCAN_ExitInitMode(base);
}

status_t MSCAN_WriteTxMb(MSCAN_Type *base, mscan_frame_t *txFrame)
{
    uint8_t txEmptyFlag;
    mscan_mb_t mb;
    IDR1_3_UNION sIDR1, sIDR3;
    /* Write IDR. */
    if (txFrame->format)
    {
        /* Deal with Extended frame. */
        sIDR1.IDR1.EID20_18_OR_SID2_0 = txFrame->ID_Type.ExtID.EID20_18;
        sIDR1.IDR1.R_TSRR = 1;
        sIDR1.IDR1.R_TEIDE = 1;
        sIDR1.IDR1.EID17_15 = txFrame->ID_Type.ExtID.EID17_15;
        sIDR3.IDR3.EID6_0 = txFrame->ID_Type.ExtID.EID6_0;
        sIDR3.IDR3.ERTR = (txFrame->type) ? 1 : 0;
        /* Write into MB structure. */
        mb.EIDR0 = txFrame->ID_Type.ExtID.EID28_21;
        mb.EIDR1 = sIDR1.Bytes;
        mb.EIDR2 = txFrame->ID_Type.ExtID.EID14_7;
        mb.EIDR3 = sIDR3.Bytes;
    }
    else
    {
        /* Deal with Standard frame. */
        sIDR1.IDR1.EID20_18_OR_SID2_0 = txFrame->ID_Type.ExtID.EID20_18;
        sIDR1.IDR1.R_TSRR = 0;
        sIDR1.IDR1.R_TEIDE = 0;
        sIDR1.IDR1.EID17_15 = txFrame->ID_Type.ExtID.EID17_15;
        /* Write into MB structure. */
        mb.EIDR0 = txFrame->ID_Type.ExtID.EID28_21;
        mb.EIDR1 = sIDR1.Bytes;
    }
    /* Write DLR, BPR */
    mb.DLR = txFrame->DLR;
    mb.BPR = txFrame->BPR;
    /* Write DSR */
    uint8_t i = 0;
    for (i=0; i<mb.DLR; i++)
    {
        mb.EDSR[i] = txFrame->DSR[i];
    }

    /* 1.Read TFLG to get the empty transmitter buffers. */
    txEmptyFlag = MSCAN_GetTxBufferEmptyFlag(base);
    if (kMSCAN_TxBufFull == txEmptyFlag)
    {
        return kStatus_Fail;
    }

    /* 2.Write TFLG value back. */
    MSCAN_TxBufferSelect(base, txEmptyFlag);
    /* Ex-copy time stamp. */
    memcpy((void *)&base->TEIDR0, (void *)&mb, sizeof(mscan_mb_t)-2);

    /* 3.Read TBSEL again to get lowest tx buffer, then write 1 to clear
    the corresponding bit to schedule transmission. */
    MSCAN_TxBufferLaunch(base, MSCAN_GetTxBufferSelect(base));

    return kStatus_Success;
}

status_t MSCAN_ReadRxMb(MSCAN_Type *base, mscan_frame_t *rxFrame)
{
    IDR1_3_UNION sIDR1;
    IDR1_3_UNION sIDR3;
    uint8_t i;
    if (MSCAN_GetRxBufferFullFlag(base))
    {
        sIDR1.Bytes = MSCAN_ReadRIDR1(base);
        sIDR3.Bytes = MSCAN_ReadRIDR3(base);
        rxFrame->format = (mscan_frame_format_t)(sIDR1.IDR1.R_TEIDE);
        rxFrame->type = (mscan_frame_type_t)(sIDR3.IDR3.ERTR);
        rxFrame->ID_Type.ExtID.EID28_21 = MSCAN_ReadRIDR0(base);
        rxFrame->ID_Type.ExtID.EID20_18 = sIDR1.IDR1.EID20_18_OR_SID2_0;
        rxFrame->ID_Type.ExtID.EID17_15 = sIDR1.IDR1.EID17_15;
        rxFrame->ID_Type.ExtID.EID14_7 = MSCAN_ReadRIDR2(base);
        rxFrame->ID_Type.ExtID.EID6_0 = sIDR3.IDR3.EID6_0;

        rxFrame->DLR = base->RDLR & 0x0F;
        for(i=0; i<rxFrame->DLR; i++)
        {
            rxFrame->DSR[i] = base->REDSR[i];
        }

        rxFrame->TSRH = base->RTSRH;
        rxFrame->TSRL = base->RTSRL;

        return kStatus_Success;
    }
    else
    {
        return kStatus_Fail;
    }
}

status_t MSCAN_TransferSendBlocking(MSCAN_Type *base, mscan_frame_t *txFrame)
{
    /* Write Tx Message Buffer to initiate a data sending. */
    if (kStatus_Success == MSCAN_WriteTxMb(base, txFrame))
    {
        /* Wait until CAN Message send out. */
        while (!MSCAN_GetTxBufferStatusFlags(base, MSCAN_GetTxBufferSelect(base)))
        {
        }

        return kStatus_Success;
    }
    else
    {
        return kStatus_Fail;
    }
}

status_t MSCAN_TransferReceiveBlocking(MSCAN_Type *base, mscan_frame_t *rxFrame)
{
    /* Wait until a new message is available in Rx Message Buffer. */
    while (!MSCAN_GetRxBufferFullFlag(base))
    {
    }

    /* Read Received CAN Message. */
    if (kStatus_Success == MSCAN_ReadRxMb(base, rxFrame))
    {
        /* Clear RXF flag to release the buffer. */
        MSCAN_ClearRxBufferFullFlag(base);
        return kStatus_Success;
    }
    else
    {
        return kStatus_Fail;
    }
}

void MSCAN_TransferCreateHandle(MSCAN_Type *base,
                                  mscan_handle_t *handle,
                                  mscan_transfer_callback_t callback,
                                  void *userData)
{
    assert(handle);

    uint8_t instance;

    /* Clean MSCAN transfer handle. */
    memset(handle, 0, sizeof(*handle));

    /* Get instance from peripheral base address. */
    instance = MSCAN_GetInstance(base);

    /* Save the context in global variables to support the double weak mechanism. */
    s_mscanHandle[instance] = handle;

    /* Register Callback function. */
    handle->callback = callback;
    handle->userData = userData;

    s_mscanIsr = MSCAN_TransferHandleIRQ;

    /* We Enable Error & Status interrupt here, because this interrupt just
     * report current status of MSCAN module through Callback function.
     * It is insignificance without a available callback function.
     */
    if (handle->callback != NULL)
    {
        MSCAN_EnableRxInterrupts(base, kMSCAN_StatusChangeInterruptEnable | kMSCAN_WakeUpInterruptEnable);
    }
    else
    {
        MSCAN_DisableRxInterrupts(base, kMSCAN_StatusChangeInterruptEnable | kMSCAN_WakeUpInterruptEnable);
    }

    /* Enable interrupts in NVIC. */
    EnableIRQ((IRQn_Type)(s_mscanRxWarningIRQ[instance]));
    EnableIRQ((IRQn_Type)(s_mscanTxWarningIRQ[instance]));
    EnableIRQ((IRQn_Type)(s_mscanWakeUpIRQ[instance]));
    EnableIRQ((IRQn_Type)(s_mscanErrorIRQ[instance]));
}

status_t MSCAN_TransferSendNonBlocking(MSCAN_Type *base, mscan_handle_t *handle, mscan_mb_transfer_t *xfer)
{
    /* Assertion. */
    assert(handle);
    assert(xfer);

    /* Check if Message Buffer is idle. */
    
    /* Distinguish transmit type. */
    if (kMSCAN_FrameTypeRemote == xfer->frame->type)
    {
        handle->mbStateTx = kMSCAN_StateTxRemote;

        /* Register user Frame buffer to receive remote Frame. */
        handle->mbFrameBuf = xfer->frame;
    }
    else
    {
        handle->mbStateTx = kMSCAN_StateTxData;
    }

    if (kStatus_Success == MSCAN_WriteTxMb(base, xfer->frame))
    {
        /* Enable Message Buffer Interrupt. */
        MSCAN_EnableTxInterrupts(base, xfer->mask);

        return kStatus_Success;
    }
    else
    {
        handle->mbStateTx = kMSCAN_StateIdle;
        return kStatus_Fail;
    }
}

status_t MSCAN_TransferReceiveNonBlocking(MSCAN_Type *base, mscan_handle_t *handle, mscan_mb_transfer_t *xfer)
{
    /* Assertion. */
    assert(handle);
    assert(xfer);

    /* Check if Message Buffer is idle. */
    if (kMSCAN_StateIdle == handle->mbStateRx)
    {
        handle->mbStateRx = kMSCAN_StateRxData;

        /* Register Message Buffer. */
        handle->mbFrameBuf = xfer->frame;

        /* Enable Message Buffer Interrupt. */
        MSCAN_EnableRxInterrupts(base, xfer->mask);

        return kStatus_Success;
    }
    else
    {
        return kStatus_MSCAN_RxBusy;
    }
}

void MSCAN_TransferAbortSend(MSCAN_Type *base, mscan_handle_t *handle, uint8_t mask)
{
    /* Assertion. */
    assert(handle);

    /* Abort Tx request. */
    MSCAN_AbortTxRequest(base, mask);

    /* Clean Message Buffer. */
    MSCAN_DisableTxInterrupts(base, mask);

    handle->mbStateTx = kMSCAN_StateIdle;
}

void MSCAN_TransferAbortReceive(MSCAN_Type *base, mscan_handle_t *handle, uint8_t mask)
{
    /* Assertion. */
    assert(handle);

    /* Disable Message Buffer Interrupt. */
    MSCAN_DisableRxInterrupts(base, mask);

    /* Un-register handle. */
    handle->mbStateRx = kMSCAN_StateIdle;
}

void MSCAN_TransferHandleIRQ(MSCAN_Type *base, mscan_handle_t *handle)
{
    /* Assertion. */
    assert(handle);

    status_t status = kStatus_MSCAN_UnHandled;
    
    /* Get current State of Message Buffer. */
    if (MSCAN_GetRxBufferFullFlag(base))
    {
        switch (handle->mbStateRx)
        {
            /* Solve Rx Data Frame. */
            case kMSCAN_StateRxData:
                status = MSCAN_ReadRxMb(base, handle->mbFrameBuf);
                if (kStatus_Success == status)
                {
                    status = kStatus_MSCAN_RxIdle;
                }
                MSCAN_TransferAbortReceive(base, handle, kMSCAN_RxFullInterruptEnable);
                break;

            /* Solve Rx Remote Frame. */
            case kMSCAN_StateRxRemote:
                status = MSCAN_ReadRxMb(base, handle->mbFrameBuf);
                if (kStatus_Success == status)
                {
                    status = kStatus_MSCAN_RxIdle;
                }
                MSCAN_TransferAbortReceive(base, handle, kMSCAN_RxFullInterruptEnable);
                break;
        }
        MSCAN_ClearRxBufferFullFlag(base);
    }
    else
    {
        switch (handle->mbStateTx)
        {
            /* Solve Tx Data Frame. */
            case kMSCAN_StateTxData:
                status = kStatus_MSCAN_TxIdle;
                MSCAN_TransferAbortSend(base, handle, kMSCAN_TxEmptyInterruptEnable);
                break;

            /* Solve Tx Remote Frame. */
            case kMSCAN_StateTxRemote:
                handle->mbStateRx = kMSCAN_StateRxRemote;
                status = kStatus_MSCAN_TxSwitchToRx;
                break;

            default:
                status = kStatus_MSCAN_UnHandled;
                break;
        }
    }

    handle->callback(base, handle, status, handle->userData);
}

#if defined(MSCAN)
void MSCAN_DriverIRQHandler(void)
{
    assert(s_mscanHandle[0]);

    s_mscanIsr(MSCAN, s_mscanHandle[0]);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
      exception return operation might vector to incorrect interrupt */
#if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
#endif
}
#endif

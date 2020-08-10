/*
 * The Clear BSD License
 * Copyright (c) 2017, NXP
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

#include <string.h>

#include <srtm_heap.h>

#include "srtm_dispatcher.h"
#include "srtm_dispatcher_struct.h"
#include "srtm_peercore.h"
#include "srtm_peercore_struct.h"
#include "srtm_service.h"
#include "srtm_service_struct.h"
#include "srtm_channel.h"
#include "srtm_channel_struct.h"
#include "srtm_message.h"
#include "srtm_message_struct.h"

#include "fsl_common.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
static void SRTM_DumpMessage(srtm_message_t msg)
{
#ifdef SRTM_DEBUG_MESSAGE_FUNC
    srtm_packet_head_t *head;

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG,
                       "  -- Msg type %d, dir %d, dlen %d, chan 0x%08x, err %d, prio %d\r\n",
                       msg->type, msg->direct, msg->dataLen, msg->channel, msg->error, msg->priority);
    if (msg->dataLen >= sizeof(struct _srtm_packet_head))
    {
        head = (srtm_packet_head_t *)msg->data;
        SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG,
                           "  -- Data cat %d, majv %d, minv %d, type %d, cmd %d, prio %d\r\n",
                           head->category, head->majorVersion, head->minorVersion, head->type,
                           head->command, head->priority);
    }
    else if (msg->type == SRTM_MessageTypeProcedure)
    {
        SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "  -- Callback 0x%08x, param 0x%x, 0x%x\r\n",
                           msg->procMsg.cb, msg->procMsg.param1, msg->procMsg.param2);
    }
#endif
}

static void SRTM_Dispatcher_InsertOrderedMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_list_t *list;
    srtm_message_t message;

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    SRTM_DumpMessage(msg);
    /* Insert message with priority order */
    for (list = disp->messageQ.prev; list != &disp->messageQ; list = list->prev)
    {
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        if (message->priority >= msg->priority)
        {
            break;
        }
    }
    SRTM_List_InsertAfter(list, &msg->node);
}

/* Send message to messageQ from ISR or task context */
static void SRTM_Dispatcher_QueueMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    uint32_t primask;

    assert(SRTM_List_IsEmpty(&msg->node));

    primask = DisableGlobalIRQ();
    SRTM_Dispatcher_InsertOrderedMessage(disp, msg);
    EnableGlobalIRQ(primask);

    SRTM_Sem_Post(disp->queueSig);
}

/* Dequeue message might detach message from messageQ, peer core's pendingQ or waitingReqs */
static srtm_status_t SRTM_Dispatcher_DequeueMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    uint32_t primask;
    srtm_status_t status = SRTM_Status_ListRemoveFailed;

    SRTM_Mutex_Lock(disp->mutex); /* Protect waitingReqs */
    primask = DisableGlobalIRQ(); /* Protect messageQ */
    if (!SRTM_List_IsEmpty(&msg->node))
    {
        SRTM_List_Remove(&msg->node);
        status = SRTM_Status_Success;
    }
    EnableGlobalIRQ(primask);
    SRTM_Mutex_Unlock(disp->mutex);

    return status;
}

static srtm_message_t SRTM_Dispatcher_RecvMessage(srtm_dispatcher_t disp)
{
    uint32_t primask;
    srtm_list_t *list;
    srtm_message_t message = NULL;

    primask = DisableGlobalIRQ();
    if (!SRTM_List_IsEmpty(&disp->messageQ))
    {
        list = disp->messageQ.next;
        SRTM_List_Remove(list);
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
    }
    EnableGlobalIRQ(primask);

    return message;
}

static void SRTM_Dispatcher_FreeRequest(srtm_dispatcher_t disp, srtm_request_t req, srtm_status_t error)
{
    req->error = error;
    if (req->reqMsg.isSyncReq)
    {
        /* Synchronous request will be freed by user. Here just wakeup user's task. */
        SRTM_Sem_Post(req->reqMsg.sync.sig);
    }
    else
    {
        if (req->reqMsg.async.cb)
        {
            req->reqMsg.async.cb(disp, req, NULL, req->reqMsg.async.param);
        }
        SRTM_Request_Destroy(req);
    }
}

static void SRTM_Dispatcher_FreeMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    switch (msg->type)
    {
        case SRTM_MessageTypeProcedure:
            SRTM_Procedure_Destroy(msg);
            break;
        case SRTM_MessageTypeRawData:
            SRTM_RawData_Destroy(msg);
            break;
        case SRTM_MessageTypeRequest:
            if (msg->direct == SRTM_MessageDirectRx)
            {
                /* Rx request will be freed after handling */
                SRTM_Request_Destroy(msg);
            }
            else
            {
                /* Tx request finalization need to inform user and then free. */
                SRTM_Dispatcher_FreeRequest(disp, msg, SRTM_Status_TransferNotAvail);
            }
            break;
        case SRTM_MessageTypeResponse:
            SRTM_Response_Destroy(msg);
            break;
        case SRTM_MessageTypeNotification:
            SRTM_Notification_Destroy(msg);
            break;
        default:
            SRTM_Message_Destroy(msg);
            break;
    }
}

static void SRTM_Dispatcher_RecycleMessage(srtm_message_t msg, void *param)
{
    uint32_t primask;
    srtm_dispatcher_t disp = (srtm_dispatcher_t)param;

    /* Put RX message back to freeRxMsgs */
    primask = DisableGlobalIRQ();
    SRTM_List_AddTail(&disp->freeRxMsgs, &msg->node);
    EnableGlobalIRQ(primask);
}

srtm_dispatcher_t SRTM_Dispatcher_Create(void)
{
    srtm_dispatcher_t disp = (srtm_dispatcher_t)SRTM_Heap_Malloc(sizeof(struct _srtm_dispatcher));
    srtm_mutex_t mutex = SRTM_Mutex_Create();
    srtm_sem_t startSig = SRTM_Sem_Create(1U, 0U);
    srtm_sem_t stopSig = SRTM_Sem_Create(1U, 0U);
    /* Assume same maximum message number of local and remote in messageQ */
    srtm_sem_t queueSig = SRTM_Sem_Create(SRTM_DISPATCHER_CONFIG_RX_MSG_NUMBER * 2, 0U);
    srtm_message_t msg;
    uint32_t i;

    assert(disp && mutex && startSig && stopSig && queueSig);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s\r\n", __func__);

    SRTM_List_Init(&disp->cores);
    SRTM_List_Init(&disp->services);
    SRTM_List_Init(&disp->freeRxMsgs);
    SRTM_List_Init(&disp->messageQ);
    SRTM_List_Init(&disp->waitingReqs);
    disp->mutex = mutex;
    disp->stopReq = false;
    disp->started = false;
    disp->startSig = startSig;
    disp->stopSig = stopSig;
    disp->queueSig = queueSig;

    for (i = 0; i < SRTM_DISPATCHER_CONFIG_RX_MSG_NUMBER; i++)
    {
        msg = SRTM_Message_Create(SRTM_DISPATCHER_CONFIG_RX_MSG_MAX_LEN);
        assert(msg);
        SRTM_Message_SetFreeFunc(msg, SRTM_Dispatcher_RecycleMessage, disp);
        SRTM_List_AddTail(&disp->freeRxMsgs, &msg->node);
    }

    return disp;
}

void SRTM_Dispatcher_Destroy(srtm_dispatcher_t disp)
{
    srtm_list_t *list;
    srtm_peercore_t core;
    srtm_service_t service;
    srtm_message_t msg;

    assert(disp);
    assert(!disp->started);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s\r\n", __func__);

    /* Before destroy, all the messages should be well handled */
    assert(SRTM_List_IsEmpty(&disp->messageQ));
    /* Before destroy, all the waiting request should responded */
    assert(SRTM_List_IsEmpty(&disp->waitingReqs));

    while(!SRTM_List_IsEmpty(&disp->cores))
    {
        list = disp->cores.next;
        SRTM_List_Remove(list);
        core = SRTM_LIST_OBJ(srtm_peercore_t, node, list);
        SRTM_PeerCore_Destroy(core);
    }

    while(!SRTM_List_IsEmpty(&disp->services))
    {
        list = disp->services.next;
        SRTM_List_Remove(list);
        service = SRTM_LIST_OBJ(srtm_service_t, node, list);
        SRTM_Service_Destroy(service);
    }

    while (!SRTM_List_IsEmpty(&disp->freeRxMsgs))
    {
        list = disp->freeRxMsgs.next;
        SRTM_List_Remove(list);
        msg = SRTM_LIST_OBJ(srtm_message_t, node, list);
        SRTM_Message_SetFreeFunc(msg, NULL, NULL);
        SRTM_Message_Destroy(msg);
    }

    SRTM_Mutex_Destroy(disp->mutex);
    SRTM_Sem_Destroy(disp->startSig);
    SRTM_Sem_Destroy(disp->stopSig);
    SRTM_Sem_Destroy(disp->queueSig);
    SRTM_Heap_Free(disp);
}

srtm_status_t SRTM_Dispatcher_Start(srtm_dispatcher_t disp)
{
    assert(disp);

    if (disp->started)
    {
        return SRTM_Status_InvalidState;
    }

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s\r\n", __func__);

    disp->stopReq = false;
    disp->started = true;
    SRTM_Sem_Post(disp->startSig);

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_Stop(srtm_dispatcher_t disp)
{
    if (!disp->started)
    {
        return SRTM_Status_InvalidState;
    }

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s\r\n", __func__);

    disp->stopReq = true;
    /* Wakeup dispatcher to do stop operations */
    SRTM_Sem_Post(disp->queueSig);
    /* Wait for dispatcher stopped */
    SRTM_Sem_Wait(disp->stopSig, SRTM_WAIT_FOR_EVER);

    disp->started = false;

    return SRTM_Status_Success;
}

void SRTM_Dispatcher_Run(srtm_dispatcher_t disp)
{
    srtm_list_t *list;
    srtm_peercore_t core;
    srtm_message_t message;

    assert(disp);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s\r\n", __func__);

    while (true)
    {
        /* Wait for start */
        SRTM_Sem_Wait(disp->startSig, SRTM_WAIT_FOR_EVER);

        /* Start peer cores */
        for (list = disp->cores.next; list != &disp->cores; list = list->next)
        {
            core = SRTM_LIST_OBJ(srtm_peercore_t, node, list);
            SRTM_PeerCore_Start(core);
        }

        while (!disp->stopReq)
        {
            /* Wait for message putting into Q */
            SRTM_Sem_Wait(disp->queueSig, SRTM_WAIT_FOR_EVER);
            /* Handle as many messages as possible */
            while ((message = SRTM_Dispatcher_RecvMessage(disp)) != NULL)
            {
                SRTM_Dispatcher_ProcessMessage(disp, message);
            }
        }

        /* Stop peer cores */
        for (list = disp->cores.next; list != &disp->cores; list = list->next)
        {
            core = SRTM_LIST_OBJ(srtm_peercore_t, node, list);
            SRTM_PeerCore_Stop(core);
        }

        /* Signal dispatcher stopped */
        SRTM_Sem_Post(disp->stopSig);
    }
}

srtm_status_t SRTM_Dispatcher_AddPeerCore(srtm_dispatcher_t disp, srtm_peercore_t core)
{
    assert(disp);
    assert(core);
    assert(!disp->started); /* Add core when SRTM dispatcher running is forbidden */

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s: 0x%x\r\n", __func__, core);

    if (!SRTM_List_IsEmpty(&core->node))
    {
        return SRTM_Status_ListAddFailed;
    }

    SRTM_Mutex_Lock(disp->mutex);
    SRTM_List_AddTail(&disp->cores, &core->node);
    SRTM_Mutex_Unlock(disp->mutex);
    core->dispatcher = disp;

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_RemovePeerCore(srtm_dispatcher_t disp, srtm_peercore_t core)
{
    uint32_t primask;
    srtm_list_t listHead;
    srtm_list_t *list, *next;
    srtm_message_t message;

    assert(disp);
    assert(core);
    assert(!disp->started); /* Remove core when SRTM dispatcher running is forbidden */

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s: 0x%x\r\n", __func__, core);

    if (SRTM_List_IsEmpty(&core->node))
    {
        return SRTM_Status_ListRemoveFailed;
    }

    SRTM_Mutex_Lock(disp->mutex);
    SRTM_List_Remove(&core->node);
    SRTM_Mutex_Unlock(disp->mutex);
    core->dispatcher = NULL;

    SRTM_List_Init(&listHead);

    /* Clean up all corresponding messages for the peer core */
    /* First clean up messages in messageQ */
    primask = DisableGlobalIRQ();
    for (list = disp->messageQ.next; list != &disp->messageQ; list = next)
    {
        next = list->next;
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        if (message->channel && message->channel->core == core)
        {
            SRTM_List_Remove(list);
            /* Add to temp list */
            SRTM_List_AddTail(&listHead, list);
        }
    }
    EnableGlobalIRQ(primask);

    /* Next clean up messages in waitingReqs */
    SRTM_Mutex_Lock(disp->mutex);
    for (list = disp->waitingReqs.next; list != &disp->waitingReqs; list = next)
    {
        next = list->next;
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        if (message->channel && message->channel->core == core)
        {
            SRTM_List_Remove(list);
            /* Add to temp list */
            SRTM_List_AddTail(&listHead, list);
        }
    }
    SRTM_Mutex_Unlock(disp->mutex);

    /* Now free all the messages on temp list */
    while (!SRTM_List_IsEmpty(&listHead))
    {
        list = listHead.next;
        SRTM_List_Remove(list);
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        SRTM_Dispatcher_FreeMessage(disp, message);
    }

    /* Last clean up messages in PeerCore's pendingQ */
    /* pendingQ is accessed only in dispatcher context, and now dispatcher is stopped.
       No need to lock */
    while (!SRTM_List_IsEmpty(&core->pendingQ))
    {
        list = core->pendingQ.next;
        SRTM_List_Remove(list);
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        SRTM_Dispatcher_FreeMessage(disp, message);
    }

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_RegisterService(srtm_dispatcher_t disp, srtm_service_t service)
{
    assert(disp);
    assert(service);
    assert(!disp->started); /* Register service when SRTM dispatcher running is forbidden */

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s: %d\r\n", __func__, service->category);

    if (!SRTM_List_IsEmpty(&service->node))
    {
        return SRTM_Status_ListAddFailed;
    }

    SRTM_Mutex_Lock(disp->mutex);
    SRTM_List_AddTail(&disp->services, &service->node);
    SRTM_Mutex_Unlock(disp->mutex);

    service->dispatcher = disp;

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_UnregisterService(srtm_dispatcher_t disp, srtm_service_t service)
{
    assert(disp);
    assert(service);
    assert(!disp->started); /* Unregister service when SRTM dispatcher running is forbidden */

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_INFO, "%s: %d\r\n", __func__, service->category);

    if (SRTM_List_IsEmpty(&service->node))
    {
        return SRTM_Status_ListRemoveFailed;
    }

    SRTM_Mutex_Lock(disp->mutex);
    SRTM_List_Remove(&service->node);
    SRTM_Mutex_Unlock(disp->mutex);

    service->dispatcher = NULL;

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_Request(srtm_dispatcher_t disp, srtm_request_t req,
                                     srtm_response_t *pResp, uint32_t timeout)
{
    srtm_sem_t signal;
    srtm_status_t status;
    srtm_response_t resp;

    assert(disp);
    assert(req);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    signal = SRTM_Sem_Create(1U, 0U);
    if (!signal)
    {
        return SRTM_Status_OutOfMemory;
    }

    req->reqMsg.isSyncReq = true;
    req->reqMsg.sync.sig = signal;
    req->reqMsg.sync.resp = NULL;

    req->error = SRTM_Status_Success;
    SRTM_Dispatcher_QueueMessage(disp, req);
    status = SRTM_Sem_Wait(signal, timeout);
    /* Make sure request is no longer in messageQ or waitingReqs list */
    if (status != SRTM_Status_Success)
    {
        while (SRTM_Dispatcher_DequeueMessage(disp, req) != SRTM_Status_Success)
        {
            /* If request is not on any list, it means SRTM dispatcher is processing or just going
               to send the response. Wait again to make sure dispatcher has put it on the
               waitingReqs queue or properly responded */
            if ((status = SRTM_Sem_Wait(signal, timeout)) == SRTM_Status_Success)
            {
                break;
            }
        }
    }

    /* Clean up */
    SRTM_Sem_Destroy(signal);
    req->reqMsg.sync.sig = NULL;

    resp = req->reqMsg.sync.resp;
    if (pResp)
    {
        *pResp = resp; /* Now application gets response and will destroy it later */
    }

    if (!resp) /* Failed to get response */
    {
        status = status == SRTM_Status_Timeout ? SRTM_Status_TransferTimeout : req->error;
    }
    else
    {
        if (!pResp) /* Response not needed by application */
        {
            SRTM_Response_Destroy(resp);
        }
        status = SRTM_Status_Success;
    }

    return status;
}

srtm_status_t SRTM_Dispatcher_DeliverRequest(srtm_dispatcher_t disp, srtm_request_t req,
                                            srtm_dispatcher_resp_cb_t callback, void *param)
{
    assert(disp);
    assert(req);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    req->reqMsg.isSyncReq = false;
    req->reqMsg.async.cb = callback;
    req->reqMsg.async.param = param;

    return SRTM_Dispatcher_DeliverRawData(disp, req);
}

srtm_status_t SRTM_Dispatcher_DeliverResponse(srtm_dispatcher_t disp,
                                             srtm_response_t resp)
{
    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    return SRTM_Dispatcher_DeliverRawData(disp, resp);
}

srtm_status_t SRTM_Dispatcher_DeliverNotification(srtm_dispatcher_t disp,
                                                 srtm_notification_t notif)
{
    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    return SRTM_Dispatcher_DeliverRawData(disp, notif);
}

srtm_status_t SRTM_Dispatcher_DeliverRawData(srtm_dispatcher_t disp,
                                            srtm_rawdata_t data)
{
    assert(disp);
    assert(data);

    SRTM_Dispatcher_QueueMessage(disp, data);

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_DeliverMessages(srtm_dispatcher_t disp, srtm_list_t *msgs)
{
    uint32_t primask;
    srtm_list_t *list;
    srtm_message_t message;

    assert(disp);
    assert(msgs);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    primask = DisableGlobalIRQ();
    while (!SRTM_List_IsEmpty(msgs))
    {
        list = msgs->next;
        SRTM_List_Remove(list);
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
        SRTM_Dispatcher_InsertOrderedMessage(disp, message);
    }
    EnableGlobalIRQ(primask);

    SRTM_Sem_Post(disp->queueSig);

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_CallProc(srtm_dispatcher_t disp, srtm_procedure_t proc, uint32_t timeout)
{
    srtm_sem_t signal;
    srtm_status_t status;

    assert(disp);
    assert(proc);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    signal = SRTM_Sem_Create(1U, 0U);
    if (!signal)
    {
        return SRTM_Status_OutOfMemory;
    }

    proc->procMsg.sig = signal;
    SRTM_Dispatcher_QueueMessage(disp, proc);

    status = SRTM_Sem_Wait(signal, timeout);
    /* Make sure proc is no longer in messageQ */
    if (status != SRTM_Status_Success)
    {
        while (SRTM_Dispatcher_DequeueMessage(disp, proc) != SRTM_Status_Success)
        {
            /* If request is not on any list, it means SRTM dispatcher is processing.
               Wait again to make sure dispatcher has finished calling the procedure. */
            if ((status = SRTM_Sem_Wait(signal, timeout)) == SRTM_Status_Success)
            {
                break;
            }
        }
    }

    /* Clean up */
    SRTM_Sem_Destroy(signal);
    proc->procMsg.sig = NULL;

    return status;
}

srtm_status_t SRTM_Dispatcher_PostProc(srtm_dispatcher_t disp, srtm_procedure_t proc)
{
    assert(disp);
    assert(proc);

    proc->procMsg.sig = NULL;
    SRTM_Dispatcher_QueueMessage(disp, proc);

    return SRTM_Status_Success;
}

srtm_status_t SRTM_Dispatcher_PostRecvData(srtm_dispatcher_t disp, srtm_channel_t channel,
                                           void *buf, uint32_t len)
{
    srtm_list_t *list;
    srtm_message_t message = NULL;
    uint32_t primask;
    srtm_packet_head_t *head;

    assert(disp);
    assert(channel);
    assert(buf);

    if (len < sizeof(struct _srtm_packet_head) || len > SRTM_DISPATCHER_CONFIG_RX_MSG_MAX_LEN)
    {
        return SRTM_Status_InvalidParameter;
    }

    /* Get free message to save the data */
    primask = DisableGlobalIRQ();
    if (!SRTM_List_IsEmpty(&disp->freeRxMsgs))
    {
        list = disp->freeRxMsgs.next;
        SRTM_List_Remove(list);
        message = SRTM_LIST_OBJ(srtm_message_t, node, list);
    }
    EnableGlobalIRQ(primask);

    assert(message); /* For debugging the shortage of RX buffer */

    if (message)
    {
        message->direct = SRTM_MessageDirectRx;
        memcpy(message->data, buf, len);
        message->dataLen = len;
        message->channel = channel;
        message->error = SRTM_Status_Success;

        head = (srtm_packet_head_t *)buf;
        message->type = (srtm_message_type_t)head->type;
        message->priority = head->priority;

        SRTM_Dispatcher_QueueMessage(disp, message);

        return SRTM_Status_Success;
    }

    return SRTM_Status_OutOfMemory;
}

static srtm_status_t SRTM_Dispatcher_CallService(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_list_t *list;
    srtm_service_t service;
    uint8_t category;
    srtm_status_t status = SRTM_Status_ServiceNotFound;

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    category = SRTM_CommMessage_GetCategory(msg);

    /* Don't need to lock mutex because service will not change when dispatcher is running */
    for (list = disp->services.next; list != &disp->services; list = list->next)
    {
        service = SRTM_LIST_OBJ(srtm_service_t, node, list);
        if (service->category == category)
        {
            if (msg->type == SRTM_MessageTypeRequest)
            {
                status = SRTM_Service_Request(service, msg);
            }
            else
            {
                status = SRTM_Service_Notify(service, msg);
            }
            break;
        }
    }

    return status;
}

static srtm_status_t SRTM_Dispatcher_HandleResponse(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_list_t *list;
    srtm_request_t req = NULL;
    srtm_response_t resp;
    uint8_t category;
    uint8_t command;
    srtm_status_t status = SRTM_Status_Success;

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    category = SRTM_CommMessage_GetCategory(msg);
    command = SRTM_CommMessage_GetCommand(msg);

    SRTM_Mutex_Lock(disp->mutex);
    for (list = disp->waitingReqs.next; list != &disp->waitingReqs; list = list->next)
    {
        req = SRTM_LIST_OBJ(srtm_request_t, node, list);
        if (SRTM_CommMessage_GetCategory(req) == category &&
            SRTM_CommMessage_GetCommand(req) == command &&
            req->channel == msg->channel) /* FIXME: need a UID? */
        {
            SRTM_List_Remove(list);
            break;
        }
    }
    SRTM_Mutex_Unlock(disp->mutex);

    if (req && list != &disp->waitingReqs) /* Find corresponding request */
    {
        if (req->reqMsg.isSyncReq)
        {
            resp = SRTM_Message_Duplicate(msg);
            if (resp)
            {
                if (msg->dataLen > 0)
                {
                    memcpy(resp->data, msg->data, msg->dataLen);
                }
                /* Duplicated response will be freed by application */
                req->reqMsg.sync.resp = resp;
            }
            else
            {
                req->error = SRTM_Status_OutOfMemory;
            }
            SRTM_Sem_Post(req->reqMsg.sync.sig);
        }
        else
        {
            if (req->reqMsg.async.cb)
            {
                req->reqMsg.async.cb(disp, req, msg, req->reqMsg.async.param);
            }
            /* Request is destroyed in dispatcher for async call */
            SRTM_Request_Destroy(req);
        }
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t SRTM_Dispatcher_SendMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_status_t status;

    status = SRTM_Channel_SendData(msg->channel, msg->data, msg->dataLen);
    assert(status == SRTM_Status_Success); /* For debugging the message sending failure */

    switch(msg->type)
    {
        case SRTM_MessageTypeRawData:
            SRTM_RawData_Destroy(msg);
            break;
        case SRTM_MessageTypeRequest:
            if (status == SRTM_Status_Success)
            {
                /* Add request to waiting queue to wait for response */
                SRTM_Mutex_Lock(disp->mutex);
                SRTM_List_AddTail(&disp->waitingReqs, &msg->node);
                SRTM_Mutex_Unlock(disp->mutex);
            }
            else
            {
                SRTM_Dispatcher_FreeRequest(disp, msg, SRTM_Status_TransferFailed);
            }
            break;
        case SRTM_MessageTypeResponse:
            SRTM_Response_Destroy(msg);
            break;
        case SRTM_MessageTypeNotification:
            SRTM_Notification_Destroy(msg);
            break;
        default:
            break;
    }

    return status;
}

static srtm_status_t SRTM_Dispatcher_HandleTxMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_status_t status = SRTM_Status_Success;
    srtm_peercore_t core;
    srtm_peercore_state_t state;
    srtm_list_t *list;
    srtm_message_t message;

    assert(msg->channel);
    assert(msg->channel->core);

    core = msg->channel->core;
    state = SRTM_PeerCore_GetState(core);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    switch (state)
    {
        case SRTM_PeerCore_State_Inactive:
            SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "SRTM_PeerCore_State_Inactive\r\n");
            /* Peer core not active, discard the message */
            SRTM_Dispatcher_FreeMessage(disp, msg);
            break;
        case SRTM_PeerCore_State_Activating:
            SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "SRTM_PeerCore_State_Activating\r\n");
            /* Peer core in activating, put message to pendingQ */
            SRTM_List_AddTail(&core->pendingQ, &msg->node);
            break;
        case SRTM_PeerCore_State_Activated:
            SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "SRTM_PeerCore_State_Activated\r\n");
            /* Peer core working well, first send messages in pendingQ and then send the msg */
            while (!SRTM_List_IsEmpty(&core->pendingQ))
            {
                list = core->pendingQ.next;
                SRTM_List_Remove(list);
                message = SRTM_LIST_OBJ(srtm_message_t, node, list);
                SRTM_Dispatcher_SendMessage(disp, message);
            }
            status = SRTM_Dispatcher_SendMessage(disp, msg);
            break;
        case SRTM_PeerCore_State_Deactivating:
        case SRTM_PeerCore_State_Deactivated:
            SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "SRTM_PeerCore_State_Deactivate\r\n");
            /* Peer core in deactivate state, put message to pendingQ and activate peer core */
            SRTM_List_AddTail(&core->pendingQ, &msg->node);
            status = SRTM_PeerCore_Activate(core);
            break;
        default:
            SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_WARN, "%s: wrong peer core state!\r\n", __func__);
            break;
    }

    return status;
}

srtm_status_t SRTM_Dispatcher_ProcessMessage(srtm_dispatcher_t disp, srtm_message_t msg)
{
    srtm_status_t status = SRTM_Status_Success;

    assert(disp);
    assert(msg);

    SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG, "%s\r\n", __func__);

    SRTM_DumpMessage(msg);

    if (msg->direct == SRTM_MessageDirectRx)
    {
        switch (msg->type)
        {
            case SRTM_MessageTypeRequest:
            case SRTM_MessageTypeNotification:
                status = SRTM_Dispatcher_CallService(disp, msg);
                break;
            case SRTM_MessageTypeResponse:
                status = SRTM_Dispatcher_HandleResponse(disp, msg);
                break;
            default:
                /* We cannot handle message other than Communication Message */
                SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_WARN,
                                   "%s: unsupported RX message type %d\r\n", __func__, msg->type);
                break;
        }
        SRTM_Dispatcher_FreeMessage(disp, msg);
    }
    else /* TX message or procedure message */
    {
        switch (msg->type)
        {
            case SRTM_MessageTypeProcedure:
                SRTM_DEBUG_MESSAGE(SRTM_DEBUG_VERBOSE_DEBUG,
                                   "%s: Callback procedure 0x%08x\r\n", __func__, msg->procMsg.cb);
                msg->procMsg.cb(disp, msg->procMsg.param1, msg->procMsg.param2);
                if (msg->procMsg.sig) /* Called by SRTM_Dispatcher_CallProc() */
                {
                    /* Synchronous procedure message will be freed by user. Here just wakeup user's task. */
                    SRTM_Sem_Post(msg->procMsg.sig);
                }
                else /* Called by SRTM_Dispatcher_PostProc() */
                {
                    SRTM_Procedure_Destroy(msg);
                }
                break;
            default:
                status = SRTM_Dispatcher_HandleTxMessage(disp, msg);
                break;
        }
    }

    return status;
}
/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

#include "phOsalNfc_Pch.h"

#include "phOsalNfc_MsgQueue.tmh"

typedef struct { LIST_ENTRY Link;phOsalNfc_MsgQueue_t Message; } PH_OSAL_PENDING_MESSAGE;

static DWORD WINAPI phOsalNfc_MsgQueue_Thread(void* pContext);

NFCSTATUS phOsalNfc_MsgQueue_Init(void)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    PH_LOG_OSAL_FUNC_ENTRY();

    InitializeCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);
    gpphOsalNfc_ClientDriver_Context->PendingMessages.Flink=
        gpphOsalNfc_ClientDriver_Context->PendingMessages.Blink=&gpphOsalNfc_ClientDriver_Context->PendingMessages;

    gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent != NULL)
    {
        gpphOsalNfc_ClientDriver_Context->hCallbackThread = CreateThread(NULL, 0, phOsalNfc_MsgQueue_Thread, NULL, CREATE_SUSPENDED, (LPDWORD)&gpphOsalNfc_ClientDriver_Context->dwCallbackThreadID);

        if(gpphOsalNfc_ClientDriver_Context->hCallbackThread != NULL)
        {
            SetThreadPriority(gpphOsalNfc_ClientDriver_Context->hCallbackThread, THREAD_PRIORITY_TIME_CRITICAL);
            ResumeThread(gpphOsalNfc_ClientDriver_Context->hCallbackThread);
        }
        else
        {
            wStatus = PHNFCSTVAL(CID_NFC_OSAL, NFCSTATUS_INSUFFICIENT_RESOURCES);
            PH_LOG_OSAL_INFO_STR("OSAL out of memory");
        }
    }
    else
    {
        wStatus = PHNFCSTVAL(CID_NFC_OSAL, NFCSTATUS_INSUFFICIENT_RESOURCES);
        PH_LOG_OSAL_INFO_STR("OSAL out of memory");
    }

    PH_LOG_OSAL_FUNC_EXIT();
    return wStatus;
}

void phOsalNfc_MsgQueue_DeInit(void)
{
    PH_LOG_OSAL_FUNC_ENTRY();

    if(gpphOsalNfc_ClientDriver_Context != NULL)
    {
        if(gpphOsalNfc_ClientDriver_Context->hCallbackThread != NULL)
        {
            phOsalNfc_PostMsg(PH_OSALNFC_EXIT_THREAD, (UINT_PTR)NULL, (UINT_PTR)NULL, (UINT_PTR)NULL, (UINT_PTR)NULL);
            WaitForSingleObject(gpphOsalNfc_ClientDriver_Context->hCallbackThread, INFINITE);

            CloseHandle(gpphOsalNfc_ClientDriver_Context->hCallbackThread);
            gpphOsalNfc_ClientDriver_Context->hCallbackThread = NULL;
        }

        if(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent != NULL)
        {
            CloseHandle(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent);
            gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent = NULL;
        }

        DeleteCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);

        gpphOsalNfc_ClientDriver_Context->bMsgQueueHead = 0;
        gpphOsalNfc_ClientDriver_Context->bMsgQueueTail = 0;
        gpphOsalNfc_ClientDriver_Context->bMsgQueueUsed = 0;
    }

    PH_LOG_OSAL_FUNC_EXIT();
}

void phOsalNc_GetMsg(_Out_ uint32_t *pMessage, _Out_ UINT_PTR * pParam1, _Out_ UINT_PTR * pParam2, _Out_ UINT_PTR * pParam3, _Out_ UINT_PTR * pParam4)
{
    bool_t bMessageAvailable = FALSE;


    while(!bMessageAvailable)
    {
        WaitForSingleObject(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent, INFINITE);

        EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);
        bMessageAvailable = gpphOsalNfc_ClientDriver_Context->PendingMessages.Flink!=&gpphOsalNfc_ClientDriver_Context->PendingMessages;

        if(bMessageAvailable)
        {
            LIST_ENTRY* head=&gpphOsalNfc_ClientDriver_Context->PendingMessages;
            LIST_ENTRY* link=head->Flink;
            head->Flink=link->Flink;link->Flink->Blink=head;
            PH_OSAL_PENDING_MESSAGE* entry=CONTAINING_RECORD(link,PH_OSAL_PENDING_MESSAGE,Link);
            *pMessage=entry->Message.Message;*pParam1=entry->Message.Param1;
            *pParam2=entry->Message.Param2;*pParam3=entry->Message.Param3;*pParam4=entry->Message.Param4;
            phOsalNfc_FreeMemory(entry);
            if(gpphOsalNfc_ClientDriver_Context->PendingMessages.Flink==&gpphOsalNfc_ClientDriver_Context->PendingMessages)
                ResetEvent(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent);
        }

        LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);
    }
}

void phOsalNfc_PostMsg(_In_ uint32_t Message, _In_ UINT_PTR Param1, _In_ UINT_PTR Param2, _In_ UINT_PTR Param3, _In_ UINT_PTR Param4)
{
    PH_OSAL_PENDING_MESSAGE* entry;
    if(!gpphOsalNfc_ClientDriver_Context) return;
    entry=phOsalNfc_GetMemory(sizeof(*entry));
    if(!entry) { phOsalNfc_RaiseException(phOsalNfc_e_InternalErr,1);return; }
    entry->Message.Message=Message;entry->Message.Param1=Param1;entry->Message.Param2=Param2;
    entry->Message.Param3=Param3;entry->Message.Param4=Param4;
    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);
    {
        LIST_ENTRY* head=&gpphOsalNfc_ClientDriver_Context->PendingMessages;
        entry->Link.Flink=head;entry->Link.Blink=head->Blink;
        head->Blink->Flink=&entry->Link;head->Blink=&entry->Link;
    }
    SetEvent(gpphOsalNfc_ClientDriver_Context->hMsgQueueEvent);
    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->MsgQueueLock);
}

static DWORD WINAPI phOsalNfc_MsgQueue_Thread(void* pContext)
{
    bool_t bContinue = TRUE;
    uint32_t message;
    UINT_PTR param1, param2, param3, param4;

    PH_LOG_OSAL_FUNC_ENTRY();

    UNUSED(pContext);

    while(bContinue)
    {
        phOsalNc_GetMsg(&message, &param1, &param2, &param3, &param4);

        if(message >= PH_OSALNFC_MESSAGE_BASE)
        {
            PH_LOG_OSAL_INFO_STR("%x: %p, %p", message, (void*)param1, (void*)param2);

            switch(message)
            {
                case PH_OSALNFC_DEFERRED_CALLBACK:
                case PH_OSALNFC_TIMER_MSG:
                {
                    pphOsalNfc_DeferFuncPointer_t pfnDeferredCallback = (pphOsalNfc_DeferFuncPointer_t)param1;
                    
                    if(pfnDeferredCallback != NULL) {
                        (*pfnDeferredCallback)((VOID*)param2);
                    }
                }
                break;

                case PH_OSALNFC_EXIT_THREAD:
                {
                    bContinue = FALSE;
                }
                break;
            }
        }
        else
        {
            gpphOsalNfc_ClientDriver_Context->pfnCallback(gpphOsalNfc_ClientDriver_Context->pCallbackContext, message, param1, param2, param3, param4);
        }
    }

    PH_LOG_OSAL_FUNC_EXIT();
    return ERROR_SUCCESS;
}

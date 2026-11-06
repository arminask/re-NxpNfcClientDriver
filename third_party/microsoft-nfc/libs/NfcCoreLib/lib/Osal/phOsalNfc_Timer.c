/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

#include "phOsalNfc_Pch.h"

#include "phOsalNfc_Timer.tmh"

static void phOsalNfc_TimerDeferredCall(void *pParam)
{
    const UINT_PTR token=(UINT_PTR)pParam;
    const uint32_t index=(uint32_t)(token & 15);
    const uint32_t generation=(uint32_t)(token >> 4);
    ppCallBck_t callback=NULL;void* context=NULL;
    if(!gpphOsalNfc_ClientDriver_Context || index>=PH_MAX_OSAL_NUM_TIMERS) return;
    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
    if(gpphOsalNfc_ClientDriver_Context->TimerList[index].Generation==generation &&
       gpphOsalNfc_ClientDriver_Context->TimerList[index].bFired) {
        callback=gpphOsalNfc_ClientDriver_Context->TimerList[index].pCallback;
        context=gpphOsalNfc_ClientDriver_Context->TimerList[index].pContext;
        gpphOsalNfc_ClientDriver_Context->TimerList[index].bFired=FALSE;
    }
    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
    if(callback) callback(index+PH_OSAL_TIMER_BASE_ADDRESS,context);
}

static void CALLBACK phOsalNfc_TimerCallback(PTP_CALLBACK_INSTANCE pInstance, PVOID pContext, PTP_TIMER pTimer)
{
    uint32_t uIndex = (uint32_t)(ULONG_PTR)pContext;

    UNREFERENCED_PARAMETER(pInstance);
    UNREFERENCED_PARAMETER(pTimer);

    if (gpphOsalNfc_ClientDriver_Context != NULL)
    {
        EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
        
        if (gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pCallback != NULL) {
            gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].bFired = TRUE;
            phOsalNfc_PostMsg(PH_OSALNFC_TIMER_MSG, 
                             (UINT_PTR)phOsalNfc_TimerDeferredCall, 
                             (((UINT_PTR)gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].Generation << 4) | uIndex),
                             (UINT_PTR)NULL,
                             (UINT_PTR)NULL);
        }

        LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
    }
}

void phOsalNfc_Timer_Init(void)
{
    if (gpphOsalNfc_ClientDriver_Context != NULL)
    {
        phOsalNfc_SetMemory((void *)gpphOsalNfc_ClientDriver_Context->TimerList,0,sizeof(gpphOsalNfc_ClientDriver_Context->TimerList));
        InitializeCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
    }
}

void phOsalNfc_Timer_DeInit(void)
{
    uint32_t i = 0;

    if (gpphOsalNfc_ClientDriver_Context != NULL)
    {
        EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

        while (i < PH_MAX_OSAL_NUM_TIMERS) 
        {
            if(gpphOsalNfc_ClientDriver_Context->TimerList[i].pTimer != NULL)
            {
                PH_LOG_OSAL_INFO_STR("TimerId=%d forced cleanup", i);
                /* A callback may need TimerLock while Delete drains it.
                 * Drop this outer recursive acquisition as well. */
                LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
                phOsalNfc_Timer_Delete(i + PH_OSAL_TIMER_BASE_ADDRESS);
                EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
            }

            i++;
        }

        LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    }
}

uint32_t phOsalNfc_Timer_Create(void)
{
    uint32_t i = 0;

    if (NULL == gpphOsalNfc_ClientDriver_Context) {
        return PH_OSALNFC_TIMER_ID_INVALID;
    }

    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    while (i < PH_MAX_OSAL_NUM_TIMERS) 
    {
        /* check whether the timer is free. If free then only it is created */
        if(gpphOsalNfc_ClientDriver_Context->TimerList[i].pTimer == NULL)
        {
            gpphOsalNfc_ClientDriver_Context->TimerList[i].pTimer = CreateThreadpoolTimer(phOsalNfc_TimerCallback, (void*)(ULONG_PTR)i, NULL);
            gpphOsalNfc_ClientDriver_Context->TimerList[i].dwThreadId = GetCurrentThreadId();
            break;
        }

        i++;
    }

    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    if ((i == PH_MAX_OSAL_NUM_TIMERS) || (gpphOsalNfc_ClientDriver_Context->TimerList[i].pTimer == NULL))
    {
        return PH_OSALNFC_TIMER_ID_INVALID;
    }

    return (i + PH_OSAL_TIMER_BASE_ADDRESS);
}


/* This starts the timer */ 
NFCSTATUS phOsalNfc_Timer_Start(uint32_t    TimerId,
                          uint32_t     dueTimeMsec, 
                          ppCallBck_t  pCallback,
                          void         *pContext)
{
    uint32_t  uIndex;
    LONGLONG  DueTime;
    uint32_t  uWindow = (dueTimeMsec < 50) ? 0 : (dueTimeMsec / 4);

    if (NULL == gpphOsalNfc_ClientDriver_Context ||
        PH_OSALNFC_TIMER_ID_INVALID == TimerId) {
        return PHNFCSTVAL(CID_NFC_OSAL, NFCSTATUS_INVALID_PARAMETER);
    }

    uIndex = TimerId - PH_OSAL_TIMER_BASE_ADDRESS;
    if(uIndex>=PH_MAX_OSAL_NUM_TIMERS) return NFCSTATUS_INVALID_PARAMETER;
    
    /* Retire an older arm before publishing the next generation. A pool
     * callback already waiting for TimerLock must not observe the new arm. */
    if(phOsalNfc_Timer_Stop(TimerId)!=NFCSTATUS_SUCCESS) return NFCSTATUS_INVALID_PARAMETER;

    // Convert dueTimeMsec to relative filetime units (100ns)
    DueTime = Int32x32To64(dueTimeMsec, -10000);

    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    if(!gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer || !pCallback) {
        LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);return NFCSTATUS_INVALID_PARAMETER;
    }
    ++gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].Generation;
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].bFired=FALSE;
    SetThreadpoolTimer(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer, (FILETIME*)&DueTime, 0, uWindow);
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pCallback = pCallback;
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pContext  = pContext;

    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    return NFCSTATUS_SUCCESS;
}


NFCSTATUS phOsalNfc_Timer_Stop(uint32_t TimerId)
{
    uint32_t  uIndex;

    if (NULL == gpphOsalNfc_ClientDriver_Context ||
        PH_OSALNFC_TIMER_ID_INVALID == TimerId)
    {
        return PHNFCSTVAL(CID_NFC_OSAL, NFCSTATUS_INVALID_PARAMETER);
    }

    uIndex = TimerId - PH_OSAL_TIMER_BASE_ADDRESS;
    if(uIndex>=PH_MAX_OSAL_NUM_TIMERS) return NFCSTATUS_INVALID_PARAMETER;
    
    if(!gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer) return NFCSTATUS_INVALID_PARAMETER;
    SetThreadpoolTimer(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer, NULL, 0, 0);
    WaitForThreadpoolTimerCallbacks(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer, TRUE);

    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    ++gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].Generation;
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].bFired    = FALSE;
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pCallback = NULL;
    gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pContext  = NULL;

    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
    return NFCSTATUS_SUCCESS;
}
    

void phOsalNfc_Timer_Delete(uint32_t TimerId)
{
    uint32_t uIndex;

    //
    // In various places in the code, timers are initialized only
    // after they are first needed.  Despite this fact, they are
    // deleted unconditionally when their context is being deleted
    // Intead of adding an if statement in all places where
    // timers are being deleted, this check is added to prevent 
    // a NULL deref.
    //
    if (NULL == gpphOsalNfc_ClientDriver_Context ||
        PH_OSALNFC_TIMER_ID_INVALID == TimerId)
    {
        return;
    }

    uIndex = TimerId - PH_OSAL_TIMER_BASE_ADDRESS;
    if(uIndex>=PH_MAX_OSAL_NUM_TIMERS) return;

    EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

    if(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer != NULL) 
    {
        SetThreadpoolTimer(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer, NULL, 0, 0);
        LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
        
        WaitForThreadpoolTimerCallbacks(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer, TRUE);

        EnterCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);

        ++gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].Generation;
        CloseThreadpoolTimer(gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer);
        gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer    = NULL;
        gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pCallback = NULL;
        gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pContext  = NULL;
        gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].bFired    = FALSE;

        uIndex = 0;
        while (uIndex < PH_MAX_OSAL_NUM_TIMERS) 
        {
            if (gpphOsalNfc_ClientDriver_Context->TimerList[uIndex].pTimer != NULL)
            {
                break;
            }
            uIndex++;
        }
    }

    LeaveCriticalSection(&gpphOsalNfc_ClientDriver_Context->TimerLock);
}

/* Called on the serialized OSAL thread; Stop drains thread-pool producers and
 * invalidates already queued timer messages before client contexts are freed. */
void phOsalNfc_Timer_StopAll(void)
{
    uint32_t i;
    if(!gpphOsalNfc_ClientDriver_Context) return;
    for(i=0;i<PH_MAX_OSAL_NUM_TIMERS;++i)
        if(gpphOsalNfc_ClientDriver_Context->TimerList[i].pTimer)
            (void)phOsalNfc_Timer_Stop(i+PH_OSAL_TIMER_BASE_ADDRESS);
}

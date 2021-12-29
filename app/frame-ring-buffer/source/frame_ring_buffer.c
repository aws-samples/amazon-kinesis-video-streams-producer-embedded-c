/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "frame_ring_buffer/frame_ring_buffer.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>

/* Thirdparty headers */
#include "azure_c_shared_utility/lock.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

typedef struct FrameKey
{
    struct FrameRingBuffer *pFrameRingBuffer;
    uint16_t uSerialNumber;
} FrameKey_t;

typedef struct FrameElement
{
    uint8_t *pData;
    size_t uLen;

    FrameKey_t xKey;

    FrameDestructorInfo_t xFrameDestructInfo;
} FrameElement_t;

typedef struct FrameRingBuffer
{
    LOCK_HANDLE xLock;

    FrameElement_t *pBuf;
    size_t uHeadIdx;
    size_t uTailIdx;
    size_t uSize;
    size_t uCapacity;

    unsigned short uNextSerialNumber;
    unsigned short uMaxSerialNumber;

    FrameRingBufferStat_t xStat;

    DropFramePolicy_t xDropFramePolicy;
} FrameRingBuffer_t;

static int prvDequeue(FrameRingBuffer_t *pFrameRingBuffer);

static size_t prvGetFreeCount(FrameRingBuffer_t *pFrameRingBuffer)
{
    return pFrameRingBuffer->xStat.uFrameFreeCount;
}

static size_t prvGetUsedCount(FrameRingBuffer_t *pFrameRingBuffer)
{
    return pFrameRingBuffer->xStat.uFrameUsedCount;
}

static bool prvIsFull(FrameRingBuffer_t *pFrameRingBuffer)
{
    return prvGetFreeCount(pFrameRingBuffer) == 0;
}

static bool prvIsEmpty(FrameRingBuffer_t *pFrameRingBuffer)
{
    return prvGetUsedCount(pFrameRingBuffer) == 0;
}

static void stepSerialNumber(FrameRingBuffer_t *pFrameRingBuffer, FrameKey_t *pKey)
{
    if (pKey->uSerialNumber == (pFrameRingBuffer->uSize) * 2 - 1)
    {
        pKey->uSerialNumber = 0;
    }
    else
    {
        pKey->uSerialNumber = pKey->uSerialNumber + 1;
    }
}

static int prvEnqueue(FrameRingBuffer_t *pFrameRingBuffer, uint8_t *pData, size_t uLen, FrameKey_t **ppKey, FrameDestructorInfo_t *pFrameDestructorInfo)
{
    int res = ERRNO_NONE;
    FrameElement_t *pFrameElement = NULL;
    FrameRingBufferStat_t *pStat = &(pFrameRingBuffer->xStat);

    if (prvIsFull(pFrameRingBuffer) && prvDequeue(pFrameRingBuffer) != ERRNO_NONE)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pFrameElement = &(pFrameRingBuffer->pBuf[pFrameRingBuffer->uHeadIdx]);
        pFrameElement->pData = pData;
        pFrameElement->uLen = uLen;

        pFrameElement->xKey.uSerialNumber = pFrameRingBuffer->uNextSerialNumber;
        pFrameRingBuffer->uNextSerialNumber++;
        if (pFrameRingBuffer->uNextSerialNumber == pFrameRingBuffer->uMaxSerialNumber)
        {
            pFrameRingBuffer->uNextSerialNumber = 0;
        }
        pFrameElement->xKey.pFrameRingBuffer = pFrameRingBuffer;

        if (pFrameDestructorInfo != NULL)
        {
            memcpy(&(pFrameElement->xFrameDestructInfo), pFrameDestructorInfo, sizeof(FrameDestructorInfo_t));
        }

        pFrameRingBuffer->uHeadIdx++;
        if (pFrameRingBuffer->uHeadIdx >= pFrameRingBuffer->uSize)
        {
            pFrameRingBuffer->uHeadIdx = 0;
        }

        *ppKey = &(pFrameElement->xKey);

        /* Update statistics */
        pStat->uSumOfFrameMemory += pFrameElement->uLen;
        pStat->uFrameFreeCount--;
        pStat->uFrameUsedCount++;
    }

    return res;
}

static int prvRemoveFrame(FrameRingBuffer_t *pFrameRingBuffer, FrameElement_t *pFrameElement)
{
    int res = ERRNO_NONE;
    int (*frameDestructor)(uint8_t *pData, size_t uLen, FrameKeyHandle keyHandle, void *pAppData);

    frameDestructor = pFrameElement->xFrameDestructInfo.frameDestructor;
    if (frameDestructor != NULL)
    {
        frameDestructor(pFrameElement->pData, pFrameElement->uLen, &(pFrameElement->xKey), pFrameElement->xFrameDestructInfo.pAppData);
    }
    memset(pFrameElement, 0, sizeof(FrameElement_t));

    return res;
}

static int prvDequeue(FrameRingBuffer_t *pFrameRingBuffer)
{
    int res = ERRNO_NONE;
    FrameElement_t *pFrameElement = NULL;
    size_t uFrameLen = 0;
    FrameRingBufferStat_t *pStat = &(pFrameRingBuffer->xStat);

    if (prvIsEmpty(pFrameRingBuffer))
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pFrameElement = &(pFrameRingBuffer->pBuf[pFrameRingBuffer->uTailIdx]);
        uFrameLen = pFrameElement->uLen;

        prvRemoveFrame(pFrameRingBuffer, pFrameElement);

        pFrameRingBuffer->uTailIdx++;
        if (pFrameRingBuffer->uTailIdx >= pFrameRingBuffer->uSize)
        {
            pFrameRingBuffer->uTailIdx = 0;
        }

        /* Update statistics */
        pStat->uSumOfFrameMemory -= uFrameLen;
        pStat->uFrameFreeCount++;
        pStat->uFrameUsedCount--;
    }

    return res;
}

static int prvFindFrameInContinuousRange(FrameRingBuffer_t *pFrameRingBuffer, FrameKey_t *pKey, size_t *puIdx, int uLeft, int uRight)
{
    int res = ERRNO_NONE;
    size_t uIdx = 0;

    uIdx = pKey->uSerialNumber % (pFrameRingBuffer->uSize);
    if (uIdx < uLeft || uIdx > uRight)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pFrameRingBuffer->pBuf[uIdx].xKey.uSerialNumber != pKey->uSerialNumber)
        {
            res = ERRNO_FAIL;
        }
        else
        {
            *puIdx = uIdx;
        }
    }


    return res;
}

static int prvFindFrame(FrameRingBuffer_t *pFrameRingBuffer, FrameKey_t *pKey, size_t *puIdx)
{
    int res = ERRNO_NONE;
    size_t uLatestIdx = 0;
    size_t uIdx = 0;

    if (prvIsEmpty(pFrameRingBuffer))
    {
        res = ERRNO_FAIL;
    }
    else
    {
        uLatestIdx = (pFrameRingBuffer->uHeadIdx == 0) ? pFrameRingBuffer->uCapacity : pFrameRingBuffer->uHeadIdx - 1;
        if (uLatestIdx >= pFrameRingBuffer->uTailIdx)
        {
            /* Frames are in one continuous range. */
            res = prvFindFrameInContinuousRange(pFrameRingBuffer, pKey, &uIdx, pFrameRingBuffer->uTailIdx, uLatestIdx);
        }
        else
        {
            /* Frames are in two separate range. The first range is 0 to uLatestIdx, the second range is uTailIdx to uCapacity */
            res = prvFindFrameInContinuousRange(pFrameRingBuffer, pKey, &uIdx, 0, (int)uLatestIdx);
            if (res != 0)
            {
                res = prvFindFrameInContinuousRange(pFrameRingBuffer, pKey, &uIdx, (int)(pFrameRingBuffer->uTailIdx), (int)(pFrameRingBuffer->uCapacity));
            }
        }
    }

    if (res == ERRNO_NONE)
    {
        if (puIdx != NULL)
        {
            *puIdx = uIdx;
        }
    }

    return res;
}

static size_t prvSumOfFrameMemory(FrameRingBuffer_t *pFrameRingBuffer)
{
    return pFrameRingBuffer->xStat.uSumOfFrameMemory;
}

static int prvGetMemoryStat(FrameRingBuffer_t *pFrameRingBuffer, FrameRingBufferStat_t *pStat)
{
    int res = ERRNO_NONE;

    pStat->uFrameUsedCount = prvGetUsedCount(pFrameRingBuffer);
    pStat->uFrameFreeCount = prvGetFreeCount(pFrameRingBuffer);
    pStat->uSumOfFrameMemory = prvSumOfFrameMemory(pFrameRingBuffer);

    return res;
}

static int prvApplyPolicyDropOldest(FrameRingBuffer_t *pFrameRingBuffer, DropOldestPolicyParameter_t *pDropOldestPolicyParameter)
{
    int res = ERRNO_NONE;
    size_t uSumOfFrameMemory = 0;

    while (prvSumOfFrameMemory(pFrameRingBuffer) > pDropOldestPolicyParameter->uMaxMem &&
        prvDequeue(pFrameRingBuffer) == ERRNO_NONE)
    {
        /* nop */
    }

    return res;
}

static int prvApplyPolicy(FrameRingBuffer_t *pFrameRingBuffer)
{
    int res = ERRNO_NONE;

    if (pFrameRingBuffer->xDropFramePolicy.type == eDontDrop)
    {
        /* nop */
    }
    else if (pFrameRingBuffer->xDropFramePolicy.type == eDropOldest)
    {
        res = prvApplyPolicyDropOldest(pFrameRingBuffer, &(pFrameRingBuffer->xDropFramePolicy.u.xDropOldestPolicyParameter));
    }

    return res;
}

FrameRingBufferHandle FrameRingBuffer_create(size_t uCapacity)
{
    FrameRingBuffer_t *pFrameRingBuffer = NULL;
    uint8_t *pMem = NULL;
    size_t uMemRequired = 0;

    if (uCapacity > 0)
    {
        uMemRequired = sizeof(FrameRingBuffer_t) + (uCapacity + 1) * sizeof(FrameElement_t);
        if ((pMem = (uint8_t *)malloc(uMemRequired)) != NULL) {
            memset(pMem, 0, uMemRequired);

            pFrameRingBuffer = (FrameRingBuffer_t *)pMem;

            if ((pFrameRingBuffer->xLock = Lock_Init()) == NULL)
            {
                free(pMem);
                pFrameRingBuffer = NULL;
            }
            else
            {
                pFrameRingBuffer->uCapacity = uCapacity;
                pFrameRingBuffer->uSize = uCapacity + 1; /* 1 more to check buffer full */
                pFrameRingBuffer->pBuf = (FrameElement_t *)(pMem + sizeof(FrameRingBuffer_t));
                pFrameRingBuffer->uHeadIdx = 0;
                pFrameRingBuffer->uTailIdx = 0;

                pFrameRingBuffer->xStat.uSumOfFrameMemory = 0;
                pFrameRingBuffer->xStat.uFrameUsedCount = 0;
                pFrameRingBuffer->xStat.uFrameFreeCount = pFrameRingBuffer->uCapacity;

                pFrameRingBuffer->uNextSerialNumber = 0;
                pFrameRingBuffer->uMaxSerialNumber = (UINT16_MAX / pFrameRingBuffer->uSize) * pFrameRingBuffer->uSize;
            }
        }
    }

    return pFrameRingBuffer;
}

void FrameRingBuffer_terminate(FrameRingBufferHandle handle)
{
    FrameRingBuffer_t *pFrameRingBuffer = (FrameRingBuffer_t *)handle;

    if (pFrameRingBuffer != NULL)
    {
        if (Lock(pFrameRingBuffer->xLock) == LOCK_OK)
        {
            while (prvDequeue(pFrameRingBuffer) == 0)
            {
                /* nop */
            }

            Unlock(pFrameRingBuffer->xLock);
        }

        Lock_Deinit(pFrameRingBuffer->xLock);
        free(pFrameRingBuffer);
    }
}

FrameKeyHandle FrameRingBuffer_enqueue(FrameRingBufferHandle handle, uint8_t *pData, size_t uLen, FrameDestructorInfo_t *pFrameDestructorInfo)
{
    int res = ERRNO_NONE;
    FrameKey_t *pKey = NULL;
    FrameRingBuffer_t *pFrameRingBuffer = (FrameRingBuffer_t *)handle;

    if (pFrameRingBuffer == NULL || pData == NULL || uLen == 0)
    {
        res = ERRNO_FAIL;
    }
    else if (Lock(pFrameRingBuffer->xLock) != LOCK_OK)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        res = prvEnqueue(pFrameRingBuffer, pData, uLen, &pKey, pFrameDestructorInfo);

        prvApplyPolicy(pFrameRingBuffer);

        Unlock(pFrameRingBuffer->xLock);
    }

    return pKey;
}

int FrameRingBuffer_dequeue(FrameRingBufferHandle handle)
{
    int res = ERRNO_NONE;
    FrameRingBuffer_t *pFrameRingBuffer = (FrameRingBuffer_t *)handle;

    if (pFrameRingBuffer == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if (Lock(pFrameRingBuffer->xLock) != LOCK_OK)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        res = prvDequeue(pFrameRingBuffer);

        Unlock(pFrameRingBuffer->xLock);
    }

    return res;
}

int FrameRingBuffer_getFrame(FrameKeyHandle keyHandle, uint8_t **ppData, size_t *puLen)
{
    int res = ERRNO_NONE;
    FrameRingBuffer_t *pFrameRingBuffer = NULL;
    FrameKey_t *pKey = (FrameKey_t *)keyHandle;
    size_t uIdx = 0;

    if (pKey == NULL || pKey->pFrameRingBuffer == NULL || ppData == NULL || puLen == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pFrameRingBuffer = pKey->pFrameRingBuffer;

        if (Lock(pFrameRingBuffer->xLock) != LOCK_OK)
        {
            res = ERRNO_FAIL;
        }
        else
        {
            if ((res = prvFindFrame(pFrameRingBuffer, pKey, &uIdx)) == ERRNO_NONE)
            {
                *ppData = pFrameRingBuffer->pBuf[uIdx].pData;
                *puLen = pFrameRingBuffer->pBuf[uIdx].uLen;
            }

            Unlock(pFrameRingBuffer->xLock);
        }
    }

    return res;
}

int FrameRingBuffer_getMemoryStat(FrameRingBufferHandle handle, FrameRingBufferStat_t *pStat)
{
    int res = ERRNO_NONE;
    FrameRingBuffer_t *pFrameRingBuffer = (FrameRingBuffer_t *)handle;

    if (pFrameRingBuffer == NULL || pStat == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if (Lock(pFrameRingBuffer->xLock) != LOCK_OK)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        res = prvGetMemoryStat(pFrameRingBuffer, pStat);

        Unlock(pFrameRingBuffer->xLock);
    }

    return res;
}

int FrameRingBuffer_setDropFramePolicy(FrameRingBufferHandle handle, DropFramePolicy_t *pPolicy)
{
    int res = ERRNO_NONE;
    FrameRingBuffer_t *pFrameRingBuffer = (FrameRingBuffer_t *)handle;

    if (pFrameRingBuffer == NULL || pPolicy == NULL)
    {
        res = ERRNO_FAIL;
    }
    /* Lock here because we may enqueue/dequeue frames at the moment. */
    else if (Lock(pFrameRingBuffer->xLock) != LOCK_OK)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memcpy(&(pFrameRingBuffer->xDropFramePolicy), pPolicy, sizeof(DropFramePolicy_t));

        prvApplyPolicy(pFrameRingBuffer);

        Unlock(pFrameRingBuffer->xLock);
    }

    return res;
}
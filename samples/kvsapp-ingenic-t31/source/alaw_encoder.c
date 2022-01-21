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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "alaw_encoder.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

typedef struct AlawEncoder
{
    unsigned int uSampleRate;
    unsigned int uChannels;
    unsigned int uBitRate;
} AlawEncoder_t;

static int16_t xSegmentAlawEnd[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};

static uint8_t prvEncodePcmToAlaw(int16_t xPcmVal)
{
    uint8_t uMask = 0x55;
    size_t uSegIdx = 0;
    uint8_t uAlawVal = 0;

    xPcmVal = xPcmVal >> 3;

    if (xPcmVal >= 0)
    {
        uMask |= 0x80;
    }
    else
    {
        xPcmVal = -xPcmVal - 1;
    }

    for (uSegIdx = 0; uSegIdx<8; uSegIdx++)
    {
        if (xPcmVal <= xSegmentAlawEnd[uSegIdx])
        {
            break;
        }
    }

    if (uSegIdx >= 8)
    {
        uAlawVal = 0x7F ^ uMask;
    }
    else
    {
        uAlawVal = uSegIdx << 4;
        uAlawVal |= (uSegIdx < 2) ? ((xPcmVal >> 1) & 0x0F) : ((xPcmVal >> uSegIdx) & 0x0F);
        uAlawVal ^= uMask;
    }

    return uAlawVal;
}

void *AlawEncoder_create(const unsigned int uSampleRate, const unsigned int uChannels, const unsigned int uBitRate)
{
    AlawEncoder_t *pAlawEncoder = NULL;

    if (uSampleRate == 0 || uChannels == 0 || uBitRate == 0)
    {
        printf("%s: Invalid parameters\n", __FUNCTION__);
    }
    else if((pAlawEncoder = (AlawEncoder_t *)malloc(sizeof(AlawEncoder_t))) == NULL)
    {
        printf("OOM: pAlawEncoder\n");
    }
    else
    {
        memset(pAlawEncoder, 0, sizeof(AlawEncoder_t));

        pAlawEncoder->uSampleRate = uSampleRate;
        pAlawEncoder->uChannels = uChannels;
        pAlawEncoder->uBitRate = uBitRate;

    }

    return (void *)pAlawEncoder;
}

void AlawEncoder_terminate(void *pHandle)
{
    AlawEncoder_t *pAlawEncoder = (AlawEncoder_t *)pHandle;

    if (pAlawEncoder != NULL)
    {
        free(pAlawEncoder);
    }
}

int AlawEncoder_setParameter(void *pHandle, char *pKey, void *pValue)
{
    int res = ERRNO_NONE;

    if (pHandle == NULL || pKey == NULL || pValue == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        /* No parameter is needed for a-law encoder. */
    }

    return res;
}

int AlawEncoder_getParameter(void *pHandle, char *pKey, void *pValue)
{
    int res = ERRNO_NONE;

    if (pHandle == NULL || pKey == NULL || pValue == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        /* TODO: Return audio parameters */
    }

    return res;
}

int AlawEncoder_encode(void *pHandle, uint8_t *pPcmBuf, size_t uPcmBufLen, uint8_t *pEncBuf, size_t uEncBufSize, size_t *puEncBufLen, size_t *puPcmBufUsed, uint64_t *puTimestampMs)
{
    int res = ERRNO_NONE;
    size_t uEncBufLen = 0;
    int16_t *pPcmData = NULL;
    struct timeval tv;
    uint64_t uTimestampMs = 0;

    gettimeofday(&tv, NULL);
    uTimestampMs = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

    if (pHandle == NULL || pPcmBuf == NULL || uPcmBufLen == 0 || puEncBufLen == NULL || puPcmBufUsed == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        /* Calculate the needed encode buffer size */
        uEncBufLen = uPcmBufLen / 2;

        if (pEncBuf == NULL)
        {
            /* If the output buffer size is null, we calculated the needed output length without really encoding it. */
            *puEncBufLen = uEncBufLen;
            *puPcmBufUsed = 0;
        }
        else
        {
            if (uEncBufSize < uEncBufLen)
            {
                res = ERRNO_FAIL;
            }
            else
            {
                pPcmData = (int16_t *)(pPcmBuf);
                for (size_t i = 0; i < uPcmBufLen/2; i++)
                {
                    pEncBuf[i] = prvEncodePcmToAlaw(pPcmData[i]);
                }
                *puEncBufLen = uEncBufLen;
                *puPcmBufUsed = uPcmBufLen;
                *puTimestampMs = uTimestampMs;
            }
        }
    }

    return res;
}


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

#include "kvs/allocator.h"
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "kvs/errors.h"
#include "kvs/mkv_parser.h"

static uint64_t prvPack(uint8_t *pBuf, size_t uLen)
{
    uint64_t uVal = 0;

    if (uLen <= 8)
    {
        for (size_t i = 0; i < uLen; i++)
        {
            uVal = (uVal << 8) | pBuf[i];
        }
    }

    return uVal;
}

size_t Mkv_getElementIdLen(uint8_t uByte)
{
    size_t uLen = 0;

    if (uByte & 0x80)
    {
        /* ID class A (1 byte) */
        uLen = 1;
    }
    else if (uByte & 0x40)
    {
        /* ID class B (2 bytes) */
        uLen = 2;
    }
    else if (uByte & 0x20)
    {
        /* ID class C (3 bytes) */
        uLen = 3;
    }
    else if (uByte & 0x10)
    {
        /* ID class D (4 bytes) */
        uLen = 4;
    }

    return uLen;
}

int Mkv_getElementId(uint8_t *pBuf, size_t uBufSize, uint32_t *puElementId, size_t *puElementIdLen)
{
    int xRes = KVS_ERRNO_NONE;
    size_t uElementIdLen = 0;

    if (pBuf == NULL || uBufSize == 0 || puElementId == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((uElementIdLen = Mkv_getElementIdLen(pBuf[0])) == 0)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (uBufSize < uElementIdLen)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        *puElementId = (uint32_t)prvPack(pBuf, uElementIdLen);
        *puElementIdLen = uElementIdLen;
    }

    return xRes;
}

size_t Mkv_getElementSizeLen(uint8_t uByte)
{
    size_t uLen = 0;
    uint8_t uMask = 0x80;

    if (uByte != 0)
    {
        uLen = 1;
        while (!(uMask & uByte))
        {
            uLen++;
            uMask >>= 1;
        }
    }

    return uLen;
}

int Mkv_getElementSize(uint8_t *pBuf, size_t uBufSize, uint64_t *puElementSize, size_t *puElementSizeLen)
{
    int xRes = KVS_ERRNO_NONE;
    size_t uElementSizeLen = 0;
    uint8_t pBufTemp[8];
    uint8_t uMask = 0;

    if (pBuf == NULL || uBufSize == 0 || puElementSize == NULL || puElementSizeLen == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((uElementSizeLen = Mkv_getElementSizeLen(pBuf[0])) == 0)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (uBufSize < uElementSizeLen)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        if (uElementSizeLen == 1 && pBuf[0] == MKV_ELEMENT_SIZE_UNKNOWN)
        {
            *puElementSize = MKV_ELEMENT_SIZE_UNKNOWN;
            *puElementSizeLen = 1;
        }
        else
        {
            memcpy(pBufTemp, pBuf, uElementSizeLen);
            uMask = 0x80 >> (uElementSizeLen - 1);
            pBufTemp[0] &= ~uMask;
            *puElementSize = prvPack(pBufTemp, uElementSizeLen);
            *puElementSizeLen = uElementSizeLen;
        }
    }

    return xRes;
}
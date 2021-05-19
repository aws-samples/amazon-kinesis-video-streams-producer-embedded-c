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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>

#include "file_input_stream.h"
#include "kvs/mkv_parser.h"

#include "mkv_update_time.h"

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

#define MKV_INPUT_FILENAME "/mnt/c/movie/test2.mkv"
#define MKV_OUTPUT_FILENAME "/mnt/c/movie/test-out.mkv"

static uint64_t pack(uint8_t *pBuf, size_t uLen)
{
    uint64_t uVal = 0;

    if (uLen <= 8)
    {
        for (size_t i = 0; i<uLen; i++)
        {
            uVal = (uVal << 8) | pBuf[i];
        }
    }

    return uVal;
}

static void unpack(uint8_t *pBuf, size_t uLen, uint64_t uVal)
{
    if (pBuf != NULL && uLen > 0 && uLen <= 8)
    {
        for (size_t i = 0; i<uLen; i++)
        {
            pBuf[uLen-i-1] = (uint8_t)(uVal & 0xFF);
            uVal >>= 8;
        }
    }
}

int readNextElement(FileInputStream_t *pFis, ElementHdr_t *pElementHdr)
{
    int res = ERRNO_NONE;

    ElementHdr_t xElementHdr = {0};
    uint64_t uElementLen = 0;

    do
    {
        if (pFis == NULL || pElementHdr == NULL)
        {
            res = ERRNO_FAIL;
            break;
        }

        if (pFis->uDataLen == 0 && FIS_readIntoBuf(pFis) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        xElementHdr.uIdLen = Mkv_getElementIdLen(pFis->pBuf[0]);

        if (pFis->uDataLen < xElementHdr.uIdLen && FIS_readIntoBuf(pFis) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        if (Mkv_getElementId(pFis->pBuf, pFis->uBufSize, &(xElementHdr.uId), &(xElementHdr.uIdLen)) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        if (pFis->uDataLen < xElementHdr.uIdLen + 1 && FIS_readIntoBuf(pFis) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        xElementHdr.uSizeLen = Mkv_getElementSizeLen(pFis->pBuf[xElementHdr.uIdLen]);
        if (pFis->uDataLen < xElementHdr.uIdLen + xElementHdr.uSizeLen && FIS_readIntoBuf(pFis) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        if (Mkv_getElementSize(pFis->pBuf + xElementHdr.uIdLen, pFis->uBufSize - xElementHdr.uIdLen, &(xElementHdr.uSize), &(xElementHdr.uSizeLen)) != 0)
        {
            res = ERRNO_FAIL;
            break;
        }

        /* We have a valid Element ID & SIZE here. Check the buffer size. */
        if (!(xElementHdr.uSizeLen == 1 && xElementHdr.uSize == MKV_ELEMENT_SIZE_UNKNOWN))
        {
            uElementLen = xElementHdr.uIdLen + xElementHdr.uSizeLen + xElementHdr.uSize;
            while (pFis->uDataLen < uElementLen)
            {
                if (FIS_readIntoBuf(pFis) != 0)
                {
                    res = ERRNO_FAIL;
                    break;
                }
            }
        }
    } while (0);

    if (res == ERRNO_NONE)
    {
        memcpy(pElementHdr, &xElementHdr, sizeof(ElementHdr_t));
    }

    return res;
}

int updateMkvBeginTimestamp(const char *pcSrcFilename, const char *pcDstFilename, uint64_t uTimestampMsBegin)
{
    int res = ERRNO_NONE;
    FileInputStream_t *pFis = NULL;
    FILE *fpDst = NULL;
    size_t uOffset = 0;

    ElementHdr_t xElementHdr = {0};
    size_t uConsumeDataLen = 0;
    uint8_t *pCopyData = NULL;
    size_t uCopyDataLen = 0;

    const uint8_t pElementSegmentHdr[] = {0x18, 0x53, 0x80, 0x67, 0xFF}; /* Unknown size of segment header */
    const uint8_t pElementClusterHdr[] = {0x1F, 0x43, 0xB6, 0x75, 0xFF}; /* Unknown size of cluster header */
    uint8_t pElementClusterTimestamp[] = {0xE7, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* Cluster timestamp template*/

    uint64_t uTimestampScaleMs = 0;
    uint64_t uTimestampMs = 0;

    if (pcSrcFilename == NULL || pcDstFilename == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if ((pFis = FIS_create(pcSrcFilename)) == NULL ||
             (fpDst = fopen(pcDstFilename, "wb")) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        while (readNextElement(pFis, &xElementHdr) == 0)
        {
            uConsumeDataLen = 0;
            pCopyData = NULL;
            uCopyDataLen = 0;
            /* printf("Element ID(%zu):%08X SIZE(%zu):%" PRIu64 "\r\n", xElementHdr.uIdLen, xElementHdr.uId, xElementHdr.uSizeLen, xElementHdr.uSize); */

            if (xElementHdr.uId == MKV_ELEMENT_ID_SEGMENT)
            {
                /* Go inner level of segment */
                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen;

                pCopyData = (uint8_t *)pElementSegmentHdr;
                uCopyDataLen = sizeof(pElementSegmentHdr);
            }
            else if (xElementHdr.uId == MKV_ELEMENT_ID_INFO)
            {
                /* Go inner level of info. It's for getting the timestamp scale. */
                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen;
                pCopyData = pFis->pBuf;
                uCopyDataLen = uConsumeDataLen;
            }
            else if (xElementHdr.uId == MKV_ELEMENT_ID_TIMESTAMP_SCALE)
            {
                uTimestampScaleMs = pack(pFis->pBuf + xElementHdr.uIdLen + xElementHdr.uSizeLen, xElementHdr.uSize) / 1000000;
                /* printf("uTimestampScaleMs:%" PRIu64 "\r\n", uTimestampScaleMs); */

                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen + xElementHdr.uSize;
                pCopyData = pFis->pBuf;
                uCopyDataLen = uConsumeDataLen;
            }
            else if (xElementHdr.uId == MKV_ELEMENT_ID_CLUSTER)
            {
                /* Go inner level of cluster */
                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen;
                pCopyData = (uint8_t *)pElementClusterHdr;
                uCopyDataLen = sizeof(pElementClusterHdr);
            }
            else if (xElementHdr.uId == MKV_ELEMENT_ID_TIMESTAMP)
            {
                uTimestampMs = pack(pFis->pBuf + xElementHdr.uIdLen + xElementHdr.uSizeLen, xElementHdr.uSize) * uTimestampScaleMs;
                /* printf("uTimestampMs:%" PRIu64 "\r\n", uTimestampMs); */

                uTimestampMs = (uTimestampMs + uTimestampMsBegin) / uTimestampScaleMs;
                unpack(pElementClusterTimestamp + 2, 8, uTimestampMs);
                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen + xElementHdr.uSize;
                pCopyData = pElementClusterTimestamp;
                uCopyDataLen = sizeof(pElementClusterTimestamp);
            }
            else
            {
                uConsumeDataLen = xElementHdr.uIdLen + xElementHdr.uSizeLen + xElementHdr.uSize;
                pCopyData = pFis->pBuf;
                uCopyDataLen = uConsumeDataLen;
            }

            if (pCopyData != NULL && uCopyDataLen > 0)
            {
                if (fwrite(pCopyData, 1, uCopyDataLen, fpDst) != uCopyDataLen)
                {
                    res = ERRNO_FAIL;
                    break;
                }
                else if (FIS_consumeBuf(pFis, uConsumeDataLen) != 0)
                {
                    res = ERRNO_FAIL;
                    break;
                }
            }
            else
            {
                res = ERRNO_FAIL;
                break;
            }
        }
    }

    FIS_terminate(pFis);
    if (fpDst != NULL)
    {
        fclose(fpDst);
    }

    return res;
}

void printUsage(char *pcProgramName)
{
    printf("Usage: %s -i infile -o outfile -t timestamp_ms\r\n", pcProgramName);
    printf("\r\n");
}

uint64_t convertTimestampMs(char *pcTimestampBegin)
{
    uint64_t uTimestampMs = 0;
    uint64_t uTimestampMsCurrent = 0;
    uint64_t uTimestampMsDelta = 0;
    struct timeval tv = {0};

    if (pcTimestampBegin != NULL && strlen(pcTimestampBegin) > 0)
    {
        if (pcTimestampBegin[0] == '-')
        {
            uTimestampMsDelta = strtoull(pcTimestampBegin + 1, NULL, 10);

            gettimeofday(&tv, NULL);
            uTimestampMsCurrent = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

            if (uTimestampMsCurrent > uTimestampMsDelta)
            {
                uTimestampMs = uTimestampMsCurrent - uTimestampMsDelta;
            }
        }
        else
        {
            uTimestampMs = strtoull(pcTimestampBegin, NULL, 10);
        }
    }

    return uTimestampMs;
}

int main(int argc, char *argv[])
{
    int res = 0;
    const char *optstring = "i:o:t:";
    struct option opts[] = {
        {"infile", 1, NULL, 'i'},
        {"outfile", 1, NULL, 'o'},
        {"time", 1, NULL, 't'}
    };
    int c;

    char *pcSrcFilename = NULL;
    char *pcDstFilename = NULL;
    char *pcTimestampBegin = NULL;
    uint64_t uTimestampBeginMs = 0;

    while ((c = getopt_long(argc, argv, optstring, opts, NULL)) != -1)
    {
        switch(c)
        {
            case 'i':
                pcSrcFilename = optarg;
                break;
            case 'o':
                pcDstFilename = optarg;
                break;
            case 't':
                pcTimestampBegin = optarg;
                break;
            default:
                printf("Unknown option\r\n");
        }
    }

    if (pcSrcFilename == NULL || pcDstFilename == NULL || pcTimestampBegin == NULL)
    {
        printUsage(argv[0]);
        res = -1;
    }
    else
    {
        uTimestampBeginMs = convertTimestampMs(pcTimestampBegin);

        printf("Updating begin timestamp of file \"%s\" to %" PRIu64 " milliseconds\r\n", pcSrcFilename, uTimestampBeginMs);
        if (updateMkvBeginTimestamp(pcSrcFilename, pcDstFilename, uTimestampBeginMs) == 0)
        {
            printf("Updated to file \"%s\"\r\n", pcDstFilename);
        }
        else
        {
            printf("Failed to update\r\n");
            res = -2;
        }
    }

    printf("\r\n");

    return res;
}
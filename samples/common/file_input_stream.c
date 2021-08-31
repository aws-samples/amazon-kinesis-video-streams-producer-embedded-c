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

#include "file_input_stream.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#define DEFAULT_BUFSIZE (1024)

FileInputStream_t *FIS_create(const char *pcFilename)
{
    int res = ERRNO_NONE;
    FileInputStream_t *pFis = NULL;

    if (pcFilename == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if ((pFis = (FileInputStream_t *)malloc(sizeof(FileInputStream_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pFis, 0, sizeof(FileInputStream_t));

        pFis->uBufSize = DEFAULT_BUFSIZE;

        if ((pFis->fp = fopen(pcFilename, "rb")) == NULL)
        {
            printf("Unable to open file:%s\r\n", pcFilename);
            res = ERRNO_FAIL;
        }
        else if (fseek(pFis->fp, 0L, SEEK_END) != 0 || ((pFis->xFileSize = ftell(pFis->fp)) < 0) || fseek(pFis->fp, 0L, SEEK_SET) != 0)
        {
            printf("Unable to identify the filesize\r\n");
            res = ERRNO_FAIL;
        }
        else if ((pFis->pBuf = (uint8_t *)malloc(pFis->uBufSize)) == NULL)
        {
            printf("OOM: pBuf in File Stream\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
            pFis->xFileIdx = 0;
            pFis->uDataLen = 0;
        }
    }

    if (res != ERRNO_NONE)
    {
        FIS_terminate(pFis);
        pFis = NULL;
    }

    return pFis;
}

void FIS_terminate(FileInputStream_t *pFis)
{
    if (pFis != NULL)
    {
        if (pFis->fp != NULL)
        {
            fclose(pFis->fp);
        }
        if (pFis->pBuf != NULL)
        {
            free(pFis->pBuf);
        }
        free(pFis);
    }
}

int FIS_readIntoBuf(FileInputStream_t *pFis)
{
    int res = ERRNO_NONE;
    uint8_t *pBufTemp = NULL;
    size_t uBytesRead = 0;

    if (pFis == NULL || pFis->fp == NULL || pFis->pBuf == NULL)
    {
        printf("Invalid argument in File Input Stream\r\n");
        res = ERRNO_FAIL;
    }
    else if (pFis->xFileIdx == pFis->xFileSize)
    {
        printf("No more data to read\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        if (pFis->uDataLen == pFis->uBufSize)
        {
            pBufTemp = (uint8_t *)realloc(pFis->pBuf, pFis->uBufSize * 2);
            if (pBufTemp == NULL)
            {
                printf("OOM: pBufTemp in File Input Stream\r\n");
                res = ERRNO_FAIL;
            }
            else
            {
                pFis->pBuf = pBufTemp;
                pFis->uBufSize *= 2;
            }
        }

        if (res == ERRNO_NONE)
        {
            uBytesRead = fread(pFis->pBuf + pFis->uDataLen, 1, pFis->uBufSize - pFis->uDataLen, pFis->fp);
            if (uBytesRead == 0 || uBytesRead + pFis->uDataLen > pFis->uBufSize)
            {
                printf("Failed to read\r\n");
                res = ERRNO_FAIL;
            }
            else
            {
                pFis->uDataLen += uBytesRead;
                pFis->xFileIdx += uBytesRead;
            }
        }
    }

    return res;
}

int FIS_consumeBuf(FileInputStream_t *pFis, size_t uSize)
{
    int res = ERRNO_NONE;

    if (pFis == NULL || pFis->fp == NULL || pFis->pBuf == NULL)
    {
        printf("Invalid argument in File Input Stream\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        if (uSize > 0 && uSize <= pFis->uDataLen)
        {
            memmove(pFis->pBuf, pFis->pBuf + uSize, pFis->uDataLen - uSize);
            pFis->uDataLen -= uSize;
        }
    }

    return res;
}
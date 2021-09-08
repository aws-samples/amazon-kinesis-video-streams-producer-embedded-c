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

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kvs/mkv_generator.h"
#include "kvs/nalu.h"

#include "g711_file_loader.h"
#include "file_io.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#ifndef SAFE_FREE
#    define SAFE_FREE(a)                                                                                                                                                           \
        do                                                                                                                                                                         \
        {                                                                                                                                                                          \
            free(a);                                                                                                                                                           \
            a = NULL;                                                                                                                                                              \
        } while (0)
#endif /* SAFE_FREE */

#define CODEC_NAME "A_MS/ACM"

typedef struct G711FileLoader
{
    char *pcTrackName;
    char *pcFileFormat;

    int xFileCurrentIdx;

    int xFileStartIdx;
    int xFileEndIdx;
    bool bKeepRotate;
    bool bStopLoading;

    AudioTrackInfo_t xAudioTrackInfo;
} G711FileLoader_t;

static int loadFrame(G711FileLoader_t *pLoader, char **ppData, size_t *puDataLen)
{
    int res = ERRNO_NONE;
    char *pcFilename = NULL;
    size_t uFilenameLen = 0;
    char *pData = NULL;
    size_t uDataLen = 0;

    if (pLoader == NULL || ppData == NULL || puDataLen == NULL)
    {
        printf("Invalid argument while loading frame\r\n");
        res = ERRNO_FAIL;
    }
    else if (pLoader->bStopLoading)
    {
        printf("File loader has stopped loading\r\n");
        res = ERRNO_FAIL;
    }
    else if (
        (uFilenameLen = snprintf(NULL, 0, pLoader->pcFileFormat, pLoader->xFileCurrentIdx)) == 0 || (pcFilename = (char *)malloc(uFilenameLen + 1)) == NULL ||
        snprintf(pcFilename, uFilenameLen + 1, pLoader->pcFileFormat, pLoader->xFileCurrentIdx) != uFilenameLen)
    {
        printf("Unable to setup filename\r\n");
        res = ERRNO_FAIL;
    }
    else if (getFileSize(pcFilename, &uDataLen) != 0 || (pData = (char *)malloc(uDataLen)) == NULL || readFile(pcFilename, pData, uDataLen, &uDataLen) != 0)
    {
        printf("Unable to load data frame: %s\r\n", pcFilename);
        res = ERRNO_FAIL;
    }
    else
    {
        *ppData = pData;
        *puDataLen = uDataLen;
    }

    if (res != ERRNO_NONE)
    {
        SAFE_FREE(pData);
    }
    SAFE_FREE(pcFilename);

    return res;
}

static int initializeAudioTrackInfo(G711FileLoader_t *pLoader, PcmFormatCode_t xObjectType, uint32_t uFrequency, uint16_t uChannelNumber)
{
    int res = ERRNO_NONE;
    AudioTrackInfo_t *pAudioTrackInfo = NULL;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateDataLen = 0;

    if (pLoader == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pAudioTrackInfo = &(pLoader->xAudioTrackInfo);
        pAudioTrackInfo->pTrackName = pLoader->pcTrackName;
        pAudioTrackInfo->pCodecName = CODEC_NAME;
        pAudioTrackInfo->uFrequency = uFrequency;
        pAudioTrackInfo->uChannelNumber = uChannelNumber;

        if (Mkv_generatePcmCodecPrivateData(xObjectType, uFrequency, uChannelNumber, &pCodecPrivateData, &uCodecPrivateDataLen) == 0)
        {
            pAudioTrackInfo->pCodecPrivate = pCodecPrivateData;
            pAudioTrackInfo->uCodecPrivateLen = (uint32_t)uCodecPrivateDataLen;
        }
    }

    return res;
}

G711FileLoaderHandle G711FileLoaderCreate(FileLoaderPara_t *pFileLoaderPara, PcmFormatCode_t xObjectType, uint32_t uFrequency, uint16_t uChannelNumber)
{
    int res = ERRNO_NONE;
    G711FileLoader_t *pLoader = NULL;
    size_t uStLen = 0;

    if (pFileLoaderPara == NULL || pFileLoaderPara->pcTrackName == NULL || pFileLoaderPara->pcFileFormat == NULL || pFileLoaderPara->xFileStartIdx < 0)
    {
        printf("Invalid H264 File Loader arguments while creating\r\n");
        res = ERRNO_FAIL;
    }
    else if ((pLoader = (G711FileLoader_t *)malloc(sizeof(G711FileLoader_t))) == NULL)
    {
        printf("OOM: pLoader in G711 File Loader\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pLoader, 0, sizeof(G711FileLoader_t));

        if ((uStLen = strlen(pFileLoaderPara->pcTrackName)) == 0 || (pLoader->pcTrackName = (char *)malloc(uStLen + 1)) == NULL ||
            snprintf(pLoader->pcTrackName, uStLen + 1, "%s", pFileLoaderPara->pcTrackName) != uStLen)
        {
            printf("Failed to init track name in AAC File Loader\r\n");
            res = ERRNO_FAIL;
        }
        else if (
            (uStLen = strlen(pFileLoaderPara->pcFileFormat)) == 0 || (pLoader->pcFileFormat = (char *)malloc(uStLen + 1)) == NULL ||
            snprintf(pLoader->pcFileFormat, uStLen + 1, "%s", pFileLoaderPara->pcFileFormat) != uStLen)
        {
            printf("Failed to init file format in AAC File Loader\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
            pLoader->xFileStartIdx = pFileLoaderPara->xFileStartIdx;
            pLoader->xFileCurrentIdx = pFileLoaderPara->xFileStartIdx;
            pLoader->xFileEndIdx = pFileLoaderPara->xFileEndIdx;
            pLoader->bKeepRotate = pFileLoaderPara->bKeepRotate;
            pLoader->bStopLoading = false;
            if (initializeAudioTrackInfo(pLoader, xObjectType, uFrequency, uChannelNumber) != 0)
            {
                printf("Failed to initialize video track info\r\n");
                res = ERRNO_FAIL;
            }
        }
    }

    if (res != ERRNO_NONE)
    {
        G711FileLoaderTerminate(pLoader);
        pLoader = NULL;
    }

    return pLoader;
}

void G711FileLoaderTerminate(G711FileLoaderHandle xLoader)
{
    G711FileLoader_t *pLoader = xLoader;

    if (pLoader != NULL)
    {
        SAFE_FREE(pLoader->xAudioTrackInfo.pCodecPrivate);
        SAFE_FREE(pLoader->pcTrackName);
        SAFE_FREE(pLoader->pcFileFormat);
        free(pLoader);
    }
}

int G711FileLoaderLoadFrame(G711FileLoaderHandle xLoader, char **ppData, size_t *puDataLen)
{
    int res = ERRNO_NONE;
    G711FileLoader_t *pLoader = xLoader;
    char *pData = NULL;
    size_t uDataLen = 0;

    if (pLoader == NULL || ppData == NULL || puDataLen == NULL)
    {
        printf("Invalid argument while loading frame\r\n");
        res = ERRNO_FAIL;
    }
    else if (loadFrame(pLoader, &pData, &uDataLen) != 0)
    {
        printf("Failed to load frame\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        *ppData = pData;
        *puDataLen = uDataLen;

        pLoader->xFileCurrentIdx = pLoader->xFileCurrentIdx + 1;
        if (pLoader->xFileEndIdx > 0 && pLoader->xFileCurrentIdx == pLoader->xFileEndIdx + 1)
        {
            if (pLoader->bKeepRotate)
            {
                pLoader->xFileCurrentIdx = pLoader->xFileStartIdx;
            }
            else
            {
                pLoader->bStopLoading = true;
            }
        }
    }

    if (res != ERRNO_NONE)
    {
        SAFE_FREE(pData);
    }

    return res;
}

AudioTrackInfo_t *G711FileLoaderGetAudioTrackInfo(G711FileLoaderHandle xLoader)
{
    AudioTrackInfo_t *pAudioTrackInfo = NULL;
    G711FileLoader_t *pLoader = xLoader;

    if (pLoader != NULL)
    {
        pAudioTrackInfo = &(pLoader->xAudioTrackInfo);
    }

    return pAudioTrackInfo;
}
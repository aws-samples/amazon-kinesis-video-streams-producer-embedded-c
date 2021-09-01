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

#include "file_io.h"
#include "h264_file_loader.h"

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

#ifndef SAFE_FREE
#define SAFE_FREE(a)    \
    do                  \
    {                   \
        free(a);        \
        a = NULL;       \
    } while (0)
#endif /* SAFE_FREE */

#define CODEC_NAME      "V_MPEG4/ISO/AVC"

#define ANNEXB_TO_AVCC_EXTRA_MEMSIZE    (32)

typedef struct H264FileLoader
{
    char *pcTrackName;
    char *pcFileFormat;

    int xFileCurrentIdx;

    int xFileStartIdx;
    int xFileEndIdx;
    bool bKeepRotate;
    bool bStopLoading;

    VideoTrackInfo_t xVideoTrackInfo;
} H264FileLoader_t;

static int loadFrame(H264FileLoader_t *pLoader, char **ppData, size_t *puDataLen)
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
        printf("File loader has stopped loading");
        res = ERRNO_FAIL;
    }
    else if (
        (uFilenameLen = snprintf(NULL, 0, pLoader->pcFileFormat, pLoader->xFileCurrentIdx)) == 0 || (pcFilename = (char *)malloc(uFilenameLen + 1)) == NULL ||
        snprintf(pcFilename, uFilenameLen + 1, pLoader->pcFileFormat, pLoader->xFileCurrentIdx) != uFilenameLen)
    {
        printf("Unable to setup filename\r\n");
        res = ERRNO_FAIL;
    }
    else if (
        getFileSize(pcFilename, &uDataLen) != 0 || (pData = (char *)malloc(uDataLen + ANNEXB_TO_AVCC_EXTRA_MEMSIZE)) == NULL ||
        readFile(pcFilename, pData, uDataLen + ANNEXB_TO_AVCC_EXTRA_MEMSIZE, &uDataLen) != 0)
    {
        printf("Unable to load data frame: %s\r\n", pcFilename);
        res = ERRNO_FAIL;
    }
    else if (
        NALU_isAnnexBFrame((uint8_t *)pData, uDataLen) &&
        NALU_convertAnnexBToAvccInPlace((uint8_t *)pData, uDataLen, uDataLen + ANNEXB_TO_AVCC_EXTRA_MEMSIZE, (uint32_t *)&uDataLen) != 0)
    {
        printf("Failed to convert frame from Annex-B to AVCC\r\n");
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

static int initializeVideoTrackInfo(H264FileLoader_t *pLoader)
{
    int res = ERRNO_NONE;

    char *pData = NULL;
    size_t uDataLen = 0;

    uint8_t *pSps = NULL;
    size_t uSpsLen = 0;

    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateDataLen = 0;

    int xFileCurrentIdxBackup = 0;
    bool bVideoTrackInfoInitialized = false;

    if (pLoader == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pLoader->xVideoTrackInfo.pTrackName = pLoader->pcTrackName;
        pLoader->xVideoTrackInfo.pCodecName = CODEC_NAME;

        xFileCurrentIdxBackup = pLoader->xFileCurrentIdx;

        pLoader->xFileCurrentIdx = pLoader->xFileStartIdx;
        while (!bVideoTrackInfoInitialized && pLoader->xFileCurrentIdx != pLoader->xFileEndIdx)
        {
            if (loadFrame(pLoader, &pData, &uDataLen) != 0)
            {
                printf("Unable to load frame to generate video track info\r\n");
                res = ERRNO_FAIL;
                break;
            }
            else
            {
                if (NALU_getNaluFromAvccNalus((uint8_t *)pData, uDataLen, NALU_TYPE_SPS, &pSps, &uSpsLen) == 0 &&
                    NALU_getH264VideoResolutionFromSps(pSps, uSpsLen, &(pLoader->xVideoTrackInfo.uWidth), &(pLoader->xVideoTrackInfo.uHeight)) == 0 &&
                    Mkv_generateH264CodecPrivateDataFromAvccNalus((uint8_t *)pData, uDataLen, &pCodecPrivateData, &uCodecPrivateDataLen) == 0)
                {
                    pLoader->xVideoTrackInfo.pCodecPrivate = pCodecPrivateData;
                    pLoader->xVideoTrackInfo.uCodecPrivateLen = (uint32_t)uCodecPrivateDataLen;
                    bVideoTrackInfoInitialized = true;
                }

                free(pData);
            }
        }

        pLoader->xFileCurrentIdx = xFileCurrentIdxBackup;
    }

    return res;
}

H264FileLoaderHandle H264FileLoaderCreate(FileLoaderPara_t *pFileLoaderPara)
{
    int res = ERRNO_NONE;
    H264FileLoader_t *pLoader = NULL;
    size_t uStLen = 0;

    if (pFileLoaderPara == NULL || pFileLoaderPara->pcTrackName == NULL || pFileLoaderPara->pcFileFormat == NULL || pFileLoaderPara->xFileStartIdx < 0)
    {
        printf("Invalid H264 File Loader arguments while creating\r\n");
        res = ERRNO_FAIL;
    }
    else if ((pLoader = (H264FileLoader_t *)malloc(sizeof(H264FileLoader_t))) == NULL)
    {
        printf("OOM: pLoader in H264 File Loader\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pLoader, 0, sizeof(H264FileLoader_t));

        if ((uStLen = strlen(pFileLoaderPara->pcTrackName)) == 0 || (pLoader->pcTrackName = (char *)malloc(uStLen + 1)) == NULL ||
            snprintf(pLoader->pcTrackName, uStLen + 1, "%s", pFileLoaderPara->pcTrackName) != uStLen)
        {
            printf("Failed to init track name in H264 File Loader\r\n");
            res = ERRNO_FAIL;
        }
        else if (
            (uStLen = strlen(pFileLoaderPara->pcFileFormat)) == 0 || (pLoader->pcFileFormat = (char *)malloc(uStLen + 1)) == NULL ||
            snprintf(pLoader->pcFileFormat, uStLen + 1, "%s", pFileLoaderPara->pcFileFormat) != uStLen)
        {
            printf("Failed to init file format in H264 File Loader\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
            pLoader->xFileStartIdx = pFileLoaderPara->xFileStartIdx;
            pLoader->xFileCurrentIdx = pFileLoaderPara->xFileStartIdx;
            pLoader->xFileEndIdx = pFileLoaderPara->xFileEndIdx;
            pLoader->bKeepRotate = pFileLoaderPara->bKeepRotate;
            pLoader->bStopLoading = false;
            if (initializeVideoTrackInfo(pLoader) != 0)
            {
                printf("Failed to initialize video track info\r\n");
                res = ERRNO_FAIL;
            }
        }
    }

    if (res != ERRNO_NONE)
    {
        H264FileLoaderTerminate(pLoader);
        pLoader = NULL;
    }

    return pLoader;
}

void H264FileLoaderTerminate(H264FileLoaderHandle xLoader)
{
    H264FileLoader_t *pLoader = xLoader;

    if (pLoader != NULL)
    {
        SAFE_FREE(pLoader->xVideoTrackInfo.pCodecPrivate);
        SAFE_FREE(pLoader->pcTrackName);
        SAFE_FREE(pLoader->pcFileFormat);
        free(pLoader);
    }
}

int H264FileLoaderLoadFrame(H264FileLoaderHandle xLoader, char **ppData, size_t *puDataLen)
{
    int res = ERRNO_NONE;
    H264FileLoader_t *pLoader = xLoader;
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

VideoTrackInfo_t *H264FileLoaderGetVideoTrackInfo(H264FileLoaderHandle xLoader)
{
    VideoTrackInfo_t *pVideoTrackInfo = NULL;
    H264FileLoader_t *pLoader = xLoader;

    if (pLoader != NULL)
    {
        pVideoTrackInfo = &(pLoader->xVideoTrackInfo);
    }

    return pVideoTrackInfo;
}
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
#include <string.h>
#include <unistd.h>

/* Third-party headers */
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/xlogging.h"

/* KVS headers */
#include "kvs/allocator.h"
#include "kvs/iot_credential_provider.h"
#include "kvs/nalu.h"
#include "kvs/port.h"
#include "kvs/restapi.h"
#include "kvs/stream.h"

#include "kvs/kvsapp.h"
#include "kvs/kvsapp_options.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#define VIDEO_CODEC_NAME "V_MPEG4/ISO/AVC"
#define VIDEO_TRACK_NAME "kvs video track"

#define DEFAULT_RING_BUFFER_MEM_LIMIT (1 * 1024 * 1024)

typedef struct PolicyRingBufferParameter
{
    size_t uMemLimit;
} PolicyRingBufferParameter_t;

typedef struct StreamStrategy
{
    KvsApp_streamPolicy_t xPolicy;
    union
    {
        PolicyRingBufferParameter_t xRingBufferPara;
    };
} StreamStrategy_t;

typedef struct KvsApp
{
    LOCK_HANDLE xLock;

    char *pHost;
    char *pRegion;
    char *pService;
    char *pStreamName;
    char *pDataEndpoint;

    /* AWS access key and secret */
    char *pAwsAccessKeyId;
    char *pAwsSecretAccessKey;

    /* Iot certificates */
    char *pIotCredentialHost;
    char *pIotRoleAlias;
    char *pIotThingName;
    char *pIotX509RootCa;
    char *pIotX509Certificate;
    char *pIotX509PrivateKey;
    IotCredentialToken_t *pToken;

    /* Restful request parameters */
    KvsServiceParameter_t xServicePara;
    KvsDescribeStreamParameter_t xDescPara;
    KvsCreateStreamParameter_t xCreatePara;
    KvsGetDataEndpointParameter_t xGetDataEpPara;
    KvsPutMediaParameter_t xPutMediaPara;

    unsigned int uDataRetentionInHours;

    /* KVS streaming variables */
    uint64_t uEarliestTimestamp;
    StreamHandle xStreamHandle;
    PutMediaHandle xPutMediaHandle;
    bool isEbmlHeaderUpdated;
    StreamStrategy_t xStrategy;

    /* Track information */
    VideoTrackInfo_t *pVideoTrackInfo;
    uint8_t *pSps;
    size_t uSpsLen;
    uint8_t *pPps;
    size_t uPpsLen;

    bool isAudioTrackPresent;
    AudioTrackInfo_t *pAudioTrackInfo;
} KvsApp_t;

static void prvSleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static void prvVideoTrackInfoTerminate(VideoTrackInfo_t *pVideoTrackInfo)
{
    if (pVideoTrackInfo != NULL)
    {
        if (pVideoTrackInfo->pTrackName != NULL)
        {
            KVS_FREE(pVideoTrackInfo->pTrackName);
        }
        if (pVideoTrackInfo->pCodecName != NULL)
        {
            KVS_FREE(pVideoTrackInfo->pCodecName);
        }
        if (pVideoTrackInfo->pCodecPrivate != NULL)
        {
            KVS_FREE(pVideoTrackInfo->pCodecPrivate);
        }
        memset(pVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));
        KVS_FREE(pVideoTrackInfo);
    }
}

static void prvAudioTrackInfoTerminate(AudioTrackInfo_t *pAudioTrackInfo)
{
    if (pAudioTrackInfo != NULL)
    {
        if (pAudioTrackInfo->pTrackName != NULL)
        {
            KVS_FREE(pAudioTrackInfo->pTrackName);
        }
        if (pAudioTrackInfo->pCodecName != NULL)
        {
            KVS_FREE(pAudioTrackInfo->pCodecName);
        }
        if (pAudioTrackInfo->pCodecPrivate != NULL)
        {
            KVS_FREE(pAudioTrackInfo->pCodecPrivate);
        }
        memset(pAudioTrackInfo, 0, sizeof(VideoTrackInfo_t));
        KVS_FREE(pAudioTrackInfo);
    }
}

static int prvBufMallocAndCopy(uint8_t **ppDst, size_t *puDstLen, uint8_t *pSrc, size_t uSrcLen)
{
    int res = ERRNO_NONE;
    uint8_t *pDst = NULL;
    size_t uDstLen = 0;

    if (ppDst == NULL || puDstLen == NULL || pSrc == NULL || uSrcLen == 0)
    {
        res = ERRNO_FAIL;
    }
    else if ((pDst = (uint8_t *)KVS_MALLOC(uSrcLen)) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memcpy(pDst, pSrc, uSrcLen);
        *ppDst = pDst;
        *puDstLen = uSrcLen;
    }

    if (res != ERRNO_NONE)
    {
        if (pDst != NULL)
        {
            KVS_FREE(pDst);
        }
    }

    return res;
}

static void prvStreamFlush(KvsApp_t *pKvs)
{
    StreamHandle xStreamHandle = pKvs->xStreamHandle;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while ((xDataFrameHandle = Kvs_streamPop(xStreamHandle)) != NULL)
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        KVS_FREE(pDataFrameIn->pData);
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static void prvStreamFlushToNextCluster(KvsApp_t *pKvs)
{
    StreamHandle xStreamHandle = pKvs->xStreamHandle;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while (1)
    {
        xDataFrameHandle = Kvs_streamPeek(xStreamHandle);
        if (xDataFrameHandle == NULL)
        {
            prvSleepInMs(50);
        }
        else
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            if (pDataFrameIn->xClusterType == MKV_CLUSTER)
            {
                pKvs->uEarliestTimestamp = pDataFrameIn->uTimestampMs;
                break;
            }
            else
            {
                xDataFrameHandle = Kvs_streamPop(xStreamHandle);
                pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
                KVS_FREE(pDataFrameIn->pData);
                Kvs_dataFrameTerminate(xDataFrameHandle);
            }
        }
    }
}

static void prvStreamFlushHeadUntilMem(KvsApp_t *pKvs, size_t uMemLimit)
{
    StreamHandle xStreamHandle = pKvs->xStreamHandle;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;
    size_t uMemTotal = 0;

    while (Kvs_streamMemStatTotal(xStreamHandle, &uMemTotal) == 0 && uMemTotal > uMemLimit && (xDataFrameHandle = Kvs_streamPop(xStreamHandle)) != NULL)
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        KVS_FREE(pDataFrameIn->pData);
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static VideoTrackInfo_t *prvCopyVideoTrackInfo(VideoTrackInfo_t *pSrcVideoTrackInfo)
{
    int res = ERRNO_NONE;
    VideoTrackInfo_t *pDstVideoTrackInfo = NULL;

    if ((pDstVideoTrackInfo = (VideoTrackInfo_t *)KVS_MALLOC(sizeof(VideoTrackInfo_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pDstVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));

        if (mallocAndStrcpy_s(&(pDstVideoTrackInfo->pTrackName), pSrcVideoTrackInfo->pTrackName) != 0 ||
            mallocAndStrcpy_s(&(pDstVideoTrackInfo->pCodecName), pSrcVideoTrackInfo->pCodecName) != 0 ||
            (pDstVideoTrackInfo->pCodecPrivate = (uint8_t *)KVS_MALLOC(pSrcVideoTrackInfo->uCodecPrivateLen)) == NULL)
        {
            res = ERRNO_FAIL;
        }
        else
        {
            pDstVideoTrackInfo->uWidth = pSrcVideoTrackInfo->uWidth;
            pDstVideoTrackInfo->uHeight = pSrcVideoTrackInfo->uHeight;

            memcpy(pDstVideoTrackInfo->pCodecPrivate, pSrcVideoTrackInfo->pCodecPrivate, pSrcVideoTrackInfo->uCodecPrivateLen);
            pDstVideoTrackInfo->uCodecPrivateLen = pSrcVideoTrackInfo->uCodecPrivateLen;
        }
    }

    if (res != ERRNO_NONE)
    {
        prvVideoTrackInfoTerminate(pDstVideoTrackInfo);
        pDstVideoTrackInfo = NULL;
    }

    return pDstVideoTrackInfo;
}

static AudioTrackInfo_t *prvCopyAudioTrackInfo(AudioTrackInfo_t *pSrcAudioTrackInfo)
{
    int res = ERRNO_NONE;
    AudioTrackInfo_t *pDstAudioTrackInfo = NULL;

    if ((pDstAudioTrackInfo = (AudioTrackInfo_t *)KVS_MALLOC(sizeof(AudioTrackInfo_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pDstAudioTrackInfo, 0, sizeof(AudioTrackInfo_t));

        if (mallocAndStrcpy_s(&(pDstAudioTrackInfo->pTrackName), pSrcAudioTrackInfo->pTrackName) != 0 ||
            mallocAndStrcpy_s(&(pDstAudioTrackInfo->pCodecName), pSrcAudioTrackInfo->pCodecName) != 0 ||
            (pDstAudioTrackInfo->pCodecPrivate = (uint8_t *)KVS_MALLOC(pSrcAudioTrackInfo->uCodecPrivateLen)) == NULL)
        {
            res = ERRNO_FAIL;
        }
        else
        {
            pDstAudioTrackInfo->uFrequency = pSrcAudioTrackInfo->uFrequency;
            pDstAudioTrackInfo->uChannelNumber = pSrcAudioTrackInfo->uChannelNumber;
            pDstAudioTrackInfo->uBitsPerSample = pSrcAudioTrackInfo->uBitsPerSample;

            memcpy(pDstAudioTrackInfo->pCodecPrivate, pSrcAudioTrackInfo->pCodecPrivate, pSrcAudioTrackInfo->uCodecPrivateLen);
            pDstAudioTrackInfo->uCodecPrivateLen = pSrcAudioTrackInfo->uCodecPrivateLen;
        }
    }

    if (res != ERRNO_NONE)
    {
        prvAudioTrackInfoTerminate(pDstAudioTrackInfo);
        pDstAudioTrackInfo = NULL;
    }

    return pDstAudioTrackInfo;
}

static bool isIotCertAvailable(KvsApp_t *pKvs)
{
    if (pKvs->pIotCredentialHost != NULL && pKvs->pIotRoleAlias != NULL && pKvs->pIotThingName != NULL && pKvs->pIotX509RootCa != NULL && pKvs->pIotX509Certificate != NULL &&
        pKvs->pIotX509PrivateKey != NULL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void updateIotCredential(KvsApp_t *pKvs)
{
    IotCredentialToken_t *pToken = NULL;
    IotCredentialRequest_t xIotCredentialReq = {
        .pCredentialHost = pKvs->pIotCredentialHost,
        .pRoleAlias = pKvs->pIotRoleAlias,
        .pThingName = pKvs->pIotThingName,
        .pRootCA = pKvs->pIotX509RootCa,
        .pCertificate = pKvs->pIotX509Certificate,
        .pPrivateKey = pKvs->pIotX509PrivateKey};

    if (isIotCertAvailable(pKvs))
    {
        Iot_credentialTerminate(pKvs->pToken);
        pKvs->pToken = NULL;

        if ((pToken = Iot_getCredential(&xIotCredentialReq)) == NULL)
        {
            LogError("Failed to get Iot credential");
        }
        else
        {
            pKvs->pToken = pToken;
        }
    }
}

static int updateAndVerifyRestfulReqParameters(KvsApp_t *pKvs)
{
    int res = ERRNO_NONE;

    pKvs->xServicePara.pcHost = pKvs->pHost;
    pKvs->xServicePara.pcRegion = pKvs->pRegion;
    pKvs->xServicePara.pcService = pKvs->pService;

    if (pKvs->pToken != NULL)
    {
        pKvs->xServicePara.pcAccessKey = pKvs->pToken->pAccessKeyId;
        pKvs->xServicePara.pcSecretKey = pKvs->pToken->pSecretAccessKey;
        pKvs->xServicePara.pcToken = pKvs->pToken->pSessionToken;
    }
    else
    {
        if (pKvs->pAwsAccessKeyId == NULL || pKvs->pAwsSecretAccessKey == NULL)
        {
            LogError("No available aws access key");
            res = ERRNO_FAIL;
        }
        else
        {
            pKvs->xServicePara.pcAccessKey = pKvs->pAwsAccessKeyId;
            pKvs->xServicePara.pcSecretKey = pKvs->pAwsSecretAccessKey;
            pKvs->xServicePara.pcToken = NULL;
        }
    }

    if (res == ERRNO_NONE)
    {
        pKvs->xDescPara.pcStreamName = pKvs->pStreamName;

        pKvs->xCreatePara.pcStreamName = pKvs->pStreamName;
        pKvs->xCreatePara.uDataRetentionInHours = pKvs->uDataRetentionInHours;

        pKvs->xGetDataEpPara.pcStreamName = pKvs->pStreamName;

        pKvs->xPutMediaPara.pcStreamName = pKvs->pStreamName;
        pKvs->xPutMediaPara.xTimecodeType = TIMECODE_TYPE_ABSOLUTE;
    }

    return res;
}

static int setupDataEndpoint(KvsApp_t *pKvs)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
        }
        else
        {
            LogInfo("Try to describe stream");
            if (Kvs_describeStream(&(pKvs->xServicePara), &(pKvs->xDescPara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
            {
                LogInfo("Failed to describe stream, status code:%u", uHttpStatusCode);
                LogInfo("Try to create stream");
                if (Kvs_createStream(&(pKvs->xServicePara), &(pKvs->xCreatePara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
                {
                    LogInfo("Failed to create stream, status code:%u", uHttpStatusCode);
                    res = ERRNO_FAIL;
                }
            }

            if (res == ERRNO_NONE)
            {
                if (Kvs_getDataEndpoint(&(pKvs->xServicePara), &(pKvs->xGetDataEpPara), &uHttpStatusCode, &(pKvs->pDataEndpoint)) != 0 || uHttpStatusCode != 200)
                {
                    LogInfo("Failed to get data endpoint, status code:%u", uHttpStatusCode);
                    res = ERRNO_FAIL;
                }
                else
                {
                    pKvs->xServicePara.pcPutMediaEndpoint = pKvs->pDataEndpoint;
                }
            }
        }
    }

    if (res == ERRNO_NONE)
    {
        LogInfo("PUT MEDIA endpoint: %s", pKvs->xServicePara.pcPutMediaEndpoint);
    }

    return res;
}

static int updateEbmlHeader(KvsApp_t *pKvs)
{
    int res = ERRNO_NONE;
    uint8_t *pEbmlSeg = NULL;
    size_t uEbmlSegLen = 0;

    if (pKvs->xPutMediaHandle != NULL && !pKvs->isEbmlHeaderUpdated)
    {
        LogInfo("Flush to next cluster");
        prvStreamFlushToNextCluster(pKvs);

        if (Kvs_streamGetMkvEbmlSegHdr(pKvs->xStreamHandle, &pEbmlSeg, &uEbmlSegLen) != 0 || Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, pEbmlSeg, uEbmlSegLen) != 0)
        {
            LogError("Failed to update ebml header");
            res = ERRNO_FAIL;
        }
        else
        {
            pKvs->isEbmlHeaderUpdated = true;
        }
    }

    return res;
}

static int createStream(KvsApp_t *pKvs)
{
    int res = ERRNO_NONE;
    VideoTrackInfo_t xVideoTrackInfo = {0};
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateDataLen = 0;

    if (pKvs->xStreamHandle == NULL)
    {
        if (pKvs->pVideoTrackInfo == NULL && pKvs->pSps != NULL && pKvs->pPps != NULL)
        {
            if (NALU_getH264VideoResolutionFromSps(pKvs->pSps, pKvs->uSpsLen, &(xVideoTrackInfo.uWidth), &(xVideoTrackInfo.uHeight)) != ERRNO_NONE ||
                Mkv_generateH264CodecPrivateDataFromSpsPps(pKvs->pSps, pKvs->uSpsLen, pKvs->pPps, pKvs->uPpsLen, &pCodecPrivateData, &uCodecPrivateDataLen) != ERRNO_NONE)
            {
                LogError("Failed to generate video track info");
            }
            else
            {
                xVideoTrackInfo.pTrackName = VIDEO_TRACK_NAME;
                xVideoTrackInfo.pCodecName = VIDEO_CODEC_NAME;
                xVideoTrackInfo.pCodecPrivate = pCodecPrivateData;
                xVideoTrackInfo.uCodecPrivateLen = uCodecPrivateDataLen;
                pKvs->pVideoTrackInfo = prvCopyVideoTrackInfo(&xVideoTrackInfo);
            }

            if (pCodecPrivateData != NULL)
            {
                KVS_FREE(pCodecPrivateData);
            }
        }

        if (pKvs->pVideoTrackInfo != NULL)
        {
            if ((pKvs->xStreamHandle = Kvs_streamCreate(pKvs->pVideoTrackInfo, pKvs->pAudioTrackInfo)) == NULL)
            {
                res = ERRNO_FAIL;
            }
            else
            {
                LogInfo("KVS stream buffer created");

                pKvs->isAudioTrackPresent = (pKvs->pAudioTrackInfo != NULL);
            }
        }
    }

    return res;
}

static int checkAndBuildStream(KvsApp_t *pKvs, uint8_t *pData, size_t uDataLen, TrackType_t xTrackType)
{
    int res = ERRNO_NONE;
    uint8_t *pSps = NULL;
    size_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    size_t uPpsLen = 0;

    if (pKvs->xStreamHandle == NULL)
    {
        /* Try to build video track info from frames. */
        if (pKvs->pVideoTrackInfo == NULL && xTrackType == TRACK_VIDEO)
        {
            if (pKvs->pSps == NULL && NALU_getNaluFromAvccNalus(pData, uDataLen, NALU_TYPE_SPS, &pSps, &uSpsLen) == ERRNO_NONE)
            {
                LogInfo("SPS is found");
                if (prvBufMallocAndCopy(&(pKvs->pSps), &(pKvs->uSpsLen), pSps, uSpsLen) != ERRNO_NONE)
                {
                    res = ERRNO_FAIL;
                }
                else
                {
                    LogInfo("SPS is set");
                }
            }
            if (pKvs->pPps == NULL && NALU_getNaluFromAvccNalus(pData, uDataLen, NALU_TYPE_PPS, &pPps, &uPpsLen) == ERRNO_NONE)
            {
                LogInfo("PPS is found");
                if (prvBufMallocAndCopy(&(pKvs->pPps), &(pKvs->uPpsLen), pPps, uPpsLen) != ERRNO_NONE)
                {
                    res = ERRNO_FAIL;
                }
                else
                {
                    LogInfo("PPS is set");
                }
            }
        }

        if (pKvs->pSps != NULL && pKvs->pPps != NULL)
        {
            createStream(pKvs);
        }
    }

    return res;
}

static int prvPutMediaSendData(KvsApp_t *pKvs, int *pxSendCnt)
{
    int res = 0;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint8_t *pMkvHeader = NULL;
    size_t uMkvHeaderLen = 0;
    int xSendCnt = 0;

    if (Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_VIDEO) && (!pKvs->isAudioTrackPresent || Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_AUDIO)))
    {
        if ((xDataFrameHandle = Kvs_streamPop(pKvs->xStreamHandle)) == NULL)
        {
            LogError("Failed to get data frame");
            res = ERRNO_FAIL;
        }
        else if (Kvs_dataFrameGetContent(xDataFrameHandle, &pMkvHeader, &uMkvHeaderLen, &pData, &uDataLen) != 0)
        {
            LogError("Failed to get data and mkv header to send");
            res = ERRNO_FAIL;
        }
        else if (Kvs_putMediaUpdate(pKvs->xPutMediaHandle, pMkvHeader, uMkvHeaderLen, pData, uDataLen) != 0)
        {
            LogError("Failed to update");
            res = ERRNO_FAIL;
        }
        else
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            pKvs->uEarliestTimestamp = pDataFrameIn->uTimestampMs;

            xSendCnt++;
        }

        if (xDataFrameHandle != NULL)
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            KVS_FREE(pDataFrameIn->pData);
            Kvs_dataFrameTerminate(xDataFrameHandle);
        }
    }

    if (pxSendCnt != NULL)
    {
        *pxSendCnt = xSendCnt;
    }

    return res;
}

KvsAppHandle KvsApp_create(char *pcHost, char *pcRegion, char *pcService, char *pcStreamName)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = NULL;

    if (pcHost == NULL || pcRegion == NULL || pcService == NULL || pcStreamName == NULL)
    {
        LogError("Invalid parameter");
        res = ERRNO_FAIL;
    }
    else if ((pKvs = (KvsApp_t *)KVS_MALLOC(sizeof(KvsApp_t))) == NULL)
    {
        LogError("OOM: pKvs");
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pKvs, 0, sizeof(KvsApp_t));

        if ((pKvs->xLock = Lock_Init()) == NULL)
        {
            LogError("Failed to init lock");
            res = ERRNO_FAIL;
        }
        else if (
            mallocAndStrcpy_s(&(pKvs->pHost), pcHost) != 0 || mallocAndStrcpy_s(&(pKvs->pRegion), pcRegion) != 0 || mallocAndStrcpy_s(&(pKvs->pService), pcService) != 0 ||
            mallocAndStrcpy_s(&(pKvs->pStreamName), pcStreamName) != 0)
        {
            LogError("OOM: parameters");
            res = ERRNO_FAIL;
        }
        else
        {
            pKvs->pDataEndpoint = NULL;
            pKvs->pAwsAccessKeyId = NULL;
            pKvs->pAwsSecretAccessKey = NULL;
            pKvs->pIotCredentialHost = NULL;
            pKvs->pIotRoleAlias = NULL;
            pKvs->pIotThingName = NULL;
            pKvs->pIotX509RootCa = NULL;
            pKvs->pIotX509Certificate = NULL;
            pKvs->pIotX509PrivateKey = NULL;
            pKvs->pToken = NULL;

            pKvs->uDataRetentionInHours = 0;

            pKvs->uEarliestTimestamp = 0;
            pKvs->xStreamHandle = NULL;
            pKvs->xPutMediaHandle = NULL;
            pKvs->isEbmlHeaderUpdated = false;
            pKvs->xStrategy.xPolicy = STREAM_POLICY_NONE;

            pKvs->pVideoTrackInfo = NULL;
            pKvs->isAudioTrackPresent = false;
            pKvs->pAudioTrackInfo = NULL;
        }
    }

    return (KvsAppHandle)pKvs;
}

void KvsApp_terminate(KvsAppHandle handle)
{
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs != NULL && Lock(pKvs->xLock) == LOCK_OK)
    {
        if (pKvs->xStreamHandle != NULL)
        {
            prvStreamFlush(pKvs);
            Kvs_streamTermintate(pKvs->xStreamHandle);
            pKvs->xStreamHandle = NULL;
        }
        if (pKvs->pHost != NULL)
        {
            free(pKvs->pHost);
            pKvs->pHost = NULL;
        }
        if (pKvs->pRegion != NULL)
        {
            free(pKvs->pRegion);
            pKvs->pRegion = NULL;
        }
        if (pKvs->pService != NULL)
        {
            free(pKvs->pService);
            pKvs->pService = NULL;
        }
        if (pKvs->pStreamName != NULL)
        {
            free(pKvs->pStreamName);
            pKvs->pStreamName = NULL;
        }
        if (pKvs->pDataEndpoint != NULL)
        {
            KVS_FREE(pKvs->pDataEndpoint);
            pKvs->pDataEndpoint = NULL;
        }
        if (pKvs->pAwsAccessKeyId != NULL)
        {
            KVS_FREE(pKvs->pAwsAccessKeyId);
            pKvs->pAwsAccessKeyId = NULL;
        }
        if (pKvs->pAwsSecretAccessKey != NULL)
        {
            KVS_FREE(pKvs->pAwsSecretAccessKey);
            pKvs->pAwsSecretAccessKey = NULL;
        }
        if (pKvs->pIotCredentialHost != NULL)
        {
            KVS_FREE(pKvs->pIotCredentialHost);
            pKvs->pIotCredentialHost = NULL;
        }
        if (pKvs->pIotRoleAlias != NULL)
        {
            KVS_FREE(pKvs->pIotRoleAlias);
            pKvs->pIotRoleAlias = NULL;
        }
        if (pKvs->pIotThingName != NULL)
        {
            KVS_FREE(pKvs->pIotThingName);
            pKvs->pIotThingName = NULL;
        }
        if (pKvs->pIotX509RootCa != NULL)
        {
            KVS_FREE(pKvs->pIotX509RootCa);
            pKvs->pIotX509RootCa = NULL;
        }
        if (pKvs->pIotX509Certificate != NULL)
        {
            KVS_FREE(pKvs->pIotX509Certificate);
            pKvs->pIotX509Certificate = NULL;
        }
        if (pKvs->pIotX509PrivateKey != NULL)
        {
            KVS_FREE(pKvs->pIotX509PrivateKey);
            pKvs->pIotX509PrivateKey = NULL;
        }
        if (pKvs->pVideoTrackInfo != NULL)
        {
            prvVideoTrackInfoTerminate(pKvs->pVideoTrackInfo);
        }
        if (pKvs->pAudioTrackInfo != NULL)
        {
            prvAudioTrackInfoTerminate(pKvs->pAudioTrackInfo);
        }

        Unlock(pKvs->xLock);

        Lock_Deinit(pKvs->xLock);

        memset(pKvs, 0, sizeof(KvsApp_t));
        KVS_FREE(pKvs);
    }
}

int KvsApp_setoption(KvsAppHandle handle, const char *pcOptionName, const char *pValue)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL || pcOptionName == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (strcmp(pcOptionName, OPTION_AWS_ACCESS_KEY_ID) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pAwsAccessKeyId), pValue) != 0)
            {
                LogError("Failed to set pAwsAccessKeyId");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_AWS_SECRET_ACCESS_KEY) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pAwsSecretAccessKey), pValue) != 0)
            {
                LogError("Failed to set pAwsSecretAccessKey");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_CREDENTIAL_HOST) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotCredentialHost), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_ROLE_ALIAS) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotRoleAlias), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_THING_NAME) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotThingName), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_X509_ROOTCA) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509RootCa), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_X509_CERT) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509Certificate), pValue) != 0)
            {
                LogError("Failed to set pIotX509Certificate");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_IOT_X509_KEY) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509PrivateKey), pValue) != 0)
            {
                LogError("Failed to set pIotX509PrivateKey");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, OPTION_KVS_DATA_RETENTION_IN_HOURS) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value for KVS data retention in hours");
                res = ERRNO_FAIL;
            }
            else
            {
                pKvs->uDataRetentionInHours = *((unsigned int *)(pValue));
            }
        }
        else if (strcmp(pcOptionName, OPTION_KVS_VIDEO_TRACK_INFO) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to KVS video track info");
                res = ERRNO_FAIL;
            }
            else if ((pKvs->pVideoTrackInfo = prvCopyVideoTrackInfo((VideoTrackInfo_t *)pValue)) == NULL)
            {
                LogError("failed to copy video track info");
            }
        }
        else if (strcmp(pcOptionName, OPTION_KVS_AUDIO_TRACK_INFO) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to KVS audio track info");
                res = ERRNO_FAIL;
            }
            else if ((pKvs->pAudioTrackInfo = prvCopyAudioTrackInfo((AudioTrackInfo_t *)pValue)) == NULL)
            {
                LogError("failed to copy audio track info");
            }
        }
        else if (strcmp(pcOptionName, OPTION_STREAM_POLICY) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to stream strategy");
                res = ERRNO_FAIL;
            }
            else
            {
                KvsApp_streamPolicy_t xPolicy = *((KvsApp_streamPolicy_t *)pValue);
                if (xPolicy < STREAM_POLICY_NONE || xPolicy >= STREAM_POLICY_MAX)
                {
                    LogError("Invalid policy val");
                }
                else
                {
                    pKvs->xStrategy.xPolicy = xPolicy;
                    if (pKvs->xStrategy.xPolicy == STREAM_POLICY_RING_BUFFER)
                    {
                        pKvs->xStrategy.xRingBufferPara.uMemLimit = DEFAULT_RING_BUFFER_MEM_LIMIT;
                    }
                }
            }
        }
        else if (strcmp(pcOptionName, OPTION_STREAM_POLICY_RING_BUFFER_MEM_LIMIT) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to parameter of ring buffer policy");
                res = ERRNO_FAIL;
            }
            else if (pKvs->xStrategy.xPolicy != STREAM_POLICY_RING_BUFFER)
            {
                LogError("Cannot set parameter to policy: %d ", (int)(pKvs->xStrategy.xPolicy));
                res = ERRNO_FAIL;
            }
            else
            {
                size_t uMemLimit = *((size_t *)pKvs);
                pKvs->xStrategy.xRingBufferPara.uMemLimit = uMemLimit;
            }
        }
        else
        {
            /* TODO: Propagate this option to KVS stream. */
        }
    }

    return res;
}

int KvsApp_open(KvsAppHandle handle)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        updateIotCredential(pKvs);
        if (updateAndVerifyRestfulReqParameters(pKvs) != ERRNO_NONE)
        {
            LogError("Failed to setup KVS");
            res = ERRNO_FAIL;
        }
        else if (setupDataEndpoint(pKvs) != ERRNO_NONE)
        {
            LogError("Failed to setup data endpoint");
            res = ERRNO_FAIL;
        }
        else if (Kvs_putMediaStart(&(pKvs->xServicePara), &(pKvs->xPutMediaPara), &uHttpStatusCode, &(pKvs->xPutMediaHandle)) != 0 || uHttpStatusCode != 200)
        {
            LogError("Failed to setup PUT MEDIA");
            res = ERRNO_FAIL;
        }
        else
        {
            if (createStream(pKvs) != ERRNO_NONE)
            {
                LogError("Failed to setup KVS stream");
                res = ERRNO_FAIL;
            }
        }
    }

    return res;
}

int KvsApp_close(KvsAppHandle handle)
{
    int res = ERRNO_NONE;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pKvs->xPutMediaHandle != NULL)
        {
            if (Lock(pKvs->xLock) != LOCK_OK)
            {
                LogError("Failed to lock");
                res = ERRNO_FAIL;
            }
            else
            {
                Kvs_putMediaFinish(pKvs->xPutMediaHandle);
                pKvs->xPutMediaHandle = NULL;
                pKvs->isEbmlHeaderUpdated = false;
                Unlock(pKvs->xLock);
            }
        }
    }

    return res;
}

int KvsApp_addFrame(KvsAppHandle handle, uint8_t *pData, size_t uDataLen, size_t uDataSize, uint64_t uTimestamp, TrackType_t xTrackType)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    DataFrameIn_t xDataFrameIn = {0};

    if (pKvs == NULL || pData == NULL || uDataLen == 0)
    {
        res = ERRNO_FAIL;
    }
    else if (uTimestamp < pKvs->uEarliestTimestamp)
    {
        res = ERRNO_FAIL;
    }
    else if (xTrackType == TRACK_VIDEO && NALU_isAnnexBFrame(pData, uDataLen) && NALU_convertAnnexBToAvccInPlace(pData, uDataLen, uDataSize, (uint32_t *)&uDataLen) != ERRNO_NONE)
    {
        LogError("Failed to convert Annex-B to Avcc in place");
        res = ERRNO_FAIL;
    }
    else if (checkAndBuildStream(pKvs, pData, uDataLen, xTrackType) != ERRNO_NONE)
    {
        LogError("Failed to build stream buffer");
        res = ERRNO_FAIL;
    }
    else if (pKvs->xStreamHandle == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        xDataFrameIn.pData = (char *)pData;
        xDataFrameIn.uDataLen = uDataLen;
        xDataFrameIn.bIsKeyFrame = (xTrackType == TRACK_VIDEO) ? isKeyFrame(pData, uDataLen) : false;
        xDataFrameIn.uTimestampMs = uTimestamp;
        xDataFrameIn.xTrackType = xTrackType;
        xDataFrameIn.xClusterType = (xDataFrameIn.bIsKeyFrame) ? MKV_CLUSTER : MKV_SIMPLE_BLOCK;

        if (pKvs->xStrategy.xPolicy == STREAM_POLICY_RING_BUFFER)
        {
            prvStreamFlushHeadUntilMem(pKvs, pKvs->xStrategy.xRingBufferPara.uMemLimit);
        }

        if (Kvs_streamAddDataFrame(pKvs->xStreamHandle, &xDataFrameIn) == NULL)
        {
            LogError("Failed to add data frame");
            res = ERRNO_FAIL;
        }
    }

    if (res != ERRNO_NONE)
    {
        if (pData != NULL)
        {
            KVS_FREE(pData);
        }
    }

    return res;
}

int KvsApp_doWork(KvsAppHandle handle)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    int xSendCnt = 0;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        do
        {
            if (updateEbmlHeader(pKvs) != ERRNO_NONE)
            {
                res = ERRNO_FAIL;
                break;
            }

            if (Kvs_putMediaDoWork(pKvs->xPutMediaHandle) != ERRNO_NONE)
            {
                res = ERRNO_FAIL;
                break;
            }

            if (prvPutMediaSendData(pKvs, &xSendCnt) != ERRNO_NONE)
            {
                res = ERRNO_FAIL;
                break;
            }

            if (xSendCnt == 0)
            {
                prvSleepInMs(50);
            }
        } while (false);
    }

    return res;
}

size_t KvsApp_getStreamMemStatTotal(KvsAppHandle handle)
{
    size_t uMemTotal = 0;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs != NULL && pKvs->xStreamHandle != NULL && Kvs_streamMemStatTotal(pKvs->xStreamHandle, &uMemTotal) == 0)
    {
        return uMemTotal;
    }
    else
    {
        return 0;
    }
}

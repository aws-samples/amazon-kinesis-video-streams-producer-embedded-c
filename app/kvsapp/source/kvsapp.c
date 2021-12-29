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

/* Third-party headers */
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/xlogging.h"

/* KVS headers */
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

#define DEFAULT_CONNECTION_TIMEOUT_MS (10 * 1000)
#define DEFAULT_DATA_RETENTION_IN_HOURS (2)
#define DEFAULT_PUT_MEDIA_RECV_TIMEOUT_MS (1 * 1000)
#define DEFAULT_PUT_MEDIA_SEND_TIMEOUT_MS (1 * 1000)
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

typedef struct OnMkvSentCallbackInfo
{
    OnMkvSentCallback_t onMkvSentCallback;
    void *pAppData;
} OnMkvSentCallbackInfo_t;

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

    /* Session scope callbacks */
    OnMkvSentCallbackInfo_t onMkvSentCallbackInfo;
} KvsApp_t;

typedef struct DataFrameUserData
{
    DataFrameCallbacks_t xCallbacks;
} DataFrameUserData_t;

/**
 * Default implementation of OnDataFrameTerminateCallback_t. It calls free() to pData to release resource.
 *
 * @param[in] pData Pointer of data frame
 * @param[in] uDataLen Size of data frame
 * @param[in] uTimestamp Timestamp of data frame
 * @param[in] xTrackType Track type of data frame
 * @param[in] pAppData Pointer of application data that is assigned in function KvsApp_addFrameWithCallbacks()
 * @return 0 on success, non-zero value otherwise
 */
static int defaultOnDataFrameTerminate(uint8_t *pData, size_t uDataLen, uint64_t uTimestamp, TrackType_t xTrackType, void *pAppData)
{
    if (pData != NULL)
    {
        free(pData);
    }

    return ERRNO_NONE;
}

static void prvCallOnDataFrameTerminate(DataFrameIn_t *pDataFrameIn)
{
    DataFrameUserData_t *pUserData = NULL;
    OnDataFrameTerminateInfo_t *pOnDataFrameTerminateCallbackInfo = NULL;

    if (pDataFrameIn != NULL)
    {
        pUserData = (DataFrameUserData_t *)pDataFrameIn->pUserData;
        if (pUserData != NULL)
        {
            pOnDataFrameTerminateCallbackInfo = &(pUserData->xCallbacks.onDataFrameTerminateInfo);
            if (pOnDataFrameTerminateCallbackInfo->onDataFrameTerminate != NULL)
            {
                pOnDataFrameTerminateCallbackInfo->onDataFrameTerminate((uint8_t *)(pDataFrameIn->pData), pDataFrameIn->uDataLen, pDataFrameIn->uTimestampMs, pDataFrameIn->xTrackType, pOnDataFrameTerminateCallbackInfo->pAppData);
            }
        }
    }
}

static void prvVideoTrackInfoTerminate(VideoTrackInfo_t *pVideoTrackInfo)
{
    if (pVideoTrackInfo != NULL)
    {
        if (pVideoTrackInfo->pTrackName != NULL)
        {
            free(pVideoTrackInfo->pTrackName);
        }
        if (pVideoTrackInfo->pCodecName != NULL)
        {
            free(pVideoTrackInfo->pCodecName);
        }
        if (pVideoTrackInfo->pCodecPrivate != NULL)
        {
            free(pVideoTrackInfo->pCodecPrivate);
        }
        memset(pVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));
        free(pVideoTrackInfo);
    }
}

static void prvAudioTrackInfoTerminate(AudioTrackInfo_t *pAudioTrackInfo)
{
    if (pAudioTrackInfo != NULL)
    {
        if (pAudioTrackInfo->pTrackName != NULL)
        {
            free(pAudioTrackInfo->pTrackName);
        }
        if (pAudioTrackInfo->pCodecName != NULL)
        {
            free(pAudioTrackInfo->pCodecName);
        }
        if (pAudioTrackInfo->pCodecPrivate != NULL)
        {
            free(pAudioTrackInfo->pCodecPrivate);
        }
        memset(pAudioTrackInfo, 0, sizeof(VideoTrackInfo_t));
        free(pAudioTrackInfo);
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
    else if ((pDst = (uint8_t *)malloc(uSrcLen)) == NULL)
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
            free(pDst);
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
        prvCallOnDataFrameTerminate(pDataFrameIn);
        if (pDataFrameIn->pUserData != NULL)
        {
            free(pDataFrameIn->pUserData);
        }
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static int prvStreamFlushToNextCluster(KvsApp_t *pKvs)
{
    int res = ERRNO_NONE;
    StreamHandle xStreamHandle = pKvs->xStreamHandle;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while (1)
    {
        if (xStreamHandle == NULL)
        {
            res = ERRNO_FAIL;
            break;
        }
        else if ((xDataFrameHandle = Kvs_streamPeek(xStreamHandle)) == NULL)
        {
            res = ERRNO_FAIL;
            break;
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
                prvCallOnDataFrameTerminate(pDataFrameIn);
                if (pDataFrameIn->pUserData != NULL)
                {
                    free(pDataFrameIn->pUserData);
                }
                Kvs_dataFrameTerminate(xDataFrameHandle);
            }
        }
    }

    return res;
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
        prvCallOnDataFrameTerminate(pDataFrameIn);
        if (pDataFrameIn->pUserData != NULL)
        {
            free(pDataFrameIn->pUserData);
        }
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static VideoTrackInfo_t *prvCopyVideoTrackInfo(VideoTrackInfo_t *pSrcVideoTrackInfo)
{
    int res = ERRNO_NONE;
    VideoTrackInfo_t *pDstVideoTrackInfo = NULL;

    if ((pDstVideoTrackInfo = (VideoTrackInfo_t *)malloc(sizeof(VideoTrackInfo_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pDstVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));

        if (mallocAndStrcpy_s(&(pDstVideoTrackInfo->pTrackName), pSrcVideoTrackInfo->pTrackName) != 0 ||
            mallocAndStrcpy_s(&(pDstVideoTrackInfo->pCodecName), pSrcVideoTrackInfo->pCodecName) != 0 ||
            (pDstVideoTrackInfo->pCodecPrivate = (uint8_t *)malloc(pSrcVideoTrackInfo->uCodecPrivateLen)) == NULL)
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

    if ((pDstAudioTrackInfo = (AudioTrackInfo_t *)malloc(sizeof(AudioTrackInfo_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pDstAudioTrackInfo, 0, sizeof(AudioTrackInfo_t));

        if (mallocAndStrcpy_s(&(pDstAudioTrackInfo->pTrackName), pSrcAudioTrackInfo->pTrackName) != 0 ||
            mallocAndStrcpy_s(&(pDstAudioTrackInfo->pCodecName), pSrcAudioTrackInfo->pCodecName) != 0 ||
            (pDstAudioTrackInfo->pCodecPrivate = (uint8_t *)malloc(pSrcAudioTrackInfo->uCodecPrivateLen)) == NULL)
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
    pKvs->xServicePara.uRecvTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;
    pKvs->xServicePara.uSendTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;

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
        pKvs->xPutMediaPara.uRecvTimeoutMs = DEFAULT_PUT_MEDIA_RECV_TIMEOUT_MS;
        pKvs->xPutMediaPara.uSendTimeoutMs = DEFAULT_PUT_MEDIA_SEND_TIMEOUT_MS;
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
        if (prvStreamFlushToNextCluster(pKvs) != ERRNO_NONE)
        {
            LogInfo("No cluster frame is found");
        }
        else if (Kvs_streamGetMkvEbmlSegHdr(pKvs->xStreamHandle, &pEbmlSeg, &uEbmlSegLen) != 0 || Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, pEbmlSeg, uEbmlSegLen) != 0)
        {
            LogError("Failed to update ebml header");
            res = ERRNO_FAIL;
        }
        else
        {
            pKvs->isEbmlHeaderUpdated = true;

            if (pKvs->onMkvSentCallbackInfo.onMkvSentCallback != NULL)
            {
                /* FIXME: Handle the return value in a proper way. */
                pKvs->onMkvSentCallbackInfo.onMkvSentCallback(pEbmlSeg, uEbmlSegLen, pKvs->onMkvSentCallbackInfo.pAppData);
            }
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
                free(pCodecPrivateData);
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

static int prvCheckOnDataFrameToBeSent(DataFrameHandle xDataFrameHandle)
{
    int res = ERRNO_NONE;
    DataFrameIn_t *pDataFrameIn = NULL;
    DataFrameUserData_t *pUserData = NULL;
    OnDataFrameToBeSentInfo_t *pOnDataFrameToBeSentCallbackInfo = NULL;

    if (xDataFrameHandle == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        pUserData = pDataFrameIn->pUserData;
        pOnDataFrameToBeSentCallbackInfo = &(pUserData->xCallbacks.onDataFrameToBeSentInfo);
        if (pOnDataFrameToBeSentCallbackInfo->onDataFrameToBeSent != NULL)
        {
            res = pOnDataFrameToBeSentCallbackInfo->onDataFrameToBeSent((uint8_t *)(pDataFrameIn->pData), pDataFrameIn->uDataLen, pDataFrameIn->uTimestampMs, pDataFrameIn->xTrackType, pOnDataFrameToBeSentCallbackInfo->pAppData);
        }
    }

    return res;
}

static int prvPutMediaSendData(KvsApp_t *pKvs, int *pxSendCnt)
{
    int res = ERRNO_NONE;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint8_t *pMkvHeader = NULL;
    size_t uMkvHeaderLen = 0;
    int xSendCnt = 0;

    if (pKvs->xStreamHandle != NULL &&
        pKvs->isEbmlHeaderUpdated == true &&
        Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_VIDEO) &&
        (!pKvs->isAudioTrackPresent || Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_AUDIO)))
    {
        if ((xDataFrameHandle = Kvs_streamPop(pKvs->xStreamHandle)) == NULL)
        {
            LogError("Failed to get data frame");
            res = ERRNO_FAIL;
        }
        else if (prvCheckOnDataFrameToBeSent(xDataFrameHandle) != ERRNO_NONE)
        {
            LogInfo("Failed to check OnDataFrameToBeSent");
            /* We don't treat this condition as error because it's a validation error. */
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

            if (pKvs->onMkvSentCallbackInfo.onMkvSentCallback != NULL)
            {
                /* FIXME: Handle the return value in a proper way. */
                pKvs->onMkvSentCallbackInfo.onMkvSentCallback(pMkvHeader, uMkvHeaderLen, pKvs->onMkvSentCallbackInfo.pAppData);

                /* FIXME: Handle the return value in a proper way. */
                pKvs->onMkvSentCallbackInfo.onMkvSentCallback(pData, uDataLen, pKvs->onMkvSentCallbackInfo.pAppData);
            }
        }

        if (xDataFrameHandle != NULL)
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            prvCallOnDataFrameTerminate(pDataFrameIn);
            if (pDataFrameIn->pUserData != NULL)
            {
                free(pDataFrameIn->pUserData);
            }
            Kvs_dataFrameTerminate(xDataFrameHandle);
        }
    }

    if (pxSendCnt != NULL)
    {
        *pxSendCnt = xSendCnt;
    }

    return res;
}

KvsAppHandle KvsApp_create(const char *pcHost, const char *pcRegion, const char *pcService, const char *pcStreamName)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = NULL;

    if (pcHost == NULL || pcRegion == NULL || pcService == NULL || pcStreamName == NULL)
    {
        LogError("Invalid parameter");
        res = ERRNO_FAIL;
    }
    else if ((pKvs = (KvsApp_t *)malloc(sizeof(KvsApp_t))) == NULL)
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

            pKvs->uDataRetentionInHours = DEFAULT_DATA_RETENTION_IN_HOURS;

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
            free(pKvs->pDataEndpoint);
            pKvs->pDataEndpoint = NULL;
        }
        if (pKvs->pAwsAccessKeyId != NULL)
        {
            free(pKvs->pAwsAccessKeyId);
            pKvs->pAwsAccessKeyId = NULL;
        }
        if (pKvs->pAwsSecretAccessKey != NULL)
        {
            free(pKvs->pAwsSecretAccessKey);
            pKvs->pAwsSecretAccessKey = NULL;
        }
        if (pKvs->pIotCredentialHost != NULL)
        {
            free(pKvs->pIotCredentialHost);
            pKvs->pIotCredentialHost = NULL;
        }
        if (pKvs->pIotRoleAlias != NULL)
        {
            free(pKvs->pIotRoleAlias);
            pKvs->pIotRoleAlias = NULL;
        }
        if (pKvs->pIotThingName != NULL)
        {
            free(pKvs->pIotThingName);
            pKvs->pIotThingName = NULL;
        }
        if (pKvs->pIotX509RootCa != NULL)
        {
            free(pKvs->pIotX509RootCa);
            pKvs->pIotX509RootCa = NULL;
        }
        if (pKvs->pIotX509Certificate != NULL)
        {
            free(pKvs->pIotX509Certificate);
            pKvs->pIotX509Certificate = NULL;
        }
        if (pKvs->pIotX509PrivateKey != NULL)
        {
            free(pKvs->pIotX509PrivateKey);
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
        if (pKvs->pSps != NULL)
        {
            free(pKvs->pSps);
            pKvs->pSps = NULL;
        }
        if (pKvs->pPps != NULL)
        {
            free(pKvs->pPps);
            pKvs->pPps = NULL;
        }

        Unlock(pKvs->xLock);

        Lock_Deinit(pKvs->xLock);

        memset(pKvs, 0, sizeof(KvsApp_t));
        free(pKvs);
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
        if (strcmp(pcOptionName, (const char *)OPTION_AWS_ACCESS_KEY_ID) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pAwsAccessKeyId), pValue) != 0)
            {
                LogError("Failed to set pAwsAccessKeyId");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_AWS_SECRET_ACCESS_KEY) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pAwsSecretAccessKey), pValue) != 0)
            {
                LogError("Failed to set pAwsSecretAccessKey");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_CREDENTIAL_HOST) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotCredentialHost), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_ROLE_ALIAS) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotRoleAlias), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_THING_NAME) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotThingName), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_ROOTCA) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509RootCa), pValue) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_CERT) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509Certificate), pValue) != 0)
            {
                LogError("Failed to set pIotX509Certificate");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_KEY) == 0)
        {
            if (mallocAndStrcpy_s(&(pKvs->pIotX509PrivateKey), pValue) != 0)
            {
                LogError("Failed to set pIotX509PrivateKey");
                res = ERRNO_FAIL;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_KVS_DATA_RETENTION_IN_HOURS) == 0)
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
        else if (strcmp(pcOptionName, (const char *)OPTION_KVS_VIDEO_TRACK_INFO) == 0)
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
        else if (strcmp(pcOptionName, (const char *)OPTION_KVS_AUDIO_TRACK_INFO) == 0)
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
        else if (strcmp(pcOptionName, (const char *)OPTION_STREAM_POLICY) == 0)
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
                    LogError("Invalid policy val: %d", xPolicy);
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
        else if (strcmp(pcOptionName, (const char *)OPTION_STREAM_POLICY_RING_BUFFER_MEM_LIMIT) == 0)
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
                size_t uMemLimit = *((size_t *)pValue);
                pKvs->xStrategy.xRingBufferPara.uMemLimit = uMemLimit;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_NETIO_CONNECTION_TIMEOUT) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to connection timeout");
                res = ERRNO_FAIL;
            }
            else
            {
                unsigned int uConnectionTimeoutMs = *((unsigned int *)pValue);
                pKvs->xServicePara.uRecvTimeoutMs = uConnectionTimeoutMs;
                pKvs->xServicePara.uSendTimeoutMs = uConnectionTimeoutMs;
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_NETIO_STREAMING_RECV_TIMEOUT) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to streaming recv timeout");
                res = ERRNO_FAIL;
            }
            else
            {
                unsigned int uRecvTimeoutMs = *((unsigned int *)pValue);
                pKvs->xPutMediaPara.uRecvTimeoutMs = uRecvTimeoutMs;

                /* Try to update receive timeout if it's already streaming. */
                Kvs_putMediaUpdateRecvTimeout(pKvs->xPutMediaHandle, uRecvTimeoutMs);
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_NETIO_STREAMING_SEND_TIMEOUT) == 0)
        {
            if (pValue == NULL)
            {
                LogError("Invalid value set to streaming send timeout");
                res = ERRNO_FAIL;
            }
            else
            {
                unsigned int uSendTimeoutMs = *((unsigned int *)pValue);
                pKvs->xPutMediaPara.uSendTimeoutMs = uSendTimeoutMs;

                /* Try to update send timeout if it's already streaming. */
                Kvs_putMediaUpdateSendTimeout(pKvs->xPutMediaHandle, uSendTimeoutMs);
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
    return KvsApp_addFrameWithCallbacks(handle, pData, uDataLen, uDataSize, uTimestamp, xTrackType, NULL);
}

int KvsApp_addFrameWithCallbacks(KvsAppHandle handle, uint8_t *pData, size_t uDataLen, size_t uDataSize, uint64_t uTimestamp, TrackType_t xTrackType, DataFrameCallbacks_t *pCallbacks)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    DataFrameIn_t xDataFrameIn = {0};
    DataFrameUserData_t *pUserData = NULL;

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
    else if ((pUserData = (DataFrameUserData_t *)malloc(sizeof(DataFrameUserData_t))) == NULL)
    {
        LogError("OOM: pUserData");
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

        memset(pUserData, 0, sizeof(DataFrameUserData_t));
        if (pCallbacks == NULL)
        {
            /* Assign default callbacks. */
            pUserData->xCallbacks.onDataFrameTerminateInfo.onDataFrameTerminate = defaultOnDataFrameTerminate;
            pUserData->xCallbacks.onDataFrameTerminateInfo.pAppData = NULL;
            pUserData->xCallbacks.onDataFrameToBeSentInfo.onDataFrameToBeSent = NULL;
            pUserData->xCallbacks.onDataFrameToBeSentInfo.pAppData = NULL;
        }
        else
        {
            memcpy(&(pUserData->xCallbacks), pCallbacks, sizeof(DataFrameCallbacks_t));
        }
        xDataFrameIn.pUserData = pUserData;

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
            if (pCallbacks != NULL && pCallbacks->onDataFrameTerminateInfo.onDataFrameTerminate != NULL)
            {
                pCallbacks->onDataFrameTerminateInfo.onDataFrameTerminate(pData, uDataLen, uTimestamp, xTrackType, NULL);
            }
            else
            {
                defaultOnDataFrameTerminate(pData, uDataLen, uTimestamp, xTrackType, NULL);
            }
        }
        if (pUserData != NULL)
        {
            free(pUserData);
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
        } while (false);

        if (xSendCnt == 0)
        {
            sleepInMs(50);
        }
    }

    return res;
}

int KvsApp_readFragmentAck(KvsAppHandle handle, ePutMediaFragmentAckEventType *peAckEventType, uint64_t *puFragmentTimecode, unsigned int *puErrorId)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        res = Kvs_putMediaReadFragmentAck(pKvs->xPutMediaHandle, peAckEventType, puFragmentTimecode, puErrorId);
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

int KvsApp_setOnMkvSentCallback(KvsAppHandle handle, OnMkvSentCallback_t onMkvSentCallback, void *pAppData)
{
    int res = ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        pKvs->onMkvSentCallbackInfo.onMkvSentCallback = onMkvSentCallback;
        pKvs->onMkvSentCallbackInfo.pAppData = pAppData;
    }

    return res;
}

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
#include "kvs/errors.h"
#include "kvs/iot_credential_provider.h"
#include "kvs/nalu.h"
#include "kvs/port.h"
#include "kvs/restapi.h"
#include "kvs/stream.h"

#include "kvs/kvsapp.h"
#include "kvs/kvsapp_options.h"

/* Internal headers */
#include "os/allocator.h"

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

    /* AWS access key, access secret and session token */
    char *pAwsAccessKeyId;
    char *pAwsSecretAccessKey;
    char *pAwsSessionToken;

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
        /* NOTE: this frame is not allocated from KVS, so it should not be freed by kvsFree() */
        free(pData);
    }

    return KVS_ERRNO_NONE;
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
            kvsFree(pVideoTrackInfo->pTrackName);
        }
        if (pVideoTrackInfo->pCodecName != NULL)
        {
            kvsFree(pVideoTrackInfo->pCodecName);
        }
        if (pVideoTrackInfo->pCodecPrivate != NULL)
        {
            kvsFree(pVideoTrackInfo->pCodecPrivate);
        }
        memset(pVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));
        kvsFree(pVideoTrackInfo);
    }
}

static void prvAudioTrackInfoTerminate(AudioTrackInfo_t *pAudioTrackInfo)
{
    if (pAudioTrackInfo != NULL)
    {
        if (pAudioTrackInfo->pTrackName != NULL)
        {
            kvsFree(pAudioTrackInfo->pTrackName);
        }
        if (pAudioTrackInfo->pCodecName != NULL)
        {
            kvsFree(pAudioTrackInfo->pCodecName);
        }
        if (pAudioTrackInfo->pCodecPrivate != NULL)
        {
            kvsFree(pAudioTrackInfo->pCodecPrivate);
        }
        memset(pAudioTrackInfo, 0, sizeof(VideoTrackInfo_t));
        kvsFree(pAudioTrackInfo);
    }
}

static int prvMallocAndStrcpyHelper(char **destination, const char *source)
{
    int res = KVS_ERRNO_NONE;

    if ((destination == NULL) || (source == NULL))
    {
        /*If strDestination or strSource is a NULL pointer[...]these functions return EINVAL */
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (*destination != NULL)
        {
            kvsFree(*destination);
            *destination = NULL;
        }
        if (mallocAndStrcpy_s(destination, source) != 0)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
        }
    }

    return res;
}

static int prvBufMallocAndCopy(uint8_t **ppDst, size_t *puDstLen, uint8_t *pSrc, size_t uSrcLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pDst = NULL;
    size_t uDstLen = 0;

    if (ppDst == NULL || puDstLen == NULL || pSrc == NULL || uSrcLen == 0)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((pDst = (uint8_t *)kvsMalloc(uSrcLen)) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        memcpy(pDst, pSrc, uSrcLen);
        *ppDst = pDst;
        *puDstLen = uSrcLen;
    }

    if (res != KVS_ERRNO_NONE)
    {
        if (pDst != NULL)
        {
            kvsFree(pDst);
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
            kvsFree(pDataFrameIn->pUserData);
        }
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static int prvStreamFlushToNextCluster(KvsApp_t *pKvs)
{
    int res = KVS_ERRNO_NONE;
    StreamHandle xStreamHandle = pKvs->xStreamHandle;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while (1)
    {
        if (xStreamHandle == NULL)
        {
            res = KVS_ERROR_INVALID_ARGUMENT;
            break;
        }
        else if ((xDataFrameHandle = Kvs_streamPeek(xStreamHandle)) == NULL)
        {
            res = KVS_ERROR_STREAM_NO_AVAILABLE_DATA_FRAME;
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
                    kvsFree(pDataFrameIn->pUserData);
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
            kvsFree(pDataFrameIn->pUserData);
        }
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static VideoTrackInfo_t *prvCopyVideoTrackInfo(VideoTrackInfo_t *pSrcVideoTrackInfo)
{
    int res = KVS_ERRNO_NONE;
    VideoTrackInfo_t *pDstVideoTrackInfo = NULL;

    if ((pDstVideoTrackInfo = (VideoTrackInfo_t *)kvsMalloc(sizeof(VideoTrackInfo_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        memset(pDstVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));

        if ((res = prvMallocAndStrcpyHelper(&(pDstVideoTrackInfo->pTrackName), pSrcVideoTrackInfo->pTrackName)) != KVS_ERRNO_NONE ||
            (res = prvMallocAndStrcpyHelper(&(pDstVideoTrackInfo->pCodecName), pSrcVideoTrackInfo->pCodecName)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
        }
        else if ((pDstVideoTrackInfo->pCodecPrivate = (uint8_t *)kvsMalloc(pSrcVideoTrackInfo->uCodecPrivateLen)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
        }
        else
        {
            pDstVideoTrackInfo->uWidth = pSrcVideoTrackInfo->uWidth;
            pDstVideoTrackInfo->uHeight = pSrcVideoTrackInfo->uHeight;

            memcpy(pDstVideoTrackInfo->pCodecPrivate, pSrcVideoTrackInfo->pCodecPrivate, pSrcVideoTrackInfo->uCodecPrivateLen);
            pDstVideoTrackInfo->uCodecPrivateLen = pSrcVideoTrackInfo->uCodecPrivateLen;
        }
    }

    if (res != KVS_ERRNO_NONE)
    {
        prvVideoTrackInfoTerminate(pDstVideoTrackInfo);
        pDstVideoTrackInfo = NULL;
    }

    return pDstVideoTrackInfo;
}

static AudioTrackInfo_t *prvCopyAudioTrackInfo(AudioTrackInfo_t *pSrcAudioTrackInfo)
{
    int res = KVS_ERRNO_NONE;
    AudioTrackInfo_t *pDstAudioTrackInfo = NULL;

    if ((pDstAudioTrackInfo = (AudioTrackInfo_t *)kvsMalloc(sizeof(AudioTrackInfo_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        memset(pDstAudioTrackInfo, 0, sizeof(AudioTrackInfo_t));

        if ((res = prvMallocAndStrcpyHelper(&(pDstAudioTrackInfo->pTrackName), pSrcAudioTrackInfo->pTrackName) != KVS_ERRNO_NONE) ||
            (res = prvMallocAndStrcpyHelper(&(pDstAudioTrackInfo->pCodecName), pSrcAudioTrackInfo->pCodecName)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
        }
        else if ((pDstAudioTrackInfo->pCodecPrivate = (uint8_t *)kvsMalloc(pSrcAudioTrackInfo->uCodecPrivateLen)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
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

    if (res != KVS_ERRNO_NONE)
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
    int res = KVS_ERRNO_NONE;

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
            res = KVS_ERROR_NO_AWS_ACCESS_KEY_OR_SECRET_KEY;
            LogError("No available aws access key");
        }
        else
        {
            pKvs->xServicePara.pcAccessKey = pKvs->pAwsAccessKeyId;
            pKvs->xServicePara.pcSecretKey = pKvs->pAwsSecretAccessKey;
            pKvs->xServicePara.pcToken = pKvs->pAwsSessionToken;
        }
    }

    if (res == KVS_ERRNO_NONE)
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
    int res = KVS_ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
            /* Since we already have the endpoint, we needn't update it again. */
        }
        else
        {
            LogInfo("Try to describe stream");
            if ((res = Kvs_describeStream(&(pKvs->xServicePara), &(pKvs->xDescPara), &uHttpStatusCode)) != KVS_ERRNO_NONE)
            {
                LogError("Unable to describe stream");
                /* Propagate the res error */
            }
            else if (uHttpStatusCode != 200)
            {
                LogInfo("Failed to describe stream, status code:%u", uHttpStatusCode);
                res = KVS_GENERATE_RESTFUL_ERROR(uHttpStatusCode);

                LogInfo("Try to create stream");
                if ((res = Kvs_createStream(&(pKvs->xServicePara), &(pKvs->xCreatePara), &uHttpStatusCode)) != KVS_ERRNO_NONE)
                {
                    LogError("Unable to create stream");
                    /* Propagate the res error */
                }
                else if (uHttpStatusCode != 200)
                {
                    LogInfo("Failed to create stream, status code:%u", uHttpStatusCode);
                    res = KVS_GENERATE_RESTFUL_ERROR(uHttpStatusCode);
                }
            }

            if (res == KVS_ERRNO_NONE)
            {
                if ((res = Kvs_getDataEndpoint(&(pKvs->xServicePara), &(pKvs->xGetDataEpPara), &uHttpStatusCode, &(pKvs->pDataEndpoint))) != KVS_ERRNO_NONE)
                {
                    LogError("Unable to get data endpoint");
                    /* Propagate the res error */
                }
                else if (uHttpStatusCode != 200)
                {
                    LogInfo("Failed to get data endpoint, status code:%u", uHttpStatusCode);
                    res = KVS_GENERATE_RESTFUL_ERROR(uHttpStatusCode);
                }
                else
                {
                    pKvs->xServicePara.pcPutMediaEndpoint = pKvs->pDataEndpoint;
                }
            }
        }
    }

    if (res == KVS_ERRNO_NONE)
    {
        LogInfo("PUT MEDIA endpoint: %s", pKvs->xServicePara.pcPutMediaEndpoint);
    }

    return res;
}

static int updateEbmlHeader(KvsApp_t *pKvs)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pEbmlSeg = NULL;
    size_t uEbmlSegLen = 0;

    if (pKvs->xPutMediaHandle != NULL && !(pKvs->isEbmlHeaderUpdated))
    {
        LogInfo("Flush to next cluster");
        if ((res = prvStreamFlushToNextCluster(pKvs)) != KVS_ERRNO_NONE)
        {
            LogInfo("No cluster frame is found");
            /* Propagate the res error */
        }
        else if ((res = Kvs_streamGetMkvEbmlSegHdr(pKvs->xStreamHandle, &pEbmlSeg, &uEbmlSegLen)) != KVS_ERRNO_NONE ||
                 (res = Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, pEbmlSeg, uEbmlSegLen)) != KVS_ERRNO_NONE)
        {
            LogError("Failed to update EBML header");
            /* Propagate the res error */
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
    int res = KVS_ERRNO_NONE;
    VideoTrackInfo_t xVideoTrackInfo = {0};
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateDataLen = 0;

    if (pKvs->xStreamHandle == NULL)
    {
        if (pKvs->pVideoTrackInfo == NULL && pKvs->pSps != NULL && pKvs->pPps != NULL)
        {
            /* We don't have video track info, but we have SPS & PPS to generate video track info from it. */
            if ((res = NALU_getH264VideoResolutionFromSps(pKvs->pSps, pKvs->uSpsLen, &(xVideoTrackInfo.uWidth), &(xVideoTrackInfo.uHeight))) != KVS_ERRNO_NONE ||
                (res = Mkv_generateH264CodecPrivateDataFromSpsPps(pKvs->pSps, pKvs->uSpsLen, pKvs->pPps, pKvs->uPpsLen, &pCodecPrivateData, &uCodecPrivateDataLen)) != KVS_ERRNO_NONE)
            {
                LogError("Failed to generate video track info");
                /* Propagate the res error */
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
                kvsFree(pCodecPrivateData);
            }
        }

        if (pKvs->pVideoTrackInfo != NULL)
        {
            if ((pKvs->xStreamHandle = Kvs_streamCreate(pKvs->pVideoTrackInfo, pKvs->pAudioTrackInfo)) == NULL)
            {
                res = KVS_ERROR_FAIL_TO_CREATE_STREAM_HANDLE;
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
    int res = KVS_ERRNO_NONE;
    uint8_t *pSps = NULL;
    size_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    size_t uPpsLen = 0;

    if (pKvs->xStreamHandle == NULL)
    {
        /* Try to build video track info from frames. */
        if (pKvs->pVideoTrackInfo == NULL && xTrackType == TRACK_VIDEO)
        {
            if (pKvs->pSps == NULL && NALU_getNaluFromAvccNalus(pData, uDataLen, NALU_TYPE_SPS, &pSps, &uSpsLen) == KVS_ERRNO_NONE)
            {
                LogInfo("SPS is found");
                if ((res = prvBufMallocAndCopy(&(pKvs->pSps), &(pKvs->uSpsLen), pSps, uSpsLen)) != KVS_ERRNO_NONE)
                {
                    /* Propagate the res error */
                }
                else
                {
                    LogInfo("SPS is set");
                }
            }
            if (pKvs->pPps == NULL && NALU_getNaluFromAvccNalus(pData, uDataLen, NALU_TYPE_PPS, &pPps, &uPpsLen) == KVS_ERRNO_NONE)
            {
                LogInfo("PPS is found");
                if ((res = prvBufMallocAndCopy(&(pKvs->pPps), &(pKvs->uPpsLen), pPps, uPpsLen)) != KVS_ERRNO_NONE)
                {
                    /* Propagate the res error */
                }
                else
                {
                    LogInfo("PPS is set");
                }
            }
        }

        if (pKvs->pSps != NULL && pKvs->pPps != NULL)
        {
            res = createStream(pKvs);
        }
    }

    return res;
}

static int prvCheckOnDataFrameToBeSent(DataFrameHandle xDataFrameHandle)
{
    int res = KVS_ERRNO_NONE;
    int retval = 0;
    DataFrameIn_t *pDataFrameIn = NULL;
    DataFrameUserData_t *pUserData = NULL;
    OnDataFrameToBeSentInfo_t *pOnDataFrameToBeSentCallbackInfo = NULL;

    if (xDataFrameHandle == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        pUserData = pDataFrameIn->pUserData;
        pOnDataFrameToBeSentCallbackInfo = &(pUserData->xCallbacks.onDataFrameToBeSentInfo);
        if (pOnDataFrameToBeSentCallbackInfo->onDataFrameToBeSent != NULL)
        {
            retval = pOnDataFrameToBeSentCallbackInfo->onDataFrameToBeSent((uint8_t *)(pDataFrameIn->pData), pDataFrameIn->uDataLen, pDataFrameIn->uTimestampMs, pDataFrameIn->xTrackType, pOnDataFrameToBeSentCallbackInfo->pAppData);
            if (retval != 0)
            {
                res = KVS_GENERATE_CALLBACK_ERROR(retval);
            }
        }
    }

    return res;
}

static int prvPutMediaSendData(KvsApp_t *pKvs, int *pxSendCnt, bool bForceSend)
{
    int res = KVS_ERRNO_NONE;
    int retVal = 0;
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
        (!bForceSend || !pKvs->isAudioTrackPresent || Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_AUDIO)))
    {
        if ((xDataFrameHandle = Kvs_streamPop(pKvs->xStreamHandle)) == NULL)
        {
            res = KVS_ERROR_STREAM_NO_AVAILABLE_DATA_FRAME;
            LogError("Failed to get data frame");
        }
        else if ((res = prvCheckOnDataFrameToBeSent(xDataFrameHandle)) != KVS_ERRNO_NONE)
        {
            LogInfo("Failed to check OnDataFrameToBeSent");
            /* Propagate the res error */
        }
        else if ((res = Kvs_dataFrameGetContent(xDataFrameHandle, &pMkvHeader, &uMkvHeaderLen, &pData, &uDataLen)) != KVS_ERRNO_NONE)
        {
            LogError("Failed to get data and mkv header to send");
            /* Propagate the res error */
        }
        else if ((res = Kvs_putMediaUpdate(pKvs->xPutMediaHandle, pMkvHeader, uMkvHeaderLen, pData, uDataLen)) != KVS_ERRNO_NONE)
        {
            LogError("Failed to update");
            /* Propagate the res error */
        }
        else
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            pKvs->uEarliestTimestamp = pDataFrameIn->uTimestampMs;

            xSendCnt++;

            if (pKvs->onMkvSentCallbackInfo.onMkvSentCallback != NULL)
            {
                /* FIXME: Handle the return value in a proper way. */
                if ((retVal = pKvs->onMkvSentCallbackInfo.onMkvSentCallback(pMkvHeader, uMkvHeaderLen, pKvs->onMkvSentCallbackInfo.pAppData)) != 0)
                {
                    res = KVS_GENERATE_CALLBACK_ERROR(retVal);
                }
                else if ((retVal = pKvs->onMkvSentCallbackInfo.onMkvSentCallback(pData, uDataLen, pKvs->onMkvSentCallbackInfo.pAppData)) != 0)
                {
                    res = KVS_GENERATE_CALLBACK_ERROR(retVal);
                }
                else
                {
                    /* nop */
                }
            }
        }

        if (xDataFrameHandle != NULL)
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            prvCallOnDataFrameTerminate(pDataFrameIn);
            if (pDataFrameIn->pUserData != NULL)
            {
                kvsFree(pDataFrameIn->pUserData);
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

static int prvPutMediaDoWorkDefault(KvsApp_t *pKvs)
{
    int res = KVS_ERRNO_NONE;
    int xSendCnt = 0;

    do
    {
        if ((res = updateEbmlHeader(pKvs)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }

        if ((res = Kvs_putMediaDoWork(pKvs->xPutMediaHandle)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }

        if ((res = prvPutMediaSendData(pKvs, &xSendCnt, false)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }
    } while (false);

    if (xSendCnt == 0)
    {
        sleepInMs(50);
    }

    return res;
}

static int prvPutMediaDoWorkSendEndOfFrames(KvsApp_t *pKvs)
{
    int res = KVS_ERRNO_NONE;
    int xSendCnt = 0;

    do
    {
        if ((res = updateEbmlHeader(pKvs)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }

        if ((res = Kvs_putMediaDoWork(pKvs->xPutMediaHandle)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }

        if ((res = prvPutMediaSendData(pKvs, &xSendCnt, true)) != KVS_ERRNO_NONE)
        {
            /* Propagate the res error */
            break;
        }
    } while (xSendCnt > 0);

    return res;
}

KvsAppHandle KvsApp_create(const char *pcHost, const char *pcRegion, const char *pcService, const char *pcStreamName)
{
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = NULL;

    if (pcHost == NULL || pcRegion == NULL || pcService == NULL || pcStreamName == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid parameter");
    }
    else if ((pKvs = (KvsApp_t *)kvsMalloc(sizeof(KvsApp_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
        LogError("OOM: pKvs");
    }
    else
    {
        memset(pKvs, 0, sizeof(KvsApp_t));

        if ((pKvs->xLock = Lock_Init()) == NULL)
        {
            res = KVS_ERROR_LOCK_ERROR;
            LogError("Failed to init lock");
        }
        else if (
            (res = prvMallocAndStrcpyHelper(&(pKvs->pHost), pcHost)) != KVS_ERRNO_NONE ||
            (res = prvMallocAndStrcpyHelper(&(pKvs->pRegion), pcRegion)) != KVS_ERRNO_NONE ||
            (res = prvMallocAndStrcpyHelper(&(pKvs->pService), pcService)) != KVS_ERRNO_NONE ||
            (res = prvMallocAndStrcpyHelper(&(pKvs->pStreamName), pcStreamName)) != KVS_ERRNO_NONE)
        {
            LogError("OOM: parameters");
            /* Propagate the res error */
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

    if (res != KVS_ERRNO_NONE)
    {
        KvsApp_terminate(pKvs);
        pKvs = NULL;
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
            kvsFree(pKvs->pHost);
            pKvs->pHost = NULL;
        }
        if (pKvs->pRegion != NULL)
        {
            kvsFree(pKvs->pRegion);
            pKvs->pRegion = NULL;
        }
        if (pKvs->pService != NULL)
        {
            kvsFree(pKvs->pService);
            pKvs->pService = NULL;
        }
        if (pKvs->pStreamName != NULL)
        {
            kvsFree(pKvs->pStreamName);
            pKvs->pStreamName = NULL;
        }
        if (pKvs->pDataEndpoint != NULL)
        {
            kvsFree(pKvs->pDataEndpoint);
            pKvs->pDataEndpoint = NULL;
        }
        if (pKvs->pAwsAccessKeyId != NULL)
        {
            kvsFree(pKvs->pAwsAccessKeyId);
            pKvs->pAwsAccessKeyId = NULL;
        }
        if (pKvs->pAwsSecretAccessKey != NULL)
        {
            kvsFree(pKvs->pAwsSecretAccessKey);
            pKvs->pAwsSecretAccessKey = NULL;
        }
        if (pKvs->pAwsSessionToken != NULL)
        {
            kvsFree(pKvs->pAwsSessionToken);
            pKvs->pAwsSessionToken = NULL;
        }
        if (pKvs->pIotCredentialHost != NULL)
        {
            kvsFree(pKvs->pIotCredentialHost);
            pKvs->pIotCredentialHost = NULL;
        }
        if (pKvs->pIotRoleAlias != NULL)
        {
            kvsFree(pKvs->pIotRoleAlias);
            pKvs->pIotRoleAlias = NULL;
        }
        if (pKvs->pIotThingName != NULL)
        {
            kvsFree(pKvs->pIotThingName);
            pKvs->pIotThingName = NULL;
        }
        if (pKvs->pIotX509RootCa != NULL)
        {
            kvsFree(pKvs->pIotX509RootCa);
            pKvs->pIotX509RootCa = NULL;
        }
        if (pKvs->pIotX509Certificate != NULL)
        {
            kvsFree(pKvs->pIotX509Certificate);
            pKvs->pIotX509Certificate = NULL;
        }
        if (pKvs->pIotX509PrivateKey != NULL)
        {
            kvsFree(pKvs->pIotX509PrivateKey);
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
            kvsFree(pKvs->pSps);
            pKvs->pSps = NULL;
        }
        if (pKvs->pPps != NULL)
        {
            kvsFree(pKvs->pPps);
            pKvs->pPps = NULL;
        }

        Unlock(pKvs->xLock);

        Lock_Deinit(pKvs->xLock);

        memset(pKvs, 0, sizeof(KvsApp_t));
        kvsFree(pKvs);
    }
}

int KvsApp_setoption(KvsAppHandle handle, const char *pcOptionName, const char *pValue)
{
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL || pcOptionName == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (strcmp(pcOptionName, (const char *)OPTION_AWS_ACCESS_KEY_ID) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pAwsAccessKeyId), pValue)) != 0)
            {
                LogError("Failed to set pAwsAccessKeyId");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_AWS_SECRET_ACCESS_KEY) == 0)
        {
            if (prvMallocAndStrcpyHelper(&(pKvs->pAwsSecretAccessKey), pValue) != 0)
            {
                LogError("Failed to set pAwsSecretAccessKey");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_AWS_SESSION_TOKEN) == 0)
        {
            if (pValue == NULL)
            {
                if (pKvs->pAwsSessionToken != NULL)
                {
                    kvsFree(pKvs->pAwsSessionToken);
                }
                pKvs->pAwsSessionToken = NULL;
            }
            else if ((res = prvMallocAndStrcpyHelper(&(pKvs->pAwsSessionToken), pValue)) != 0)
            {
                LogError("Failed to set pAwsSessionToken");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_CREDENTIAL_HOST) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotCredentialHost), pValue)) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_ROLE_ALIAS) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotRoleAlias), pValue)) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_THING_NAME) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotThingName), pValue)) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_ROOTCA) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotX509RootCa), pValue)) != 0)
            {
                LogError("Failed to set pIotX509RootCa");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_CERT) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotX509Certificate), pValue)) != 0)
            {
                LogError("Failed to set pIotX509Certificate");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_IOT_X509_KEY) == 0)
        {
            if ((res = prvMallocAndStrcpyHelper(&(pKvs->pIotX509PrivateKey), pValue)) != 0)
            {
                LogError("Failed to set pIotX509PrivateKey");
                /* Propagate the res error */
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_KVS_DATA_RETENTION_IN_HOURS) == 0)
        {
            if (pValue == NULL)
            {
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value for KVS data retention in hours");
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
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to KVS video track info");
            }
            else if ((pKvs->pVideoTrackInfo = prvCopyVideoTrackInfo((VideoTrackInfo_t *)pValue)) == NULL)
            {
                res = KVS_ERROR_OUT_OF_MEMORY;
                LogError("failed to copy video track info");
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_KVS_AUDIO_TRACK_INFO) == 0)
        {
            if (pValue == NULL)
            {
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to KVS audio track info");
            }
            else if ((pKvs->pAudioTrackInfo = prvCopyAudioTrackInfo((AudioTrackInfo_t *)pValue)) == NULL)
            {
                res = KVS_ERROR_OUT_OF_MEMORY;
                LogError("failed to copy audio track info");
            }
        }
        else if (strcmp(pcOptionName, (const char *)OPTION_STREAM_POLICY) == 0)
        {
            if (pValue == NULL)
            {
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to stream strategy");
            }
            else
            {
                KvsApp_streamPolicy_t xPolicy = *((KvsApp_streamPolicy_t *)pValue);
                if (xPolicy < STREAM_POLICY_NONE || xPolicy >= STREAM_POLICY_MAX)
                {
                    LogError("Invalid policy val: %d", xPolicy);
                    res = KVS_ERROR_INVALID_STREAM_POLICY;
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
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to parameter of ring buffer policy");
            }
            else if (pKvs->xStrategy.xPolicy != STREAM_POLICY_RING_BUFFER)
            {
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Cannot set parameter to policy: %d ", (int)(pKvs->xStrategy.xPolicy));
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
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to connection timeout");
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
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to streaming recv timeout");
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
                res = KVS_ERROR_INVALID_ARGUMENT;
                LogError("Invalid value set to streaming send timeout");
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
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        updateIotCredential(pKvs);
        if ((res = updateAndVerifyRestfulReqParameters(pKvs)) != KVS_ERRNO_NONE)
        {
            LogError("Failed to setup KVS");
            /* Propagate the res error */
        }
        else if ((res = setupDataEndpoint(pKvs)) != KVS_ERRNO_NONE)
        {
            LogError("Failed to setup data endpoint");
            /* Propagate the res error */
        }
        else if ((res = Kvs_putMediaStart(&(pKvs->xServicePara), &(pKvs->xPutMediaPara), &uHttpStatusCode, &(pKvs->xPutMediaHandle))) != KVS_ERRNO_NONE)
        {
            LogError("Failed to setup PUT MEDIA");
            /* Propagate the res error */
        }
        else if (uHttpStatusCode != 200)
        {
            res = KVS_GENERATE_RESTFUL_ERROR(uHttpStatusCode);
            LogError("PUT MEDIA http status code:%d\n", uHttpStatusCode);
        }
        else
        {
            if ((res = createStream(pKvs)) != KVS_ERRNO_NONE)
            {
                LogError("Failed to setup KVS stream");
                /* Propagate the res error */
            }
        }
    }

    return res;
}

int KvsApp_close(KvsAppHandle handle)
{
    int res = KVS_ERRNO_NONE;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (pKvs->xPutMediaHandle != NULL)
        {
            if (Lock(pKvs->xLock) != LOCK_OK)
            {
                res = KVS_ERROR_LOCK_ERROR;
                LogError("Failed to lock");
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
    int res = KVS_ERRNO_NONE;
    int retVal = 0;
    KvsApp_t *pKvs = (KvsApp_t *)handle;
    DataFrameIn_t xDataFrameIn = {0};
    DataFrameUserData_t *pUserData = NULL;

    if (pKvs == NULL || pData == NULL || uDataLen == 0)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if (uTimestamp < pKvs->uEarliestTimestamp)
    {
        res = KVS_ERROR_ADD_FRAME_WHOSE_TIMESTAMP_GOES_BACK;
    }
    else if (
        xTrackType == TRACK_VIDEO && NALU_isAnnexBFrame(pData, uDataLen) &&
        (res = NALU_convertAnnexBToAvccInPlace(pData, uDataLen, uDataSize, (uint32_t *)&uDataLen)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to convert Annex-B to Avcc in place");
        /* Propagate the res error */
    }
    else if ((res = checkAndBuildStream(pKvs, pData, uDataLen, xTrackType)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to build stream buffer");
        /* Propagate the res error */
    }
    else if (pKvs->xStreamHandle == NULL)
    {
        res = KVS_ERROR_STREAM_NOT_READY;
    }
    else if ((pUserData = (DataFrameUserData_t *)kvsMalloc(sizeof(DataFrameUserData_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
        LogError("OOM: pUserData");
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
            res = KVS_ERROR_FAIL_TO_ADD_DATA_FRAME_TO_STREAM;
            LogError("Failed to add data frame");
        }
    }

    if (res != KVS_ERRNO_NONE)
    {
        if (pData != NULL)
        {
            if (pCallbacks != NULL && pCallbacks->onDataFrameTerminateInfo.onDataFrameTerminate != NULL)
            {
                retVal = pCallbacks->onDataFrameTerminateInfo.onDataFrameTerminate(pData, uDataLen, uTimestamp, xTrackType, NULL);
            }
            else
            {
                retVal = defaultOnDataFrameTerminate(pData, uDataLen, uTimestamp, xTrackType, NULL);
            }
            if (retVal != 0)
            {
                res = KVS_GENERATE_CALLBACK_ERROR(retVal);
            }
        }
        if (pUserData != NULL)
        {
            kvsFree(pUserData);
        }
    }

    return res;
}

int KvsApp_doWork(KvsAppHandle handle)
{
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        res = prvPutMediaDoWorkDefault(pKvs);
    }

    return res;
}

int KvsApp_doWorkEx(KvsAppHandle handle, DoWorkExParamter_t *pPara)
{
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (pPara == NULL || pPara->eType == DO_WORK_DEFAULT)
        {
            res = prvPutMediaDoWorkDefault(pKvs);
        }
        else if (pPara->eType == DO_WORK_SEND_END_OF_FRAMES)
        {
            res = prvPutMediaDoWorkSendEndOfFrames(pKvs);
        }
        else
        {
            res = KVS_ERROR_KVSAPP_UNKNOWN_DO_WORK_TYPE;
        }
    }

    return res;
}

int KvsApp_readFragmentAck(KvsAppHandle handle, ePutMediaFragmentAckEventType *peAckEventType, uint64_t *puFragmentTimecode, unsigned int *puErrorId)
{
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
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

    if (pKvs != NULL && pKvs->xStreamHandle != NULL && Kvs_streamMemStatTotal(pKvs->xStreamHandle, &uMemTotal) == KVS_ERRNO_NONE)
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
    int res = KVS_ERRNO_NONE;
    KvsApp_t *pKvs = (KvsApp_t *)handle;

    if (pKvs == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        pKvs->onMkvSentCallbackInfo.onMkvSentCallback = onMkvSentCallback;
        pKvs->onMkvSentCallbackInfo.pAppData = pAppData;
    }

    return res;
}

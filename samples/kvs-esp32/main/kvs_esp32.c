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
#include <pthread.h>

#include "kvs_esp32.h"

#include "kvs/port.h"
#include "kvs/nalu.h"
#include "kvs/restapi.h"
#include "kvs/stream.h"

#include "sample_config.h"
#include "h264_file_loader.h"

#if ENABLE_AUDIO_TRACK
#include "aac_file_loader.h"
#endif /* ENABLE_AUDIO_TRACK */

#if ENABLE_IOT_CREDENTIAL
#include "kvs/iot_credential_provider.h"
#endif /* ENABLE_IOT_CREDENTIAL */

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

typedef struct Kvs
{
#if ENABLE_IOT_CREDENTIAL
    IotCredentialRequest_t xIotCredentialReq;
#endif

    KvsServiceParameter_t xServicePara;
    KvsDescribeStreamParameter_t xDescPara;
    KvsCreateStreamParameter_t xCreatePara;
    KvsGetDataEndpointParameter_t xGetDataEpPara;
    KvsPutMediaParameter_t xPutMediaPara;

    StreamHandle xStreamHandle;
    PutMediaHandle xPutMediaHandle;

    VideoTrackInfo_t *pVideoTrackInfo;
    AudioTrackInfo_t *pAudioTrackInfo;

    pthread_t videoTid;
    H264FileLoaderHandle xVideoFileLoader;

#if ENABLE_AUDIO_TRACK
    pthread_t audioTid;
    AacFileLoaderHandle xAudioFileLoader;
#endif /* ENABLE_AUDIO_TRACK */

#if DEBUG_STORE_MEDIA_TO_FILE
    FILE *fp;
#endif
} Kvs_t;

static void sleepInMs( uint32_t ms )
{
    usleep( ms * 1000 );
}

static int kvsInitialize(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    char *pcStreamName = KVS_STREAM_NAME;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pKvs, 0, sizeof(Kvs_t));

        pKvs->xServicePara.pcHost = AWS_KVS_HOST;
        pKvs->xServicePara.pcRegion = AWS_KVS_REGION;
        pKvs->xServicePara.pcService = AWS_KVS_SERVICE;
        pKvs->xServicePara.pcAccessKey = AWS_ACCESS_KEY;
        pKvs->xServicePara.pcSecretKey = AWS_SECRET_KEY;

        pKvs->xDescPara.pcStreamName = pcStreamName;

        pKvs->xCreatePara.pcStreamName = pcStreamName;
        pKvs->xCreatePara.uDataRetentionInHours = 2;

        pKvs->xGetDataEpPara.pcStreamName = pcStreamName;

        pKvs->xPutMediaPara.pcStreamName = pcStreamName;
        pKvs->xPutMediaPara.xTimecodeType = TIMECODE_TYPE_ABSOLUTE;

#if ENABLE_IOT_CREDENTIAL
        pKvs->xIotCredentialReq.pCredentialHost = CREDENTIALS_HOST;
        pKvs->xIotCredentialReq.pRoleAlias = ROLE_ALIAS;
        pKvs->xIotCredentialReq.pThingName = THING_NAME;
        pKvs->xIotCredentialReq.pRootCA = ROOT_CA;
        pKvs->xIotCredentialReq.pCertificate = CERTIFICATE;
        pKvs->xIotCredentialReq.pPrivateKey = PRIVATE_KEY;
#endif

#if DEBUG_STORE_MEDIA_TO_FILE
        pKvs->fp = fopen(MEDIA_FILENAME, "wb");
        if (pKvs->fp == NULL)
        {
            printf("Failed to open media file\r\n");
        }
#endif /* DEBUG_STORE_MEDIA_TO_FILE */
    }

    return res;
}

static void kvsTerminate(Kvs_t *pKvs)
{
    if (pKvs != NULL)
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
            free(pKvs->xServicePara.pcPutMediaEndpoint);
            pKvs->xServicePara.pcPutMediaEndpoint = NULL;
        }
    }
}

static int setupDataEndpoint(Kvs_t *pKvs)
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
            printf("Try to describe stream\r\n");
            if (Kvs_describeStream(&(pKvs->xServicePara), &(pKvs->xDescPara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
            {
                printf("Failed to describe stream\r\n");
                printf("Try to create stream\r\n");
                if (Kvs_createStream(&(pKvs->xServicePara), &(pKvs->xCreatePara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to create stream\r\n");
                    res = ERRNO_FAIL;
                }
            }

            if (res == ERRNO_NONE)
            {
                if (Kvs_getDataEndpoint(&(pKvs->xServicePara), &(pKvs->xGetDataEpPara), &uHttpStatusCode, &(pKvs->xServicePara.pcPutMediaEndpoint)) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to get data endpoint\r\n");
                    res = ERRNO_FAIL;
                }
            }
        }
    }

    if (res == ERRNO_NONE)
    {
        printf("PUT MEDIA endpoint: %s\r\n", pKvs->xServicePara.pcPutMediaEndpoint);
    }

    return res;
}

static void streamFlush(StreamHandle xStreamHandle)
{
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while ((xDataFrameHandle = Kvs_streamPop(xStreamHandle)) != NULL)
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        free(pDataFrameIn->pData);
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static void streamFlushToNextCluster(StreamHandle xStreamHandle)
{
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while (1)
    {
        xDataFrameHandle = Kvs_streamPeek(xStreamHandle);
        if (xDataFrameHandle == NULL)
        {
            sleepInMs(50);
        }
        else
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            if (pDataFrameIn->xClusterType == MKV_CLUSTER)
            {
                break;
            }
            else
            {
                xDataFrameHandle = Kvs_streamPop(xStreamHandle);
                pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
                free(pDataFrameIn->pData);
                Kvs_dataFrameTerminate(xDataFrameHandle);
            }
        }
    }
}

static int putMediaSendData(Kvs_t *pKvs)
{
    int res = 0;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint8_t *pMkvHeader = NULL;
    size_t uMkvHeaderLen = 0;

    if (Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_VIDEO)
#if ENABLE_AUDIO_TRACK
           && Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_AUDIO)
#endif
    )
    {
        if ((xDataFrameHandle = Kvs_streamPop(pKvs->xStreamHandle)) == NULL)
        {
            printf("Failed to get data frame\r\n");
            res = ERRNO_FAIL;
        }
        else if (Kvs_dataFrameGetContent(xDataFrameHandle, &pMkvHeader, &uMkvHeaderLen, &pData, &uDataLen) != 0)
        {
            printf("Failed to get data and mkv header to send\r\n");
            res = ERRNO_FAIL;
        }
        else if (Kvs_putMediaUpdate(pKvs->xPutMediaHandle, pMkvHeader, uMkvHeaderLen, pData, uDataLen) != 0)
        {
            printf("Failed to update\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
#if DEBUG_STORE_MEDIA_TO_FILE
            if (pKvs->fp != NULL && (fwrite(pMkvHeader, 1, uMkvHeaderLen, pKvs->fp) == 0 || fwrite(pData, 1, uDataLen, pKvs->fp) == 0))
            {
                printf("Failed to write media to file\r\n");
                fclose(pKvs->fp);
                pKvs->fp = NULL;
            }
#endif
        }

        if (xDataFrameHandle != NULL)
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            free(pDataFrameIn->pData);
            Kvs_dataFrameTerminate(xDataFrameHandle);
        }
    }

    return res;
}

static int putMedia(Kvs_t *pKvs)
{
    int res = 0;
    unsigned int uHttpStatusCode = 0;
    uint8_t *pEbmlSeg = NULL;
    size_t uEbmlSegLen = 0;

    printf("Try to put media\r\n");
    if (pKvs == NULL)
    {
        printf("Invalid argument: pKvs\r\n");
        res = ERRNO_FAIL;
    }
    else if (Kvs_putMediaStart(&(pKvs->xServicePara), &(pKvs->xPutMediaPara), &uHttpStatusCode, &(pKvs->xPutMediaHandle)) != 0 || uHttpStatusCode != 200)
    {
        printf("Failed to setup PUT MEDIA\r\n");
        res = ERRNO_FAIL;
    }
    else if (Kvs_streamGetMkvEbmlSegHdr(pKvs->xStreamHandle, &pEbmlSeg, &uEbmlSegLen) != 0 ||
             Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, pEbmlSeg, uEbmlSegLen) != 0)
    {
        printf("Failed to upadte MKV EBML and segment\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        /* The beginning of a KVS stream has to be a cluster frame. */
        streamFlushToNextCluster(pKvs->xStreamHandle);

#if DEBUG_STORE_MEDIA_TO_FILE
        if (pKvs->fp != NULL && fwrite(pEbmlSeg, 1, uEbmlSegLen, pKvs->fp) == 0)
        {
            printf("Failed to write media to file\r\n");
            fclose(pKvs->fp);
            pKvs->fp = NULL;
        }
#endif
        while (1)
        {
            if (putMediaSendData(pKvs) != ERRNO_NONE)
            {
                break;
            }
            if (Kvs_putMediaDoWork(pKvs->xPutMediaHandle) != ERRNO_NONE)
            {
                break;
            }
            if (Kvs_streamIsEmpty(pKvs->xStreamHandle))
            {
                sleepInMs(50);
            }
        }
    }

    printf("Leaving put media\r\n");
    Kvs_putMediaFinish(pKvs->xPutMediaHandle);
    pKvs->xPutMediaHandle = NULL;

    return res;
}

static void *videoThread(void *arg)
{
    int res = 0;
    Kvs_t *pKvs = (Kvs_t *)arg;

    DataFrameIn_t xDataFrameIn = {0};
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint64_t uTimestamp = getEpochTimestampInMs();
    const uint32_t uFps = VIDEO_FPS;
    size_t uMemTotal = 0;

    if (pKvs == NULL)
    {
        printf("%s(): Invalid argument: pKvs\r\n", __FUNCTION__);
    }
    else
    {
        /* Wait a little to avoid buffer full */
        sleepInMs(5000);

        while (1)
        {
            uMemTotal = 0;
            if (Kvs_streamMemStatTotal(pKvs->xStreamHandle, &uMemTotal) == 0)
            {
                if (uMemTotal > STREAM_MAX_BUFFERING_SIZE)
                {
                    sleepInMs(1000);
                    continue;
                }
            }
            else
            {
                printf("failed to get current memory stat\r\n");
                res = ERRNO_FAIL;
                break;
            }

            if (H264FileLoaderLoadFrame(pKvs->xVideoFileLoader, (char **)&pData, &uDataLen) != 0)
            {
                printf("Failed to load data frame\r\n");
                res = ERRNO_FAIL;
                break;
            }
            else
            {
                xDataFrameIn.pData = (char *)pData;
                xDataFrameIn.uDataLen = uDataLen;
                xDataFrameIn.bIsKeyFrame = isKeyFrame(pData, uDataLen);
                xDataFrameIn.uTimestampMs = uTimestamp;
                xDataFrameIn.xTrackType = TRACK_VIDEO;

                xDataFrameIn.xClusterType = (xDataFrameIn.bIsKeyFrame) ? MKV_CLUSTER : MKV_SIMPLE_BLOCK;

                if (Kvs_streamAddDataFrame(pKvs->xStreamHandle, &xDataFrameIn) == NULL)
                {
                    printf("Failed to add data frame\r\n");
                    res = ERRNO_FAIL;
                    break;
                }
                else
                {
                    
                }
            }

            sleepInMs(1000/uFps);
            uTimestamp += 1000/uFps;
        }
    }

    printf("video thread leaving, err:%d\r\n", res);

    return NULL;
}

#if ENABLE_AUDIO_TRACK
static void *audioThread(void *arg)
{
    int res = 0;
    Kvs_t *pKvs = (Kvs_t *)arg;

    DataFrameIn_t xDataFrameIn = {0};
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint64_t uTimestamp = getEpochTimestampInMs();
    uint32_t uFps = AUDIO_FPS;

    if (pKvs == NULL)
    {
        printf("%s(): Invalid argument: pKvs\r\n", __FUNCTION__);
    }
    else
    {
        while (1)
        {
            if (AacFileLoaderLoadFrame(pKvs->xAudioFileLoader, (char **)&pData, &uDataLen) != 0)
            {
                printf("Failed to load data frame\r\n");
                res = ERRNO_FAIL;
            }
            else
            {
                xDataFrameIn.pData = (char *)pData;
                xDataFrameIn.uDataLen = uDataLen;
                xDataFrameIn.bIsKeyFrame = false;
                xDataFrameIn.uTimestampMs = uTimestamp;
                xDataFrameIn.xTrackType = TRACK_AUDIO;

                xDataFrameIn.xClusterType = MKV_SIMPLE_BLOCK;

                if (Kvs_streamAddDataFrame(pKvs->xStreamHandle, &xDataFrameIn) == NULL)
                {
                    printf("Failed to add data frame\r\n");
                    res = ERRNO_FAIL;
                    break;
                }
                else
                {
                    
                }
            }

            sleepInMs(1000/uFps);
            uTimestamp += 1000/uFps;
        }
    }

    return NULL;
}
#endif /* ENABLE_AUDIO_TRACK */

void Kvs_run(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;
    FileLoaderPara_t xVideoFileLoaderParam = {0};
    FileLoaderPara_t xAudioFileLoaderParam = {0};

    xVideoFileLoaderParam.pcTrackName = VIDEO_TRACK_NAME;
    xVideoFileLoaderParam.pcFileFormat = H264_FILE_FORMAT;
    xVideoFileLoaderParam.xFileStartIdx = H264_FILE_IDX_BEGIN;
    xVideoFileLoaderParam.xFileEndIdx = H264_FILE_IDX_END;
    xVideoFileLoaderParam.bKeepRotate = true;

#if ENABLE_AUDIO_TRACK
    xAudioFileLoaderParam.pcTrackName = AUDIO_TRACK_NAME;
    xAudioFileLoaderParam.pcFileFormat = AAC_FILE_FORMAT;
    xAudioFileLoaderParam.xFileStartIdx = AAC_FILE_IDX_BEGIN;
    xAudioFileLoaderParam.xFileEndIdx = AAC_FILE_IDX_END;
    xAudioFileLoaderParam.bKeepRotate = true;
#endif /* ENABLE_AUDIO_TRACK */

#if ENABLE_IOT_CREDENTIAL
    IotCredentialToken_t *pToken = NULL;
#endif /* ENABLE_IOT_CREDENTIAL */

    if (kvsInitialize(pKvs) != 0)
    {
        printf("Failed to initialize KVS\r\n");
    }
    else if ((pKvs->xVideoFileLoader = H264FileLoaderCreate(&xVideoFileLoaderParam)) == NULL)
    {
        printf("Failed to initialize H264 file loader\r\n");
    }
#if ENABLE_AUDIO_TRACK
    else if ((pKvs->xAudioFileLoader = AacFileLoaderCreate(&xAudioFileLoaderParam, AUDIO_MPEG_OBJECT_TYPE, AUDIO_FREQUENCY, AUDIO_CHANNEL_NUMBER)) == NULL)
    {
        printf("Failed to initialize AAC file loader\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else if ((pKvs->pVideoTrackInfo = H264FileLoaderGetVideoTrackInfo(pKvs->xVideoFileLoader)) == NULL ||
#if ENABLE_AUDIO_TRACK
             (pKvs->pAudioTrackInfo = AacFileLoaderGetAudioTrackInfo(pKvs->xAudioFileLoader)) == NULL ||
#endif /* ENABLE_AUDIO_TRACK */
             (pKvs->xStreamHandle = Kvs_streamCreate(pKvs->pVideoTrackInfo, pKvs->pAudioTrackInfo)) == NULL)
    {
        printf("Failed to create stream\r\n");
    }
    else if (pthread_create(&(pKvs->videoTid), NULL, videoThread, pKvs) != 0)
    {
        printf("Failed to create video thread\r\n");
    }
#if ENABLE_AUDIO_TRACK
    else if (pthread_create(&(pKvs->audioTid), NULL, audioThread, pKvs) != 0)
    {
        printf("Failed to create audio thread\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else
    {
        while (1)
        {
#if ENABLE_IOT_CREDENTIAL
            Iot_credentialTerminate(pToken);
            if ((pToken = Iot_getCredential(&(pKvs->xIotCredentialReq))) == NULL)
            {
                printf("Failed to get Iot credential\r\n");
                break;
            }
            else
            {
                pKvs->xServicePara.pcAccessKey = pToken->pAccessKeyId;
                pKvs->xServicePara.pcSecretKey = pToken->pSecretAccessKey;
                pKvs->xServicePara.pcToken = pToken->pSessionToken;
            }
#endif

            if (setupDataEndpoint(pKvs) != ERRNO_NONE)
            {
                printf("Failed to get PUT MEDIA endpoint");
            }
            else if (putMedia(pKvs) != ERRNO_NONE)
            {
                printf("End of PUT MEDIA\r\n");
                break;
            }

            sleepInMs(100); /* Wait for retry */
        }
    }

#if ENABLE_IOT_CREDENTIAL
    Iot_credentialTerminate(pToken);
#endif /* ENABLE_IOT_CREDENTIAL */
    H264FileLoaderTerminate(pKvs->xVideoFileLoader);
#if ENABLE_AUDIO_TRACK
    AacFileLoaderTerminate(pKvs->xAudioFileLoader);
#endif /* ENABLE_AUDIO_TRACK */

    streamFlush(pKvs->xStreamHandle);
    Kvs_streamTermintate(pKvs->xStreamHandle);
}

KvsHandle Kvs_create(void)
{
    Kvs_t *pKvs = NULL;

    if ((pKvs = (Kvs_t *)malloc(sizeof(Kvs_t))) != NULL)
    {
        memset(pKvs, 0, sizeof(Kvs_t));
    }

    return pKvs;
}

void Kvs_terminate(KvsHandle xKvsHandle)
{
    Kvs_t *pKvs = xKvsHandle;

    if (pKvs != NULL)
    {
        free(pKvs);
    }
}
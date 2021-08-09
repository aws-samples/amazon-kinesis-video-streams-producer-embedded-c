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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvs/kvsapp.h"
#include "kvs/port.h"

#include "h264_file_loader.h"
#include "sample_config.h"

#if ENABLE_AUDIO_TRACK
#    include "aac_file_loader.h"
#endif /* ENABLE_AUDIO_TRACK */

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

static pthread_t videoTid;
static H264FileLoaderHandle xVideoFileLoader = NULL;

#if ENABLE_AUDIO_TRACK
static pthread_t audioTid;
static AacFileLoaderHandle xAudioFileLoader = NULL;
#endif /* ENABLE_AUDIO_TRACK */

static void sleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static void *videoThread(void *arg)
{
    int res = 0;
    KvsAppHandle kvsAppHandle = (KvsAppHandle)arg;

    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint64_t uTimestamp = getEpochTimestampInMs();
    const uint32_t uFps = VIDEO_FPS;

    if (kvsAppHandle == NULL)
    {
        printf("%s(): Invalid argument: pKvs\r\n", __FUNCTION__);
    }
    else
    {
        while (1)
        {
            if (H264FileLoaderLoadFrame(xVideoFileLoader, (char **)&pData, &uDataLen) != 0)
            {
                printf("Failed to load data frame\r\n");
                res = ERRNO_FAIL;
                break;
            }
            else
            {
                KvsApp_addFrame(kvsAppHandle, pData, uDataLen, uDataLen, uTimestamp, TRACK_VIDEO);
            }

            sleepInMs(1000 / uFps);
            uTimestamp += 1000 / uFps;
        }
    }

    printf("video thread leaving, err:%d\r\n", res);

    return NULL;
}

#if ENABLE_AUDIO_TRACK
static void *audioThread(void *arg)
{
    KvsAppHandle kvsAppHandle = (KvsAppHandle)arg;

    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint64_t uTimestamp = getEpochTimestampInMs();
    uint32_t uFps = AUDIO_FPS;

    if (kvsAppHandle == NULL)
    {
        printf("%s(): Invalid argument: pKvs\r\n", __FUNCTION__);
    }
    else
    {
        while (1)
        {
            if (AacFileLoaderLoadFrame(xAudioFileLoader, (char **)&pData, &uDataLen) != 0)
            {
                printf("Failed to load data frame\r\n");
                break;
            }
            else
            {
                KvsApp_addFrame(kvsAppHandle, pData, uDataLen, uDataLen, uTimestamp, TRACK_AUDIO);
            }

            sleepInMs(1000 / uFps);
            uTimestamp += 1000 / uFps;
        }
    }

    return NULL;
}
#endif /* ENABLE_AUDIO_TRACK */

static int setKvsAppOptions(KvsAppHandle kvsAppHandle)
{
    int res = ERRNO_NONE;

    /* Setup credentials, it should be either using IoT credentials or AWS access key. */
#if ENABLE_IOT_CREDENTIAL
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_CREDENTIAL_HOST, (const char *)CREDENTIALS_HOST) != 0)
    {
        printf("Failed to set credential host\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_ROLE_ALIAS, (const char *)ROLE_ALIAS) != 0)
    {
        printf("Failed to set role alias\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_THING_NAME, (const char *)THING_NAME) != 0)
    {
        printf("Failed to set thing name\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_ROOTCA, (const char *)ROOT_CA) != 0)
    {
        printf("Failed to set root CA\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_CERT, (const char *)CERTIFICATE) != 0)
    {
        printf("Failed to set certificate\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_KEY, (const char *)PRIVATE_KEY) != 0)
    {
        printf("Failed to set private key\r\n");
    }
#else
    if (KvsApp_setoption(kvsAppHandle, OPTION_AWS_ACCESS_KEY_ID, (const char *)AWS_ACCESS_KEY) != 0)
    {
        printf("Failed to set AWS_ACCESS_KEY\r\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_AWS_SECRET_ACCESS_KEY, (const char *)AWS_SECRET_KEY) != 0)
    {
        printf("Failed to set AWS_SECRET_KEY\r\n");
    }
#endif /* ENABLE_IOT_CREDENTIAL */

    if (KvsApp_setoption(kvsAppHandle, OPTION_KVS_VIDEO_TRACK_INFO, (const char *)H264FileLoaderGetVideoTrackInfo(xVideoFileLoader)) != 0)
    {
        printf("Failed to set video track info\r\n");
    }

#if ENABLE_AUDIO_TRACK
    if (KvsApp_setoption(kvsAppHandle, OPTION_KVS_AUDIO_TRACK_INFO, (const char *)AacFileLoaderGetAudioTrackInfo(xAudioFileLoader)) != 0)
    {
        printf("Failed to set video track info\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */

    return res;
}

int main(int argc, char *argv[])
{
    KvsAppHandle kvsAppHandle;
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

    if ((kvsAppHandle = KvsApp_create(AWS_KVS_HOST, AWS_KVS_REGION, AWS_KVS_SERVICE, KVS_STREAM_NAME)) == NULL)
    {
        printf("Failed to initialize KVS\r\n");
    }
    else if ((xVideoFileLoader = H264FileLoaderCreate(&xVideoFileLoaderParam)) == NULL)
    {
        printf("Failed to initialize H264 file loader\r\n");
    }
#if ENABLE_AUDIO_TRACK
    else if ((xAudioFileLoader = AacFileLoaderCreate(&xAudioFileLoaderParam, AUDIO_MPEG_OBJECT_TYPE, AUDIO_FREQUENCY, AUDIO_CHANNEL_NUMBER)) == NULL)
    {
        printf("Failed to initialize AAC file loader\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else if (setKvsAppOptions(kvsAppHandle) != ERRNO_NONE)
    {
        printf("Failed to set options\r\n");
    }
    else if (pthread_create(&videoTid, NULL, videoThread, kvsAppHandle) != 0)
    {
        printf("Failed to create video thread\r\n");
    }
#if ENABLE_AUDIO_TRACK
    else if (pthread_create(&audioTid, NULL, audioThread, kvsAppHandle) != 0)
    {
        printf("Failed to create audio thread\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else
    {
        while (1)
        {
            if (KvsApp_open(kvsAppHandle) != 0)
            {
                printf("Failed to open KVS app\r\n");
                break;
            }

            while (1)
            {
                if (KvsApp_doWork(kvsAppHandle) != 0)
                {
                    break;
                }
            }

            if (KvsApp_close(kvsAppHandle) != 0)
            {
                printf("Failed to close KVS app\r\n");
                break;
            }
        }
    }

    KvsApp_close(kvsAppHandle);

    H264FileLoaderTerminate(xVideoFileLoader);
    xVideoFileLoader = NULL;

#if ENABLE_AUDIO_TRACK
    AacFileLoaderTerminate(xAudioFileLoader);
    xAudioFileLoader = NULL;
#endif /* ENABLE_AUDIO_TRACK */

    pthread_join(videoTid, NULL);
#if ENABLE_AUDIO_TRACK
    pthread_join(audioTid, NULL);
#endif /* ENABLE_AUDIO_TRACK */

    KvsApp_terminate(kvsAppHandle);

    return 0;
}
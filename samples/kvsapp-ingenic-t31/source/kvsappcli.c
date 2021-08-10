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

#include "sample_config.h"
#include "t31_video.h"
#if ENABLE_AUDIO_TRACK
#    include "t31_audio.h"
#endif /* ENABLE_AUDIO_TRACK */

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

static T31VideoHandle videoHandle = NULL;

#if ENABLE_AUDIO_TRACK
static T31AudioHandle audioHandle = NULL;
#endif /* ENABLE_AUDIO_TRACK */

static void sleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

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

#if ENABLE_AUDIO_TRACK
    if (KvsApp_setoption(kvsAppHandle, OPTION_KVS_AUDIO_TRACK_INFO, (const char *)T31Audio_getAudioTrackInfoClone(audioHandle)) != 0)
    {
        printf("Failed to set video track info\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */

    KvsApp_streamPolicy_t xPolicy = STREAM_POLICY_RING_BUFFER;
    if (KvsApp_setoption(kvsAppHandle, OPTION_STREAM_POLICY, (const char *)&xPolicy) != 0)
    {
        printf("Failed to set stream policy\r\n");
    }
    size_t uRingBufferMemLimit = RING_BUFFER_MEM_LIMIT;
    if (KvsApp_setoption(kvsAppHandle, OPTION_STREAM_POLICY_RING_BUFFER_MEM_LIMIT, (const char *)&uRingBufferMemLimit) != 0)
    {
        printf("Failed to set ring buffer memory limit\r\n");
    }

    return res;
}

int main(int argc, char *argv[])
{
    KvsAppHandle kvsAppHandle;
    uint64_t uLastPrintMemStatTimestamp = 0;

    if ((kvsAppHandle = KvsApp_create(AWS_KVS_HOST, AWS_KVS_REGION, AWS_KVS_SERVICE, KVS_STREAM_NAME)) == NULL)
    {
        printf("Failed to initialize KVS\r\n");
    }
    else if ((videoHandle = T31Video_create(kvsAppHandle)) == NULL)
    {
        printf("Failed to initialize T31 video\r\n");
    }
#if ENABLE_AUDIO_TRACK
    else if ((audioHandle = T31Audio_create(kvsAppHandle)) == NULL)
    {
        printf("Failed to initialize t31 audio\r\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else if (setKvsAppOptions(kvsAppHandle) != ERRNO_NONE)
    {
        printf("Failed to set options\r\n");
    }
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

                if (getEpochTimestampInMs() > uLastPrintMemStatTimestamp + 1000)
                {
                    printf("Buffer memory used: %zu\r\n", KvsApp_getStreamMemStatTotal(kvsAppHandle));
                    uLastPrintMemStatTimestamp = getEpochTimestampInMs();
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

    T31Video_terminate(videoHandle);
#if ENABLE_AUDIO_TRACK
    T31Audio_terminate(audioHandle);
#endif

    KvsApp_terminate(kvsAppHandle);

    return 0;
}
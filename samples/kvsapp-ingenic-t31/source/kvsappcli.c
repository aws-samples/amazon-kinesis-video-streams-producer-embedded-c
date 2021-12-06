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
#include "option_configuration.h"
#include "t31_video.h"
#if ENABLE_AUDIO_TRACK
#include "t31_audio.h"
#endif /* ENABLE_AUDIO_TRACK */

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#ifdef KVS_USE_POOL_ALLOCATOR
#include "kvs/pool_allocator.h"
static char pMemPool[POOL_ALLOCATOR_SIZE];
#endif

static T31VideoHandle videoHandle = NULL;

#if ENABLE_AUDIO_TRACK
static T31AudioHandle audioHandle = NULL;
#endif /* ENABLE_AUDIO_TRACK */

#if DEBUG_STORE_MEDIA_TO_FILE
static FILE *fpDbgMedia = NULL;
static int onMkvSent(uint8_t *pData, size_t uDataLen, void *pAppData)
{
    int res = ERRNO_NONE;
    char pFilename[ sizeof(MEDIA_FILENAME_FORMAT) + 21 ]; /* 20 digits for uint64_t, 1 digit for EOS */

    if (fpDbgMedia == NULL)
    {
        snprintf(pFilename, sizeof(pFilename)-1, MEDIA_FILENAME_FORMAT, getEpochTimestampInMs());
        fpDbgMedia = fopen(pFilename, "wb");
        if (fpDbgMedia == NULL)
        {
            printf("Failed to open debug file: %s\n", pFilename);
        }
        else
        {
            printf("Opened debug file %s\n", pFilename);
        }
    }

    if (fpDbgMedia != NULL)
    {
        fwrite(pData, 1, uDataLen, fpDbgMedia);
    }

    return res;
}
#endif

static int setKvsAppOptions(KvsAppHandle kvsAppHandle)
{
    int res = ERRNO_NONE;

    /* Setup credentials, it should be either using IoT credentials or AWS access key. */
#if ENABLE_IOT_CREDENTIAL
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_CREDENTIAL_HOST, (const char *)CREDENTIALS_HOST) != 0)
    {
        printf("Failed to set credential host\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_ROLE_ALIAS, (const char *)ROLE_ALIAS) != 0)
    {
        printf("Failed to set role alias\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_THING_NAME, (const char *)THING_NAME) != 0)
    {
        printf("Failed to set thing name\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_ROOTCA, (const char *)ROOT_CA) != 0)
    {
        printf("Failed to set root CA\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_CERT, (const char *)CERTIFICATE) != 0)
    {
        printf("Failed to set certificate\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_IOT_X509_KEY, (const char *)PRIVATE_KEY) != 0)
    {
        printf("Failed to set private key\n");
    }
#else
    if (KvsApp_setoption(kvsAppHandle, OPTION_AWS_ACCESS_KEY_ID, OptCfg_getAwsAccessKey()) != 0)
    {
        printf("Failed to set AWS_ACCESS_KEY\n");
    }
    if (KvsApp_setoption(kvsAppHandle, OPTION_AWS_SECRET_ACCESS_KEY, OptCfg_getAwsSecretAccessKey()) != 0)
    {
        printf("Failed to set AWS_SECRET_KEY\n");
    }
#endif /* ENABLE_IOT_CREDENTIAL */

#if ENABLE_AUDIO_TRACK
    if (KvsApp_setoption(kvsAppHandle, OPTION_KVS_AUDIO_TRACK_INFO, (const char *)T31Audio_getAudioTrackInfoClone(audioHandle)) != 0)
    {
        printf("Failed to set audio track info\n");
    }
#endif /* ENABLE_AUDIO_TRACK */

#if ENABLE_RING_BUFFER_MEM_LIMIT
    KvsApp_streamPolicy_t xPolicy = STREAM_POLICY_RING_BUFFER;
    if (KvsApp_setoption(kvsAppHandle, OPTION_STREAM_POLICY, (const char *)&xPolicy) != 0)
    {
        printf("Failed to set stream policy\n");
    }
    size_t uRingBufferMemLimit = RING_BUFFER_MEM_LIMIT;
    if (KvsApp_setoption(kvsAppHandle, OPTION_STREAM_POLICY_RING_BUFFER_MEM_LIMIT, (const char *)&uRingBufferMemLimit) != 0)
    {
        printf("Failed to set ring buffer memory limit\n");
    }
#endif /* ENABLE_RING_BUFFER_MEM_LIMIT */

#if DEBUG_STORE_MEDIA_TO_FILE
    if (KvsApp_setOnMkvSentCallback(kvsAppHandle, onMkvSent, NULL) != 0)
    {
        printf("Failed to set onMkvSentCallback\n");
    }
#endif /* DEBUG_STORE_MEDIA_TO_FILE */

    return res;
}

int main(int argc, char *argv[])
{
    KvsAppHandle kvsAppHandle;
    uint64_t uLastPrintMemStatTimestamp = 0;
    const char *pKvsStreamName = NULL;

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorInit((void *)pMemPool, sizeof(pMemPool));
#endif

    /* Resolve KVS stream name */
    pKvsStreamName = (argc >= 2) ? argv[1] : KVS_STREAM_NAME;

    if ((kvsAppHandle = KvsApp_create(OptCfg_getHostKinesisVideo(), OptCfg_getRegion(), OptCfg_getServiceKinesisVideo(), pKvsStreamName)) == NULL)
    {
        printf("Failed to initialize KVS\n");
    }
    else if ((videoHandle = T31Video_create(kvsAppHandle)) == NULL)
    {
        printf("Failed to initialize T31 video\n");
    }
#if ENABLE_AUDIO_TRACK
    else if ((audioHandle = T31Audio_create(kvsAppHandle)) == NULL)
    {
        printf("Failed to initialize t31 audio\n");
    }
#endif /* ENABLE_AUDIO_TRACK */
    else if (setKvsAppOptions(kvsAppHandle) != ERRNO_NONE)
    {
        printf("Failed to set options\n");
    }
    else
    {
        while (1)
        {
            /* FIXME: Check if network is reachable before running KVS. */

            if (KvsApp_open(kvsAppHandle) != 0)
            {
                printf("Failed to open KVS app\n");
                break;
            }
            else
            {
                printf("KvsApp opened\n");
            }

            while (1)
            {
                if (KvsApp_doWork(kvsAppHandle) != 0)
                {
                    break;
                }

                if (getEpochTimestampInMs() > uLastPrintMemStatTimestamp + 1000)
                {
                    printf("Buffer memory used: %zu\n", KvsApp_getStreamMemStatTotal(kvsAppHandle));
                    uLastPrintMemStatTimestamp = getEpochTimestampInMs();

#ifdef KVS_USE_POOL_ALLOCATOR
                    PoolStats_t stats = {0};
                    poolAllocatorGetStats(&stats);
                    printf("Sum of used/free memory:%zu/%zu, size of largest used/free block:%zu/%zu, number of used/free blocks:%zu/%zu\n",
                        stats.uSumOfUsedMemory, stats.uSumOfFreeMemory,
                        stats.uSizeOfLargestUsedBlock, stats.uSizeOfLargestFreeBlock,
                        stats.uNumberOfUsedBlocks, stats.uNumberOfFreeBlocks
                    );
#endif
                }
            }

            if (KvsApp_close(kvsAppHandle) != 0)
            {
                printf("Failed to close KVS app\n");
                break;
            }
            else
            {
                printf("KvsApp closed\n");
#if DEBUG_STORE_MEDIA_TO_FILE
                if (fpDbgMedia != NULL)
                {
                    fclose(fpDbgMedia);
                    fpDbgMedia = NULL;
                    printf("Closed debug file\n");
                }
#endif
            }
        }
    }

    KvsApp_close(kvsAppHandle);

    T31Video_terminate(videoHandle);
#if ENABLE_AUDIO_TRACK
    T31Audio_terminate(audioHandle);
#endif

    KvsApp_terminate(kvsAppHandle);

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorDeinit();
#endif

    return 0;
}

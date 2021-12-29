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
#include <signal.h>

/* Headers for KVS */
#include "kvs/pool_allocator.h"
#include "kvs/kvsapp.h"
#include "kvs/port.h"

#include "frame_ring_buffer/frame_ring_buffer.h"

#include "sample_config.h"
#include "option_configuration.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

/* A global variable to exit program if it's set to true. */
extern bool gStopRunning;
static KvsAppHandle gKvsAppHandle = NULL;

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

    return res;
}

int producer_main(int argc, char *argv[])
{
    KvsAppHandle kvsAppHandle = NULL;
    ePutMediaFragmentAckEventType eAckEventType = eUnknown;
    uint64_t uFragmentTimecode = 0;
    unsigned int uErrorId = 0;
    const char *pKvsStreamName = NULL;

    /* Resolve KVS stream name */
    pKvsStreamName = (argc >= 2) ? argv[1] : KVS_STREAM_NAME;

    if ((kvsAppHandle = KvsApp_create(OptCfg_getHostKinesisVideo(), OptCfg_getRegion(), OptCfg_getServiceKinesisVideo(), pKvsStreamName)) == NULL)
    {
        printf("Failed to initialize KVS\n");
    }
    else if (setKvsAppOptions(kvsAppHandle) != ERRNO_NONE)
    {
        printf("Failed to set options\n");
    }
    else
    {
        gKvsAppHandle = kvsAppHandle;

        while (!gStopRunning)
        {
            if (KvsApp_open(kvsAppHandle) != 0)
            {
                printf("Failed to open KVS app\n");
                break;
            }

            while (!gStopRunning)
            {
                if (KvsApp_doWork(kvsAppHandle) != 0)
                {
                    break;
                }

                while (KvsApp_readFragmentAck(kvsAppHandle, &eAckEventType, &uFragmentTimecode, &uErrorId) == 0)
                {
                    if (eAckEventType == ePersisted)
                    {
                        // printf("key-frame with timecode %" PRIu64 " is persisted\n", uFragmentTimecode);
                    }
                }
            }

            while (KvsApp_readFragmentAck(kvsAppHandle, &eAckEventType, &uFragmentTimecode, &uErrorId) == 0)
            {
                if (eAckEventType == eError)
                {
                    /* Please refer to this link to get more information of the error ID:
                     *      https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_dataplane_PutMedia.html
                     */
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
            }
        }
    }

    KvsApp_close(kvsAppHandle);

    gKvsAppHandle = NULL;

    KvsApp_terminate(kvsAppHandle);

    return 0;
}

int producer_onDataFrameTerminateCallback(uint8_t *pData, size_t uDataLen, uint64_t uTimestamp, TrackType_t xTrackType, void *pAppData)
{
    /* Do nothing, this frame will be freed by frame ring buffer. */
    return 0;
}

static int producer_onDataFrameToBeSent(uint8_t *pData, size_t uDataLen, uint64_t uTimestamp, TrackType_t xTrackType, void *pAppData)
{
    int res = 0;
    FrameKeyHandle keyHandle = (FrameKeyHandle)pAppData;
    uint8_t *pRefData = NULL;
    size_t uRefDataLen = 0;

    res = FrameRingBuffer_getFrame(keyHandle, &pRefData, &uRefDataLen);

    if (res != 0)
    {
        res = FrameRingBuffer_getFrame(keyHandle, &pRefData, &uRefDataLen);
    }

    return res;
}

int producer_taskAddVideoFrame(uint8_t *pData, size_t uLen, uint64_t uTimestamp, FrameKeyHandle keyHandle)
{
    int res = ERRNO_NONE;
    DataFrameCallbacks_t producerCallbacks = {0};

    if (gKvsAppHandle != NULL)
    {
        producerCallbacks.onDataFrameTerminateInfo.onDataFrameTerminate = producer_onDataFrameTerminateCallback;
        producerCallbacks.onDataFrameTerminateInfo.pAppData = NULL;
        producerCallbacks.onDataFrameToBeSentInfo.onDataFrameToBeSent = producer_onDataFrameToBeSent;
        producerCallbacks.onDataFrameToBeSentInfo.pAppData = keyHandle;

        res = KvsApp_addFrameWithCallbacks(gKvsAppHandle, pData, uLen, uLen, uTimestamp, TRACK_VIDEO, &producerCallbacks);
    }

    return res;
}

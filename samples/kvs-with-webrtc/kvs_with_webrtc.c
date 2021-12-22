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
#include <stdbool.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "kvs_with_webrtc.h"
#include "frame_ring_buffer/frame_ring_buffer.h"
#include "kvs/nalu.h"

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

#define MAX_FILENAME_PATH_SIZE   256

#define ANNEXB_TO_AVCC_EXTRA_BUFSIZE 16

#ifdef KVS_USE_POOL_ALLOCATOR
#include "kvs/pool_allocator.h"
static char pMemPool[POOL_ALLOCATOR_SIZE];
#endif

/* A global variable to exit program if it's set to true. It can be set to true if signal.h is available and user press Ctrl+c. It can also be set to true via debugger. */
bool gStopRunning = false;

#if ENABLE_WEBRTC
/* It's webrtc signal interrupt handler. */
extern void sigintHandler(int sigNum);
#endif

static void signalHandler(int signum)
{
    if (!gStopRunning)
    {
        printf("Received interrupt signal\n");
        gStopRunning = true;

#if ENABLE_WEBRTC
        sigintHandler(signum);
#endif
    }
    else
    {
        printf("Force leaving\n");
        exit(signum);
    }
}

static void sleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static uint64_t getTimestampInMs(void)
{
    struct timeval tv;
    uint64_t timestamp = 0;

    gettimeofday(&tv, NULL);
    timestamp = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

    return timestamp;
}

#if ENABLE_WEBRTC
static void *webrtcThread(void *arg)
{
    int argc = 1;
    char *argv[] = { "webrtc" };

    printf("Start %s task\n", argv[0]);
    webrtc_main(argc, argv);
    printf("Stop %s task\n", argv[0]);

    return NULL;
}
#endif /* #if ENABLE_WEBRTC */

#if ENABLE_PRODUCER
static void *producerThread(void *arg)
{
    int argc = 1;
    char *argv[] = { "producer" };

    sleepInMs(2000);
    printf("Start %s task\n", argv[0]);
    producer_main(argc, argv);
    printf("Stop %s task\n", argv[0]);

    return NULL;
}
#endif /* #if ENABLE_PRODUCER */

static int frameDestructor(uint8_t *pData, size_t uLen, FrameKeyHandle keyHandle, void *pAppData)
{
    if (pData != NULL)
    {
        free(pData);
    }

    return 0;
}

static int readFile(char *pfilePath, uint8_t **ppData, size_t *puLen)
{
    int res = ERRNO_NONE;
    FILE *fp = NULL;
    long xFileSize = 0;
    uint8_t *pData = NULL;

    if (pfilePath == NULL || ppData == NULL || puLen == NULL)
    {
        printf("Invalid parameter\n");
        res = ERRNO_FAIL;
    }
    else if ((fp = fopen(pfilePath, "rb")) == NULL)
    {
        printf("Failed to open file: %s\n", pfilePath);
        res = ERRNO_FAIL;
    }
    else if (fseek(fp, 0L, SEEK_END) != 0 ||
             ((xFileSize = ftell(fp)) < 0) ||
             fseek(fp, 0L, SEEK_SET) != 0)
    {
        printf("Failed to calculate file size\n");
        res = ERRNO_FAIL;
    }
    else if ((pData = (uint8_t *)malloc(xFileSize + ANNEXB_TO_AVCC_EXTRA_BUFSIZE)) == NULL)
    {
        printf("OOM: pData for file %s\n", pfilePath);
        res = ERRNO_FAIL;
    }
    else if (fread(pData, 1, xFileSize, fp) != xFileSize)
    {
        printf("Failed to read file %s\n", pfilePath);
        res = ERRNO_FAIL;
    }
    else
    {
        *ppData = pData;
        *puLen = xFileSize;
    }

    if (fp != NULL)
    {
        fclose(fp);
    }

    return res;
}

static void *videoSourceThread(void *arg)
{
    FrameRingBufferHandle frameRingBufferHandle = (FrameRingBufferHandle)arg;
    FrameKeyHandle frameKeyHandle = NULL;
    uint64_t uCurrentTimestamp = 0;
    int xFileIdx = 0;
    char pFilePath[MAX_FILENAME_PATH_SIZE];
    uint8_t *pData = NULL;
    size_t uLen = 0;
    FrameDestructorInfo_t frameDestructorInfo = {
        .frameDestructor = frameDestructor,
        .pAppData = NULL
    };

    while (!gStopRunning)
    {
        xFileIdx = xFileIdx % NUMBER_OF_VIDEO_FRAME_FILES + 1;
        snprintf(pFilePath, MAX_FILENAME_PATH_SIZE, VIDEO_FRAME_FILEPATH_FORMAT, xFileIdx);
        readFile(pFilePath, &pData, &uLen);
        uCurrentTimestamp = getTimestampInMs();

        NALU_convertAnnexBToAvccInPlace(pData, uLen, uLen + ANNEXB_TO_AVCC_EXTRA_BUFSIZE, (uint32_t *)&(uLen));

        frameKeyHandle = FrameRingBuffer_enqueue(frameRingBufferHandle, pData, uLen, &frameDestructorInfo);

#if ENABLE_WEBRTC
        webrtc_taskAddVideoFrame(pData, uLen, uCurrentTimestamp, frameKeyHandle);
#endif /* #if ENABLE_WEBRTC */

#if ENABLE_PRODUCER
        producer_taskAddVideoFrame(pData, uLen, uCurrentTimestamp, frameKeyHandle);
#endif /* #if ENABLE_PRODUCER */

        sleepInMs(1000 / VIDEO_FPS);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t videoSourceTid = 0;
    pthread_t producerTid = 0;
    pthread_t webrtcTid = 0;
    FrameRingBufferHandle frameRingBufferHandle = NULL;

    signal(SIGINT, signalHandler);

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorInit((void *)pMemPool, sizeof(pMemPool));
#endif

    DropFramePolicy_t dropFramePolicy = {0};
    dropFramePolicy.type = eDropOldest;
    dropFramePolicy.u.xDropOldestPolicyParameter.uMaxMem = FRAME_RING_BUFFER_MAX_MEMORY_LIMIT;

    if ((frameRingBufferHandle = FrameRingBuffer_create(FRAME_RING_BUFFER_CAPACITY)) == NULL ||
        FrameRingBuffer_setDropFramePolicy(frameRingBufferHandle, &dropFramePolicy) != 0)
    {
        printf("Failed to create frame ring buffer\n");
    }
#if ENABLE_WEBRTC
    else if (pthread_create(&(webrtcTid), NULL, webrtcThread, NULL) != 0)
    {
        printf("Failed to create WebRTC main thread\n");
    }
#endif /* #if ENABLE_WEBRTC */
#if ENABLE_PRODUCER
    else if (pthread_create(&(producerTid), NULL, producerThread, NULL) != 0)
    {
        printf("Failed to create producer main thread\n");
    }
#endif /* #if ENABLE_PRODUCER */
    else if (pthread_create(&videoSourceTid, NULL, videoSourceThread, frameRingBufferHandle) != 0)
    {
        printf("Failed to create video source thread\n");
    }
    else
    {
        while(!gStopRunning)
        {
            sleepInMs(1000);
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

    pthread_join(videoSourceTid, NULL);

#if ENABLE_PRODUCER
    pthread_join(producerTid, NULL);
#endif /* #if ENABLE_PRODUCER */

#if ENABLE_WEBRTC
    pthread_join(webrtcTid, NULL);
#endif /* #if ENABLE_WEBRTC */

    FrameRingBuffer_terminate(frameRingBufferHandle);

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorDeinit();
#endif

    return 0;
}


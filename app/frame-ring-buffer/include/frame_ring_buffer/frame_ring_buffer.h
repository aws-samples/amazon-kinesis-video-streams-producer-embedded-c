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

#ifndef FRAME_RING_BUFFER_H
#define FRAME_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct FrameRingBuffer *FrameRingBufferHandle;

typedef struct FrameKey *FrameKeyHandle;

typedef struct FrameDestructorInfo
{
    int (*frameDestructor)(uint8_t *pData, size_t uLen, FrameKeyHandle keyHandle, void *pAppData);
    void *pAppData;
} FrameDestructorInfo_t;

typedef struct FrameRingBufferStatistics
{
    size_t uFrameUsedCount;
    size_t uFrameFreeCount;
    size_t uSumOfFrameMemory;
} FrameRingBufferStat_t;

typedef enum
{
    eDontDrop = 0,  /* It's default value and no drop frame policy. Frame is dropped manually by using dequeue API. */
    eDropOldest     /* Whenever frame exceeds the memory limit, it drops oldest frame first. */
} eDropFramePolicyType;

typedef struct DropOldestPolicyParameter
{
    size_t uMaxMem; /* Max memory limit. When the sum of frame lengths exceed this limit, it drops the oldest frame first. */
} DropOldestPolicyParameter_t;

typedef struct DropFramePolicy
{
    eDropFramePolicyType type;
    union
    {
        DropOldestPolicyParameter_t xDropOldestPolicyParameter;
    } u;
} DropFramePolicy_t;

/**
 * Create a frame ring buffer with assigned capacity.
 *
 * @param[in] uCapacity Capacity of the frame ring buffer
 * @return Handle of the frame ring buffer on success, or NULL otherwise
 */
FrameRingBufferHandle FrameRingBuffer_create(size_t uCapacity);

/**
 * Terminate frame ring buffer and free all resources.
 *
 * @param[in] handle Handle of the frame ring buffer
 */
void FrameRingBuffer_terminate(FrameRingBufferHandle handle);

/**
 * Enqueue a frame and return a key handle of the frame. This frame key handle is for a application to check and validate if the frame is still available.
 *
 * @param[in] handle Handle of the frame ring buffer
 * @param[in] pData Data pointer of the frame
 * @param[in] uLen Length of the frame
 * @param[in] pFrameDestructorInfo Destructor of the frame
 * @return Frame key handle on success, NULL otherwise
 */
FrameKeyHandle FrameRingBuffer_enqueue(FrameRingBufferHandle handle, uint8_t *pData, size_t uLen, FrameDestructorInfo_t *pFrameDestructorInfo);

/**
 * Specifically dequeue a frame. In most cases, frames are expected to dequeued by its policy instead of manually dequeued by this API.
 *
 * @param[in] handle Handle of the frame ring buffer
 * @return 0 on success, non-zero value otherwise
 */
int FrameRingBuffer_dequeue(FrameRingBufferHandle handle);

/**
 * Get a frame by frame key handle. This API can be used to validate a frame key handle.
 *
 * @param[in] keyHandle Frame key handle
 * @param[out] ppData Pointer of the frame
 * @param[out] puLen Length of the frame
 * @return 0 on success, non-zero value otherwise
 */
int FrameRingBuffer_getFrame(FrameKeyHandle keyHandle, uint8_t **ppData, size_t *puLen);

/**
 * Get statistics of frame ring buffer.
 *
 * @param[in] handle Handle of the frame ring buffer
 * @param[out] pStat Statistics of frame ring buffer.
 * @return 0 on success, non-zero value otherwise
 */
int FrameRingBuffer_getMemoryStat(FrameRingBufferHandle handle, FrameRingBufferStat_t *pStat);

/**
 * Set drop frame policy to a frame ring buffer. It's applied whenever a frame is enqueued or this API is called.
 *
 * @param[in] handle Handle of the frame ring buffer
 * @param[in] pPolicy Drop frame policy
 * @return 0 on success, non-zero value otherwise
 */
int FrameRingBuffer_setDropFramePolicy(FrameRingBufferHandle handle, DropFramePolicy_t *pPolicy);

#endif /* FRAME_RING_BUFFER_H */
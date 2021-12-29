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

#ifndef KVS_WITH_WEBRTC_H
#define KVS_WITH_WEBRTC_H

#include <stdint.h>
#include "frame_ring_buffer/frame_ring_buffer.h"

#define FRAME_RING_BUFFER_CAPACITY  (30 * 3)
#define FRAME_RING_BUFFER_MAX_MEMORY_LIMIT  (1536 * 1024)

#define ENABLE_PRODUCER     (1)
#define ENABLE_WEBRTC       (1)

#ifdef KVS_USE_POOL_ALLOCATOR

/* This size is not only for APP but also for avoiding memory fragmentation. */
#define POOL_ALLOCATOR_SIZE_FOR_APP                 (1 * 1024 * 1024)

#define POOL_ALLOCATOR_SIZE_FOR_PRODUCER            (128 * 1024)

#define EXPECTED_WEBRTC_VIEWER_COUNT                (1)
#define POOL_ALLOCATOR_SIZE_FOR_WEBRTC              (384 * 1024)
#define POOL_ALLOCATOR_SIZE_FOR_WEBRTC_ONE_VIEWER   (3 * 1024 * 1024)

#define POOL_ALLOCATOR_SIZE \
    POOL_ALLOCATOR_SIZE_FOR_APP + \
    FRAME_RING_BUFFER_MAX_MEMORY_LIMIT + \
    ENABLE_PRODUCER * POOL_ALLOCATOR_SIZE_FOR_PRODUCER + \
    ENABLE_WEBRTC * (POOL_ALLOCATOR_SIZE_FOR_WEBRTC + EXPECTED_WEBRTC_VIEWER_COUNT * POOL_ALLOCATOR_SIZE_FOR_WEBRTC_ONE_VIEWER)

#endif /* KVS_USE_POOL_ALLOCATOR */

#define NUMBER_OF_VIDEO_FRAME_FILES     240
#define VIDEO_FRAME_FILEPATH_FORMAT     "../res/media/h264_annexb/frame-%03d.h264"
#define VIDEO_FPS                       30

int producer_main(int argc, char *argv[]);
int producer_taskAddVideoFrame(uint8_t *pData, size_t uLen, uint64_t uTimestamp, FrameKeyHandle keyHandle);

int webrtc_main(int argc, char* argv[]);
int webrtc_taskAddVideoFrame(uint8_t *pData, size_t uLen, uint64_t uTimestamp, FrameKeyHandle keyHandle);

#endif
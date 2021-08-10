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

#ifndef KVSAPP_H
#define KVSAPP_H

#include "kvs/kvsapp_options.h"
#include "kvs/mkv_generator.h"
#include <inttypes.h>

typedef struct KvsApp *KvsAppHandle;

/**
 * Create a KVS application.
 *
 * @param[in] pcHost KVS hostname
 * @param[in] pcRegion Region to be used
 * @param[in] pcService Service name, it should always be "kinesisvideo"
 * @param[in] pcStreamName KVS stream name
 * @return KVS application handle
 */
KvsAppHandle KvsApp_create(char *pcHost, char *pcRegion, char *pcService, char *pcStreamName);

/**
 * Terminate a KVS application.
 *
 * @param[in] handle KVS application handle
 */
void KvsApp_terminate(KvsAppHandle handle);

/**
 * Set option to KVS application.
 *
 * @param[in] handle KVS application handle
 * @param[in] pcOptionName Option name
 * @param[in] pValue Value
 * @return 0 on success, non-zero value otherwise
 */
int KvsApp_setoption(KvsAppHandle handle, const char *pcOptionName, const char *pValue);

/**
 * Open KVS application. It includes validating if stream exist, getting PUT_MEDIA data endpoint, and setup PUT_MEDIA
 * connection. It also tries to setup stream buffer if track info are already set.
 *
 * @param[in] handle KVS application handle.
 * @return 0 on success, non-zero value otherwise
 */
int KvsApp_open(KvsAppHandle handle);

/**
 * Close KVS application. It includes closing connection, flush stream buffer and terminate it.
 *
 * @param[in] handle KVS application handle
 * @return 0 on success, non-zero value otherwise
 */
int KvsApp_close(KvsAppHandle handle);

/**
 * Add a frame to KVS application. If the stream buffer is not allocated yet, then it'll try to parse decode information
 * and then setup stream buffer.
 *
 * @param[in] handle KVS application handle
 * @param[in] pData Data buffer pointer
 * @param[in] uDataLen Data length
 * @param[in] uDataSize Data buffer size
 * @param[in] uTimestamp Frame absolution timestamp in milliseconds.
 * @param[in] xTrackType Track type, it could be TRACK_VIDEO or TRACK_AUDIO
 * @return 0 on success, non-zero value otherwise
 */
int KvsApp_addFrame(KvsAppHandle handle, uint8_t *pData, size_t uDataLen, size_t uDataSize, uint64_t uTimestamp, TrackType_t xTrackType);

/**
 * Let KVS application do works. It will try to send out frames, and check if any messages from server.
 *
 * @param[in] handle KVS application handle
 * @return 0 on success, non-zero value otherwise
 */
int KvsApp_doWork(KvsAppHandle handle);

/**
 * Get the memory used in the stream buffer.
 *
 * @param handle  KVS application handle
 * @return Memory used in current stream buffer, zero value otherwise
 */
size_t KvsApp_getStreamMemStatTotal(KvsAppHandle handle);

#endif /* KVSAPP_H */

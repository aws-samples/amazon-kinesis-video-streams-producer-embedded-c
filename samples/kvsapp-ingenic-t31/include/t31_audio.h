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

#ifndef T31_AUDIO_H
#define T31_AUDIO_H

#include "kvs/stream.h"

typedef struct T31Audio *T31AudioHandle;

/**
 * Create a T31 audio and audio thread.
 *
 * @param kvsAppHandle KVS app handle. This handle is for adding frame into KVS stream buffer.
 * @return T31 audio handle
 */
T31AudioHandle T31Audio_create(KvsAppHandle kvsAppHandle);

/**
 * Terminate T31 audio and its thread.
 *
 * @param handle T31 audio handle
 */
void T31Audio_terminate(T31AudioHandle handle);

/**
 * Get audio track info for KVS
 *
 * @param handle T31 audio handle
 * @return Audio track info
 */
AudioTrackInfo_t *T31Audio_getAudioTrackInfoClone(T31AudioHandle handle);

#endif

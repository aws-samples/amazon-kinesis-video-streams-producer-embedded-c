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

#ifndef T31_VIDEO_H
#define T31_VIDEO_H

#include "kvs/kvsapp.h"

typedef struct T31Video *T31VideoHandle;

/**
 * Create a T31 video and video thread.
 * @param kvsAppHandle KVS app handle. This handle is for adding frame into KVS stream buffer.
 * @return T31 video handle
 */
T31VideoHandle T31Video_create(KvsAppHandle kvsAppHandle);

/**
 * Terminate T31 video and its thread.
 *
 * @param handle T31 video handle
 */
void T31Video_terminate(T31VideoHandle handle);

#endif

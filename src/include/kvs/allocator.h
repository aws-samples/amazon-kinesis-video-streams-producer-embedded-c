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

#ifndef KVS_ALLOCATOR_H
#define KVS_ALLOCATOR_H

#ifndef KVS_USE_POOL_ALLOCATOR
#include <stdlib.h>

#define KVS_MALLOC          malloc
#define KVS_REALLOC         realloc
#define KVS_CALLOC          calloc
#define KVS_FREE            free
#else
#include "kvs/pool_allocator.h"

void *__wrap_malloc(size_t bytes);
void *__wrap_realloc(void *ptr, size_t bytes);
void *__wrap_calloc(size_t num, size_t bytes);
void __wrap_free(void *ptr);

void *__real_malloc(size_t);
void *__real_realloc(void *ptr, size_t bytes);
void *__real_calloc(size_t num, size_t bytes);
void __real_free(void *ptr);

#define KVS_MALLOC          poolAllocatorMalloc
#define KVS_REALLOC         poolAllocatorRealloc
#define KVS_CALLOC          poolAllocatorCalloc
#define KVS_FREE            poolAllocatorFree
#endif /* KVS_USE_POOL_ALLOCATOR */

#endif /* KVS_ALLOCATOR_H */

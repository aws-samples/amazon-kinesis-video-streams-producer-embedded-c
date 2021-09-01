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

#include <stdlib.h>

/* Public headers */
#include "kvs/pool_allocator.h"

/* Internal headers */
#include "allocator.h"

void *kvsMalloc(size_t bytes)
{
    return malloc(bytes);
}

void *kvsRealloc(void *ptr, size_t bytes)
{
    return realloc(ptr, bytes);
}

void *kvsCalloc(size_t num, size_t bytes)
{
    return calloc(num, bytes);
}

void kvsFree(void *ptr)
{
    free(ptr);
}

/**
 * Wrapper of KVS malloc that use pool allocator malloc.
 *
 * @param[in] bytes Memory size
 * @return New allocated address on success, NULL otherwise
 */
void *__wrap_kvsMalloc(size_t bytes)
{
    return poolAllocatorMalloc(bytes);
}

/**
 * Wrapper of KVS realloc that use pool allocator realloc.
 *
 * @param[in] ptr Pointer to be re-allocated
 * @param[in] bytes New memory size
 * @return New allocated address on success, NULL otherwise
 */
void *__wrap_kvsRealloc(void *ptr, size_t bytes)
{
    return poolAllocatorRealloc(ptr, bytes);
}

/**
 * Wrapper of KVS calloc that use pool allocator calloc.
 *
 * @param[in] num Number of elements
 * @param[in] bytes Element size
 * @return Newly allocated address on success, NULL otherwise
 */
void *__wrap_kvsCalloc(size_t num, size_t bytes)
{
    return poolAllocatorCalloc(num, bytes);
}

/**
 * Wrapper of KVS free that use pool allocator free.
 *
 * @param[in] ptr Memory pointer to be freed
 */
void __wrap_kvsFree(void *ptr)
{
    poolAllocatorFree(ptr);
}

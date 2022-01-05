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

#ifdef KVS_USE_POOL_ALLOCATOR

#include "kvs/pool_allocator.h"

/**
 * Wrap malloc and make it use pool allocator malloc.
 *
 * @param size Memory size
 * @return New allocated address on success, NULL otherwise
 */
void *__wrap_malloc(size_t size)
{
    return poolAllocatorMalloc(size);
}

/**
 * Wrap realloc and make it use pool allocator realloc.
 *
 * @param[in] ptr Pointer to be re-allocated
 * @param[in] bytes New memory size
 * @return New allocated address on success, NULL otherwise
 */
void *__wrap_realloc(void *ptr, size_t bytes)
{
    return poolAllocatorRealloc(ptr, bytes);
}

/**
 * Overwrite calloc and make it use pool allocator calloc.
 *
 * @param[in] num Number of elements
 * @param[in] bytes Element size
 * @return Newly allocated address on success, NULL otherwise
 */
void *__wrap_calloc(size_t num, size_t bytes)
{
    return poolAllocatorCalloc(num, bytes);
}

/**
 * Overwrite free and make it use pool allocator free.
 *
 * @param[in] ptr Memory pointer to be freed
 */
void __wrap_free(void *ptr)
{
    poolAllocatorFree(ptr);
}

#endif /* KVS_USE_POOL_ALLOCATOR */
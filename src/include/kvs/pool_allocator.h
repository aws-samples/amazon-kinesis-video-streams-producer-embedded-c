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

#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <stddef.h>

typedef struct PoolStats
{
    size_t uNumberOfUsedBlocks;
    size_t uNumberOfFreeBlocks;
    size_t uSumOfUsedMemory;
    size_t uSumOfFreeMemory;
    size_t uSizeOfLargestUsedBlock;
    size_t uSizeOfLargestFreeBlock;
} PoolStats_t;

/**
 * Init pool allocator with specified size. The pool is allocated from default malloc.
 *
 * @param[in] pMemBuf Pointer of memory pool
 * @param[in] bytes Memory pool size
 * @return 0 on success, negative value otherwise
 */
int poolAllocatorInit(void *pMemPool, size_t bytes);

/**
 * Allocate memory from memory pool. If pool allocator hasn't been initialized, then it'll be initialized here with default size.
 *
 * @param bytes Size of memory
 * @return Newly allocated memory on success, NULL otherwise
 */
void *poolAllocatorMalloc(size_t bytes);

/**
 * Re-allocate memory from memory pool. If pool allocator hasn't been initialized, then it'll be initialized here with default size.
 *
 * @param[in] ptr Pointer to be re-allocate
 * @param[in] bytes New memory size
 * @return Newly allocated memory on success, NULL otherwise
 */
void *poolAllocatorRealloc(void *ptr, size_t bytes);

/**
 * Allocate and clear memory from memory pool. If pool allocator hasn't been initialized, then it'll be initialized here with default size.
 *
 * @param[in] num Number of elements
 * @param[in] bytes Element size
 * @return Newly allocated memory on success, NULL otherwise
 */
void *poolAllocatorCalloc(size_t num, size_t bytes);

/**
 * Free memory from memory pool.
 *
 * @param[in] ptr Pointer to be freed
 */
void poolAllocatorFree(void *ptr);

/**
 * Deinit pool allocator.
 */
void poolAllocatorDeinit(void);

/**
 * Get statistics of pool allocator.
 *
 * @param[in] pPoolStats Pool statistics
 */
void poolAllocatorGetStats(PoolStats_t *pPoolStats);

#endif /* POOL_ALLOCATOR_H */

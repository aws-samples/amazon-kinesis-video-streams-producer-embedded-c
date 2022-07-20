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

#include <pthread.h>
#include <string.h>

/* Third party headers */
#include "tlsf.h"

/* Public headers */
#include "kvs/errors.h"
#include "kvs/pool_allocator.h"

static pthread_mutex_t memPoolMutex = PTHREAD_MUTEX_INITIALIZER;
static tlsf_t tlsf = NULL;
static void *pMem = NULL;

int poolAllocatorInit(void *pMemPool, size_t bytes)
{
    int res = 0;

    if (pMemPool == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        pthread_mutex_lock(&memPoolMutex);
        if (pMem == NULL)
        {
            pMem = pMemPool;
            tlsf = tlsf_create_with_pool(pMem, bytes);
            if (tlsf == NULL)
            {
                pMem = NULL;
                res = KVS_ERROR_TLSF_FAILED_TO_CREATE_POOL;
            }
        }
        pthread_mutex_unlock(&memPoolMutex);
    }

    return res;
}

void *poolAllocatorMalloc(size_t bytes)
{
    void *pNewPtr = NULL;

    pthread_mutex_lock(&memPoolMutex);
    if (tlsf != NULL)
    {
        pNewPtr = tlsf_malloc(tlsf, bytes);
    }
    pthread_mutex_unlock(&memPoolMutex);

    return pNewPtr;
}

void *poolAllocatorRealloc(void *ptr, size_t bytes)
{
    void *pNewPtr = NULL;

    pthread_mutex_lock(&memPoolMutex);
    if (tlsf != NULL)
    {
        pNewPtr = tlsf_realloc(tlsf, ptr, bytes);
    }
    pthread_mutex_unlock(&memPoolMutex);

    return pNewPtr;
}

void *poolAllocatorCalloc(size_t num, size_t bytes)
{
    void *pNewPtr = NULL;

    pNewPtr = poolAllocatorMalloc(num * bytes);
    if (pNewPtr != NULL)
    {
        memset(pNewPtr, 0, num * bytes);
    }

    return pNewPtr;
}

void poolAllocatorFree(void *ptr)
{
    if (ptr != NULL && tlsf != NULL)
    {
        pthread_mutex_lock(&memPoolMutex);
        tlsf_free(tlsf, ptr);
        pthread_mutex_unlock(&memPoolMutex);
    }
}

void poolAllocatorDeinit(void)
{
    pthread_mutex_lock(&memPoolMutex);
    if (tlsf != NULL)
    {
        tlsf_destroy(tlsf);
        tlsf = NULL;
    }
    if (pMem != NULL)
    {
        pMem = NULL;
    }
    pthread_mutex_unlock(&memPoolMutex);
}

static void prvTlsfPoolWalker(void *ptr, size_t size, int used, void *pUser)
{
    PoolStats_t *pStats = (PoolStats_t *)pUser;
    if (used)
    {
        pStats->uNumberOfUsedBlocks++;
        pStats->uSumOfUsedMemory += size;
        if (size > pStats->uSizeOfLargestUsedBlock)
        {
            pStats->uSizeOfLargestUsedBlock = size;
        }
    }
    else
    {
        pStats->uNumberOfFreeBlocks++;
        pStats->uSumOfFreeMemory += size;
        if (size > pStats->uSizeOfLargestFreeBlock)
        {
            pStats->uSizeOfLargestFreeBlock = size;
        }
    }
}

void poolAllocatorGetStats(PoolStats_t *pPoolStats)
{
    pthread_mutex_lock(&memPoolMutex);
    if (tlsf != NULL && pMem != NULL)
    {
        tlsf_walk_pool(tlsf_get_pool(tlsf), prvTlsfPoolWalker, pPoolStats);
    }
    pthread_mutex_unlock(&memPoolMutex);
}


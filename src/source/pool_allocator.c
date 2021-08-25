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

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "tlsf.h"

#include "kvs/allocator.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static tlsf_t tlsf = NULL;
static void *mem = NULL;

int poolAllocatorInit(size_t bytes)
{
    pthread_mutex_lock(&mutex);
    mem = __real_malloc(bytes);
    if (!mem)
    {
        pthread_mutex_unlock(&mutex);
        return -ENOMEM;
    }
    tlsf = tlsf_create_with_pool(mem, bytes);
    pthread_mutex_unlock(&mutex);
    return 0;
}

void *poolAllocatorMalloc(size_t bytes)
{
    pthread_mutex_lock(&mutex);
    void *ret = tlsf_malloc(tlsf, bytes);
    pthread_mutex_unlock(&mutex);
    return ret;
}

void *poolAllocatorRealloc(void *ptr, size_t bytes)
{
    pthread_mutex_lock(&mutex);
    void *ret = tlsf_realloc(tlsf, ptr, bytes);
    pthread_mutex_unlock(&mutex);
    return ret;
}

void *poolAllocatorCalloc(size_t num, size_t bytes)
{
    pthread_mutex_lock(&mutex);
    void *ret = tlsf_malloc(tlsf, num * bytes);
    memset(ret, 0, num * bytes);
    pthread_mutex_unlock(&mutex);
    return ret;
}

void poolAllocatorFree(void *ptr)
{
    pthread_mutex_lock(&mutex);
    tlsf_free(tlsf, ptr);
    pthread_mutex_unlock(&mutex);
}

void poolAllocatorDeinit(void)
{
    pthread_mutex_lock(&mutex);
    if (tlsf)
    {
        tlsf_destroy(tlsf);
        tlsf = NULL;
    }
    if (mem)
    {
        free(mem);
        mem = NULL;
    }
    pthread_mutex_unlock(&mutex);
}

void *__wrap_malloc(size_t size)
{
    return poolAllocatorMalloc(size);
}

void *__wrap_realloc(void *ptr, size_t bytes)
{
    return poolAllocatorRealloc(ptr, bytes);
}

void *__wrap_calloc(size_t num, size_t bytes)
{
    return poolAllocatorCalloc(num, bytes);
}

void __wrap_free(void *ptr)
{
    poolAllocatorFree(ptr);
}

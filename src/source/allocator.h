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

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

/**
 * KVS memory allocation.
 *
 * @param[in] bytes Memory size
 * @return New allocated address on success, NULL otherwise
 */
void *kvsMalloc(size_t bytes);

/**
 * KVS memory re-allocation.
 *
 * @param[in] ptr Pointer to be re-allocated
 * @param[in] bytes New memory size
 * @return New allocated address on success, NULL otherwise
 */
void *kvsRealloc(void *ptr, size_t bytes);

/**
 * KVS memory clear allocation.
 *
 * @param[in] num Number of elements
 * @param[in] bytes Element size
 * @return Newly allocated address on success, NULL otherwise
 */
void *kvsCalloc(size_t num, size_t bytes);

/**
 * KVS memory free allocation.
 *
 * @param[in] ptr Memory pointer to be freed
 */
void kvsFree(void *ptr);

#endif /* ALLOCATOR_H */

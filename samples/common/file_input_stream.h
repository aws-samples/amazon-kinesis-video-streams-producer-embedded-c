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

#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

typedef struct FileInputStream
{
    FILE *fp;
    long int xFileSize;
    long int xFileIdx;

    uint8_t *pBuf;
    size_t uDataLen;
    size_t uBufSize;
} FileInputStream_t;

/**
 * @brief Create a file input stream
 *
 * This file input stream is an input stream with a buffer within.  This buffer is a kind of sliding window of the file.
 * If application keeps using readIntoBuf without consumeBuf, then the buffer size would enlarge and keep more data of
 * the file.
 *
 * @param[in] pcFilename filename
 * @return handle of file input stream
 */
FileInputStream_t *FIS_create(const char *pcFilename);

/**
 * @brief Terminate file input stream
 * @param[in] pFs handle of file input stream
 */
void FIS_terminate(FileInputStream_t *pFs);

/**
 * @brief Read file content into buffer
 *
 * If the buffer is not full, then this function will try to fill the buffer until full or the end of the file.  If the
 * buffer is full already, then this function will double the size of buffer, and then fill it.
 *
 * @param[in] pFs handle of file input stream
 * @return 0 on success, non-zero value otherwise
 */
int FIS_readIntoBuf(FileInputStream_t *pFs);

/**
 * @brief Consume the buffer with specific size.
 *
 * @param[in] pFs handle of file input stream
 * @param[in] uSize Consumed size
 * @return 0 on success, non-zero value otherwise
 */
int FIS_consumeBuf(FileInputStream_t *pFs, size_t uSize);

#endif /* FILE_STREAM_H */
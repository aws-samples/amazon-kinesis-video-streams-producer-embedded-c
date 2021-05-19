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

#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>

/**
 * @brief Get filesize by given a filename
 *
 * @param[in] pcFilename filename
 * @param[out] puFileSize the size of the file
 * @return 0 on success, non-zero value otherwise
 */
int getFileSize(char *pcFilename, size_t *puFileSize);

/**
 * @brief Read a whole file into a buffer
 *
 * @param[in] pcFilename filename
 * @param[out] pBuf buffer that stores the content of the file
 * @param[in] uBufSize buffer size
 * @param[out] puBytesRead the actual bytes are stored into the buffer
 * @return 0 on success, non-zero value otherwise
 */
int readFile(char *pcFilename, char *pBuf, size_t uBufSize, size_t *puBytesRead);

#endif /* FILE_IO_H */
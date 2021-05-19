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

#ifndef H264_FILE_LOADER_H
#define H264_FILE_LOADER_H

#include <stddef.h>
#include "kvs/mkv_generator.h"
#include "file_loader.h"

typedef struct H264FileLoader *H264FileLoaderHandle;

/**
 * @brief Create a H264 file loader
 *
 * @param[in] pFileLoaderPara file loader parameter that describe the filename format, start index, and end index
 * @return handle of the H264 file loader on success, NULL otherwise
 */
H264FileLoaderHandle H264FileLoaderCreate(FileLoaderPara_t *pFileLoaderPara);

/**
 * @brief Terminate H264 file loader
 * @param[in] xLoader handle of the H264 file loader
 */
void H264FileLoaderTerminate(H264FileLoaderHandle xLoader);

/**
 * @brief Load a H264 file into a memory allocated pointer
 *
 * @param[in] xLoader handle of the H264 file loader
 * @param[out] ppData H264 frame pointer
 * @param[out] puDataLen H264 frame length
 * @return 0 on success, non-zero value otherwise
 */
int H264FileLoaderLoadFrame(H264FileLoaderHandle xLoader, char **ppData, size_t *puDataLen);

/**
 * @brief Get video track info
 *
 * @param[in] xLoader handle of the H264 file loader
 * @return Video track info on success, NULL otherwise
 */
VideoTrackInfo_t *H264FileLoaderGetVideoTrackInfo(H264FileLoaderHandle xLoader);

#endif /* H264_FILE_LOADER_H */
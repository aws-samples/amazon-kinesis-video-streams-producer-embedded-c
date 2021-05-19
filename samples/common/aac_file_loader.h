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

#ifndef AAC_FILE_LOADER_H
#define AAC_FILE_LOADER_H

#include <stddef.h>
#include "kvs/mkv_generator.h"
#include "file_loader.h"

typedef struct AacFileLoader *AacFileLoaderHandle;

/**
 * @brief Create a AAC file loader
 *
 * This file loader loads AAC files iteratively.  The filename format, start index, and end index are defined in
 * pFileLoaderPara.
 *
 * @param[in] pFileLoaderPara file loader parameter that describe the filename format, start index, and end index
 * @param[in] xObjectType MPEG4 audio object type
 * @param[in] uFrequency Audio frequency/sampling rate
 * @param[in] uChannelNumber Audio channel number
 * @return handle of the AAC file loader on success, NULL otherwise
 */
AacFileLoaderHandle AacFileLoaderCreate(FileLoaderPara_t *pFileLoaderPara, Mpeg4AudioObjectTypes_t xObjectType, uint32_t uFrequency, uint16_t uChannelNumber);

/**
 * @brief Terminate AAC file loader
 *
 * @param[in] xLoader The AAC file loader
 */
void AacFileLoaderTerminate(AacFileLoaderHandle xLoader);

/**
 * @brief Load a AAC file into a memory allocated pointer
 *
 * @param[in] xLoader handle of AAC file loader
 * @param[out] ppData AAC frame pointer
 * @param[out] puDataLen length of the frame
 * @return 0 on success, non-zero value otherwise
 */
int AacFileLoaderLoadFrame(AacFileLoaderHandle xLoader, char **ppData, size_t *puDataLen);

/**
 * @brief Get audio track info
 *
 * @param[in] xLoader handle of AAC file loader
 * @return audio track info on success, NULL otherwise
 */
AudioTrackInfo_t *AacFileLoaderGetAudioTrackInfo(AacFileLoaderHandle xLoader);

#endif /* AAC_FILE_LOADER_H */
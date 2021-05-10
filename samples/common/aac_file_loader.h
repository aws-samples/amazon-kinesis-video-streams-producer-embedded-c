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

AacFileLoaderHandle AacFileLoaderCreate(FileLoaderPara_t *pFileLoaderPara, Mpeg4AudioObjectTypes_t xObjectType, uint32_t uFrequency, uint16_t uChannelNumber);
void AacFileLoaderTerminate(AacFileLoaderHandle xLoader);

int AacFileLoaderLoadFrame(AacFileLoaderHandle xLoader, char **ppData, size_t *puDataLen);
AudioTrackInfo_t *AacFileLoaderGetAudioTrackInfo(AacFileLoaderHandle xLoader);

#endif /* AAC_FILE_LOADER_H */
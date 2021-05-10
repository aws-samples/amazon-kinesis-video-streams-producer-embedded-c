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

#ifndef SPS_DECODE_H
#define SPS_DECODE_H

/**
 * @breif Get H264 resolution from SPS
 *
 * @param[in] pSps The SPS buffer
 * @param[in] uSpsLen The length of SPS buffer
 * @param[out] puWidth The width of H264 video
 * @param[out] puHeight The height of H264 video
 */
void getH264VideoResolution(char *pSps, size_t uSpsLen, uint16_t *puWidth, uint16_t *puHeight);

#endif
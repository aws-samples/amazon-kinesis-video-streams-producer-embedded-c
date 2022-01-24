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

#ifndef ALAW_ENCODER_H
#define ALAW_ENCODER_H

#include <stddef.h>
#include <stdint.h>

/**
 * Create an PCM a-law encoder.
 *
 * @param[in] uSampleRate The audio sample rate
 * @param[in] uChannels The audio channel numbers
 * @param[in] uBitRate The bit rate. In most cases, one PCM sample is 16 bits long. So it equals uSampleRate * uChannels * 16.
 * @return The handle of the PCM a-law encoder.
 */
void *AlawEncoder_create(const unsigned int uSampleRate, const unsigned int uChannels, const unsigned int uBitRate);

/**
 * Terminate the PCM a-law encoder
 *
 * @param[in] pHandle The PCM a-law encoder handle
 */
void AlawEncoder_terminate(void *pHandle);

/**
 * Set parameter to the PCM a-law encoder.
 *
 * @param[in] pHandle The PCM a-law encoder handle
 * @param[in] pKey The key of the parameter
 * @param[in] pValue The value of the parameter
 * @return 0 on success, non-zero value otherwise
 */
int AlawEncoder_setParameter(void *pHandle, char *pKey, void *pValue);

/**
 * Get parameter from the PCM a-law encoder.
 *
 * @param[in] pHandle The PCM a-law encoder handle
 * @param[in] pKey The key of the parameter
 * @param[out] pValue The value of the parameter
 * @return 0 on success, non-zero value otherwise
 */
int AlawEncoder_getParameter(void *pHandle, char *pKey, void *pValue);

/**
 * Encode a PCM data buffer into an PCM a-law data buffer. If the pEncBuf is NULL, it will just calculate the needed encode buffer size.
 *
 * @param[in] pHandle The PCM a-law encoder handle
 * @param[in] pPcmBuf The pointer of PCM buffer
 * @param[in] uPcmBufLen The data length in bytes of PCM buffer
 * @param[in] pEncBuf The pointer of the encoding buffer
 * @param[in] uEncBufSize The size of the encoding buffer
 * @param[out] puEncBufLen The length in bytes of the encoding buffer
 * @param[out] puPcmBufUsed The byte counts of PCM data were being used to do the encoding
 * @param[out] puTimestampMs The timestamp of the encoding data
 * @return 0 on success, non-zero value otherwise
 */
int AlawEncoder_encode(void *pHandle, uint8_t *pPcmBuf, size_t uPcmBufLen, uint8_t *pEncBuf, size_t uEncBufSize, size_t *puEncBufLen, size_t *puPcmBufUsed, uint64_t *puTimestampMs);

#endif /* ALAW_ENCODER_H */
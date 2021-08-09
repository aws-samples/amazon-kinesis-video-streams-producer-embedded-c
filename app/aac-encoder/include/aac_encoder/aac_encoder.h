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

#ifndef AAC_ENCODER_H
#define AAC_ENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif

#define AAC_OBJECT_TYPE_AAC_LC      2               // AOT_AAC_LC
#define AAC_OBJECT_TYPE_AAC_HE      5               // AOT_SBR
#define AAC_OBJECT_TYPE_AAC_HE_v2   29              // AOT_PS PS, Parametric Stereo (includes SBR)
#define AAC_OBJECT_TYPE_AAC_LD      23              // AOT_ER_AAC_LD Error Resilient(ER) AAC LowDelay object
#define AAC_OBJECT_TYPE_AAC_ELD     39              // AOT_ER_AAC_ELD AAC Enhanced Low Delay

typedef struct AacEncoder *AacEncoderHandle;

AacEncoderHandle AacEncoder_create(const unsigned int sample_rate, const unsigned int channels, const unsigned int bit_rate, const unsigned int xAacObjectType, size_t *puPcmFrameLen);

void AacEncoder_terminate(AacEncoderHandle xAacEncoderHandle);

int AacEncoder_encode(AacEncoderHandle xAacEncoderHandle, const uint8_t *pInputBuf, const int uInputBufLen, uint8_t *pOutputBuf, int *puOutputBufLen);

#ifdef __cplusplus
}
#endif

#endif /* AAC_ENCODER_H */

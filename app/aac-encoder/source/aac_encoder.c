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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <aacenc_lib.h>

#include "aac_encoder/aac_encoder.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

typedef struct AacEncoder
{
    HANDLE_AACENCODER aacEncoderHandle;
    AACENC_InfoStruct xEncInfo;
} AacEncoder_t;

AacEncoderHandle AacEncoder_create(const unsigned int sample_rate, const unsigned int channels, const unsigned int bit_rate, const unsigned int xAacObjectType, size_t *puPcmFrameLen)
{
    int res = ERRNO_NONE;
    AACENC_ERROR xAacencError = AACENC_OK;
    AacEncoder_t *pAacEncoder = NULL;

    if (sample_rate == 0 || channels == 0 || bit_rate == 0 || puPcmFrameLen == NULL)
    {
        printf("%s: Invalid parameters\n", __FUNCTION__);
    }
    else if ((pAacEncoder = (AacEncoder_t *)malloc(sizeof(AacEncoder_t))) == NULL)
    {
        printf("OOM: pAacEncoder\n");
    }
    else
    {
        memset(pAacEncoder, 0, sizeof(AacEncoder_t));

        if ((xAacencError = aacEncOpen(&(pAacEncoder->aacEncoderHandle), 0x00, channels)) != AACENC_OK)
        {
            printf("Failed to open fdkaac encoder, err:0x%04X\n", xAacencError);
            res = ERRNO_FAIL;
        }
        else if (
            (xAacencError = aacEncoder_SetParam(pAacEncoder->aacEncoderHandle, AACENC_AOT, xAacObjectType)) != AACENC_OK ||
            (xAacencError = aacEncoder_SetParam(pAacEncoder->aacEncoderHandle, AACENC_SAMPLERATE, sample_rate)) != AACENC_OK ||
            (xAacencError = aacEncoder_SetParam(pAacEncoder->aacEncoderHandle, AACENC_CHANNELMODE, channels)) != AACENC_OK ||
            (xAacencError = aacEncoder_SetParam(pAacEncoder->aacEncoderHandle, AACENC_BITRATE, bit_rate)) != AACENC_OK ||
            (xAacencError = aacEncoder_SetParam(pAacEncoder->aacEncoderHandle, AACENC_TRANSMUX, TT_MP4_RAW)) != AACENC_OK)
        {
            printf("Failed to set AAC encoder parameter, err:0x%04X\n", xAacencError);
            res = ERRNO_FAIL;
        }
        else if ((xAacencError = aacEncEncode(pAacEncoder->aacEncoderHandle, NULL, NULL, NULL, NULL)) != AACENC_OK)
        {
            printf("Failed to initialize aacEncEncode, err:0x%04X\n", xAacencError);
            res = ERRNO_FAIL;
        }
        else if ((xAacencError = aacEncInfo(pAacEncoder->aacEncoderHandle, &(pAacEncoder->xEncInfo))) != AACENC_OK)
        {
            printf("Failed to get aacEncInfo, err:0x%04X\n", xAacencError);
            res = ERRNO_FAIL;
        }
        else
        {
            *puPcmFrameLen = pAacEncoder->xEncInfo.inputChannels * pAacEncoder->xEncInfo.frameLength * 2;
            printf("AacEncoder_create: ch:%u pcmFrameLen:%u\n", pAacEncoder->xEncInfo.inputChannels, *puPcmFrameLen);
        }
    }

    if (res != ERRNO_NONE)
    {
        if (pAacEncoder != NULL)
        {
            if (pAacEncoder->aacEncoderHandle != NULL)
            {
                aacEncClose(&(pAacEncoder->aacEncoderHandle));
            }
            free(pAacEncoder);
            pAacEncoder = NULL;
        }
    }

    return pAacEncoder;
}

void AacEncoder_terminate(AacEncoderHandle xAacEncoderHandle)
{
    AacEncoder_t *pAacEncoder = (AacEncoder_t *)xAacEncoderHandle;

    if (pAacEncoder != NULL)
    {
        if (pAacEncoder->aacEncoderHandle != NULL)
        {
            aacEncClose(&(pAacEncoder->aacEncoderHandle));
        }
        free(pAacEncoder);
    }
}

int AacEncoder_encode(AacEncoderHandle xAacEncoderHandle, const uint8_t *pInputBuf, const int uInputBufLen, uint8_t *pOutputBuf, int *puOutputBufLen)
{
    AACENC_ERROR xAacencError = AACENC_OK;
    AacEncoder_t *pAacEncoder = (AacEncoder_t *)xAacEncoderHandle;

    if (pAacEncoder == NULL)
    {
        xAacencError = AACENC_INVALID_HANDLE;
    }
    else if (uInputBufLen != pAacEncoder->xEncInfo.inputChannels * pAacEncoder->xEncInfo.frameLength * 2)
    {
        xAacencError = AACENC_UNSUPPORTED_PARAMETER;
    }
    else
    {
        /* Setup peremeter AACENC_BufDesc */
        AACENC_BufDesc xInBuf = {0};
        void *pInBufs = (void *)pInputBuf;
        int xInBufferIdentifier = IN_AUDIO_DATA;
        int xInElSizes = 2;
        int xInBufSizes = uInputBufLen;
        xInBuf.numBufs = 1;
        xInBuf.bufs = &pInBufs;
        xInBuf.bufferIdentifiers = &xInBufferIdentifier;
        xInBuf.bufSizes = &xInBufSizes;
        xInBuf.bufElSizes = &xInElSizes;

        AACENC_BufDesc xOutBuf = {0};
        void *pOutBufs = pOutputBuf;
        int xOutBufferIdentifier = OUT_BITSTREAM_DATA;
        int xOutBufSizes = *puOutputBufLen;
        int xOutElSizes = 1;
        xOutBuf.numBufs = 1;
        xOutBuf.bufs = &pOutBufs;
        xOutBuf.bufferIdentifiers = &xOutBufferIdentifier;
        xOutBuf.bufSizes = &xOutBufSizes;
        xOutBuf.bufElSizes = &xOutElSizes;

        AACENC_InArgs xInArgs = {0};
        xInArgs.numInSamples = uInputBufLen / 2;

        AACENC_OutArgs xOutArgs = {0};

        if ((xAacencError = aacEncEncode(pAacEncoder->aacEncoderHandle, &xInBuf, &xOutBuf, &xInArgs, &xOutArgs)) != AACENC_OK)
        {
            printf("Failed to aacEncEncode, err:0x%04X\n", xAacencError);
        }
        *puOutputBufLen = xOutArgs.numOutBytes;
    }

    return (int)xAacencError;
}

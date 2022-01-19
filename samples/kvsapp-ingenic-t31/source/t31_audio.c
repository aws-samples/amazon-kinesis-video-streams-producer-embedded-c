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

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* T31 headers */
#include <imp/imp_audio.h>

/* KVS headers */
#include "kvs/kvsapp.h"
#include "kvs/port.h"

#include "sample_config.h"
#include "t31_audio.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#if ENABLE_AUDIO_TRACK
typedef struct AudioConfiguration
{
    int devID;
    int chnID;
    IMPAudioIOAttr attr;
    IMPAudioIChnParam chnParam;
    int chnVol;
    int aigain;

    int channelNumber;

    // FDK AAC parameters
    int sampleRate;
    int channel;
    int bitRate;
} AudioConfiguration_t;

typedef struct T31Audio
{
    pthread_mutex_t lock;

    pthread_t tid;
    bool isTerminating;
    bool isTerminated;

    // KVS
    KvsAppHandle kvsAppHandle;
    AudioTrackInfo_t *pAudioTrackInfo;

    AudioConfiguration_t xAudioConf;

#if USE_AUDIO_AAC
    uint64_t uPcmTimestamp;
    uint8_t *pPcmBuf;
    size_t uPcmOffset;
    size_t uPcmBufSize;

    uint8_t *pFrameBuf;
    int xFrameBufSize;
#endif /* USE_AUDIO_AAC */
} T31Audio_t;

#if USE_AUDIO_AAC
/* pAacCh1Sr8K is a silence AAC_LC audio with 1 channel and 8K sample rate. */
static const uint8_t pAacCh1Sr8K[768] = {
    0x01, 0x40, 0x42, 0x80, 0xA3, 0x7F, 0xF8, 0x85, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D,
    0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2E, 0xFF, 0xF1, 0x0A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
    0x5A, 0x5A, 0x5D, 0xF9, 0xA2, 0x14, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
    0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xBC,

};
#endif /* USE_AUDIO_AAC */

static void prvSleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static int audioConfigurationInit(AudioConfiguration_t *pConf)
{
    int res = ERRNO_NONE;

    if (pConf == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pConf, 0, sizeof(AudioConfiguration_t));

        pConf->devID = 1;

        pConf->chnID = 0;

        pConf->attr.samplerate = AUDIO_SAMPLE_RATE_8000;
        pConf->attr.bitwidth = AUDIO_BIT_WIDTH_16;
        pConf->attr.soundmode = AUDIO_SOUND_MODE_MONO;
        pConf->attr.frmNum = 40;
        pConf->attr.numPerFrm = 640; // it has to be multiple of (sample rate * 2 / 100)
        pConf->attr.chnCnt = 1;

        pConf->chnParam.usrFrmDepth = 40;

        pConf->chnVol = 60;

        pConf->aigain = 28;

        pConf->channelNumber = 1;

        pConf->sampleRate = 8000;
        pConf->channel = 1;
        pConf->bitRate = 128000;
    }

    return res;
}

#if USE_AUDIO_G711
static int16_t xSegmentAlawEnd[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};

uint8_t prvEncodePcmToAlaw(int16_t xPcmVal)
{
    uint8_t uMask = 0x55;
    size_t uSegIdx = 0;
    uint8_t uAlawVal = 0;

    xPcmVal = xPcmVal >> 3;

    if (xPcmVal >= 0)
    {
        uMask |= 0x80;
    }
    else
    {
        xPcmVal = -xPcmVal - 1;
    }

    for (uSegIdx = 0; uSegIdx<8; uSegIdx++)
    {
        if (xPcmVal <= xSegmentAlawEnd[uSegIdx])
        {
            break;
        }
    }

    if (uSegIdx >= 8)
    {
        uAlawVal = 0x7F ^ uMask;
    }
    else
    {
        uAlawVal = uSegIdx << 4;
        uAlawVal |= (uSegIdx < 2) ? ((xPcmVal >> 1) & 0x0F) : ((xPcmVal >> uSegIdx) & 0x0F);
        uAlawVal ^= uMask;
    }

    return uAlawVal;
}
#endif /* USE_AUDIO_G711 */

#if USE_AUDIO_AAC
int prvEncodePcmToAac(uint8_t *pPcmBuf, size_t uPcmBufSize, uint8_t *pFrameBuf, int *pxFramelen)
{
    int res = ERRNO_NONE;

    /* TODO: This function only copies silent AAC-LC audio into the frame buffer.
     * It requires the solution provider to use their AAC encoder and encode here.  */

    *pxFramelen = sizeof(pAacCh1Sr8K)/sizeof(pAacCh1Sr8K[0]);
    memcpy(pFrameBuf, pAacCh1Sr8K, *pxFramelen);

    return res;
}
#endif /* USE_AUDIO_AAC */

int sendAudioFrame(T31Audio_t *pAudio, IMPAudioFrame *pFrame)
{
    int res = ERRNO_NONE;
    int xFrameLen = 0;
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    int xFrameOffset = 0;
    size_t uCopySize = 0;
    uint64_t uCurrentTimestamp = 0;

    if (pAudio == NULL || pFrame == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
#if USE_AUDIO_G711
        if (pFrame->len <= 0 || (pData = (uint8_t *)malloc(pFrame->len / 2)) == NULL)
        {
            printf("%s(): OOM: pData\n", __FUNCTION__);
        }
        else
        {
            uDataLen = pFrame->len / 2;
            uint8_t *p = (uint8_t *)(pFrame->virAddr);
            int16_t *pPcmData = (int16_t *)(pFrame->virAddr);
            for (size_t i = 0; i < uDataLen; i++)
            {
                pData[i] = prvEncodePcmToAlaw(pPcmData[i]);
            }
            KvsApp_addFrame(pAudio->kvsAppHandle, pData, uDataLen, uDataLen, getEpochTimestampInMs(), TRACK_AUDIO);
        }
#endif /* USE_AUDIO_G711 */

#if USE_AUDIO_AAC
        uCurrentTimestamp = getEpochTimestampInMs();
        if (pAudio->uPcmOffset == 0)
        {
            pAudio->uPcmTimestamp = uCurrentTimestamp;
        }

        while (xFrameOffset < pFrame->len)
        {
            if (pAudio->uPcmBufSize - pAudio->uPcmOffset > pFrame->len - xFrameOffset)
            {
                /* Remaining data is not enough to fill the pcm buffer. */
                uCopySize = pFrame->len - xFrameOffset;
                memcpy(pAudio->pPcmBuf + pAudio->uPcmOffset, pFrame->virAddr + xFrameOffset, uCopySize);
                pAudio->uPcmOffset += uCopySize;
                xFrameOffset += uCopySize;
            }
            else
            {
                /* Remaining data is bigger than pcm buffer and able to do encode. */
                uCopySize = pAudio->uPcmBufSize - pAudio->uPcmOffset;
                memcpy(pAudio->pPcmBuf + pAudio->uPcmOffset, pFrame->virAddr + xFrameOffset, uCopySize);
                pAudio->uPcmOffset += uCopySize;
                xFrameOffset += uCopySize;

                xFrameLen = pAudio->xFrameBufSize;

                if (prvEncodePcmToAac(pAudio->pPcmBuf, pAudio->uPcmBufSize, pAudio->pFrameBuf, &xFrameLen) != 0)
                {
                    printf("%s(): aac encode failed\n", __FUNCTION__);
                }
                else
                {
                    uDataLen = xFrameLen;
                    if (uDataLen == 0 || (pData = (uint8_t *)malloc(uDataLen)) == NULL)
                    {
                        printf("%s(): OOM: pData\n", __FUNCTION__);
                    }
                    else
                    {
                        memcpy(pData, pAudio->pFrameBuf, uDataLen);
                        KvsApp_addFrame(pAudio->kvsAppHandle, pData, uDataLen, uDataLen, pAudio->uPcmTimestamp, TRACK_AUDIO);
                    }
                }

                pAudio->uPcmTimestamp = uCurrentTimestamp + (uCopySize * 1000) / (pAudio->xAudioConf.sampleRate * 2);
                pAudio->uPcmOffset = 0;
            }
        }
#endif /* USE_AUDIO_AAC */
    }

    return res;
}

static void *audioThread(void *arg)
{
    int res = ERRNO_NONE;
    T31Audio_t *pAudio = (T31Audio_t *)arg;
    IMPAudioFrame frm = {0};

    if (pAudio == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else if (
        IMP_AI_SetPubAttr(pAudio->xAudioConf.devID, &(pAudio->xAudioConf.attr)) != 0 || IMP_AI_Enable(pAudio->xAudioConf.devID) != 0 ||
        IMP_AI_SetChnParam(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, &(pAudio->xAudioConf.chnParam)) != 0 ||
        IMP_AI_EnableChn(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID) != 0 ||
        IMP_AI_SetVol(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, pAudio->xAudioConf.chnVol) != 0 ||
        IMP_AI_SetGain(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, pAudio->xAudioConf.aigain) != 0)
    {
        printf("%s(): Failed to setup audio\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        while (1)
        {
            if (IMP_AI_PollingFrame(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, 1000) != 0)
            {
                printf("%s(): Audio Polling Frame Data error\n", __FUNCTION__);
                continue;
            }
            else if (IMP_AI_GetFrame(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, &frm, BLOCK) != 0)
            {
                printf("%s(): Audio Get Frame Data error\n", __FUNCTION__);
                res = ERRNO_FAIL;
                break;
            }
            else
            {
                // Compress and send frame
                if (sendAudioFrame(pAudio, &frm) != 0)
                {
                    printf("%s(): Failed to send Audio frame\n", __FUNCTION__);
                }

                if (IMP_AI_ReleaseFrame(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID, &frm) != 0)
                {
                    printf("%s(): Audio release frame data error\n", __FUNCTION__);
                    break;
                }
            }

            if (pAudio->isTerminating)
            {
                break;
            }
        }

        if (IMP_AI_DisableChn(pAudio->xAudioConf.devID, pAudio->xAudioConf.chnID) != 0 || IMP_AI_Disable(pAudio->xAudioConf.devID) != 0)
        {
            printf("%s(): Audio device disable error\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
    }

    pAudio->isTerminated = true;

    return (void *)res;
}

static int initAudioTrackInfo(T31Audio_t *pAudio)
{
    int res = ERRNO_NONE;
    AudioTrackInfo_t *pAudioTrackInfo = NULL;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateDataLen = 0;

    if (pAudio == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        if (pAudio->pAudioTrackInfo == NULL)
        {
            if ((pAudioTrackInfo = (AudioTrackInfo_t *)malloc(sizeof(AudioTrackInfo_t))) == NULL)
            {
                printf("%s(): OOM: pAudioTrackInfo\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }
            else
            {
                memset(pAudioTrackInfo, 0, sizeof(AudioTrackInfo_t));
                pAudioTrackInfo->pTrackName = AUDIO_TRACK_NAME;
                pAudioTrackInfo->pCodecName = AUDIO_CODEC_NAME;
                pAudioTrackInfo->uFrequency = pAudio->xAudioConf.sampleRate;
                pAudioTrackInfo->uChannelNumber = pAudio->xAudioConf.channelNumber;

#if USE_AUDIO_G711
                if (Mkv_generatePcmCodecPrivateData(
                        AUDIO_PCM_OBJECT_TYPE, pAudioTrackInfo->uFrequency, pAudioTrackInfo->uChannelNumber, &pCodecPrivateData, &uCodecPrivateDataLen) != 0)
#endif
#if USE_AUDIO_AAC
                    if (Mkv_generateAacCodecPrivateData(
                            AUDIO_MPEG_OBJECT_TYPE, pAudioTrackInfo->uFrequency, pAudioTrackInfo->uChannelNumber, &pCodecPrivateData, &uCodecPrivateDataLen) != 0)
#endif /* USE_AUDIO_AAC */
                    {
                        printf("%s(): Failed to generate codec private data\n", __FUNCTION__);
                        res = ERRNO_FAIL;
                    }
                    else
                    {
                        pAudioTrackInfo->pCodecPrivate = pCodecPrivateData;
                        pAudioTrackInfo->uCodecPrivateLen = (uint32_t)uCodecPrivateDataLen;

                        pAudio->pAudioTrackInfo = pAudioTrackInfo;
                    }
            }
        }
    }

    if (res != ERRNO_NONE)
    {
        if (pAudioTrackInfo != NULL)
        {
            free(pAudioTrackInfo);
        }
    }

    return res;
}

#if USE_AUDIO_AAC
static int initAacEncoder(T31Audio_t *pAudio)
{
    int res = ERRNO_NONE;

    /**
     * TODO: In this function, it only initializes buffers for PCM and AAC. It needs to initialize the AAC encoder here.
     * This requires the solution provider to implement this.
     */

    if (pAudio == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        pAudio->xFrameBufSize = 8 * 1024;
        pAudio->uPcmTimestamp = 0;
        pAudio->uPcmOffset = 0;

        if ((pAudio->pPcmBuf = (uint8_t *)malloc(pAudio->uPcmBufSize)) == NULL)
        {
            printf("%s(): OOM: pPcmBuf\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else if ((pAudio->pFrameBuf = (uint8_t *)malloc(pAudio->xFrameBufSize)) == NULL)
        {
            printf("%s(): OOM: pFrameBuf\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else
        {

        }
    }

    // TODO: add error handling

    return res;
}
#endif /* USE_AUDIO_AAC */

T31AudioHandle T31Audio_create(KvsAppHandle kvsAppHandle)
{
    int res = ERRNO_NONE;
    T31Audio_t *pAudio = NULL;

    if ((pAudio = (T31Audio_t *)malloc(sizeof(T31Audio_t))) == NULL)
    {
        printf("%s(): OOM: pAudio\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pAudio, 0, sizeof(T31Audio_t));

        pAudio->isTerminating = false;
        pAudio->isTerminated = false;

        pAudio->kvsAppHandle = kvsAppHandle;

        if (pthread_mutex_init(&(pAudio->lock), NULL) != 0)
        {
            printf("%s(): Failed to initialize lock\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else if (audioConfigurationInit(&(pAudio->xAudioConf)) != ERRNO_NONE)
        {
            printf("%s(): failed to init audio configuration\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else if (initAudioTrackInfo(pAudio) != ERRNO_NONE)
        {
            printf("%s(): Failed to init audio track info\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
#if USE_AUDIO_AAC
        else if (initAacEncoder(pAudio) != ERRNO_NONE)
        {
            printf("%s(): Failed to init aac encoder\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
#endif /* USE_AUDIO_AAC */
        else if (pthread_create(&(pAudio->tid), NULL, audioThread, pAudio) != 0)
        {
            printf("%s(): Failed to create video thread\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
    }

    if (res != ERRNO_NONE)
    {
        T31Audio_terminate(pAudio);
        pAudio = NULL;
    }

    return pAudio;
}

void T31Audio_terminate(T31AudioHandle handle)
{
    T31Audio_t *pAudio = (T31Audio_t *)handle;

    if (pAudio != NULL)
    {
        pAudio->isTerminating = true;
        while (!pAudio->isTerminated)
        {
            prvSleepInMs(10);
        }

        pthread_join(pAudio->tid, NULL);

        pthread_mutex_destroy(&(pAudio->lock));
        free(pAudio);
    }
}

AudioTrackInfo_t *T31Audio_getAudioTrackInfoClone(T31AudioHandle handle)
{
    int res = ERRNO_NONE;
    AudioTrackInfo_t *pAudioTrackInfo = NULL;
    T31Audio_t *pAudio = (T31Audio_t *)handle;

    if (pAudio == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if (pthread_mutex_lock(&(pAudio->lock)) != 0)
    {
        printf("%s(): Failed to lock\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        if (pAudio->pAudioTrackInfo != NULL)
        {
            if ((pAudioTrackInfo = (AudioTrackInfo_t *)malloc(sizeof(AudioTrackInfo_t))) == NULL)
            {
                printf("%s(): OOM: pAudioTrackInfo\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }
            else
            {
                memcpy(pAudioTrackInfo, pAudio->pAudioTrackInfo, sizeof(AudioTrackInfo_t));
            }
        }

        pthread_mutex_unlock(&(pAudio->lock));
    }

    return pAudioTrackInfo;
}

#endif /* ENABLE_AUDIO_TRACK */
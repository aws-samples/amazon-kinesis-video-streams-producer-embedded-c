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

typedef struct AudioEncoder
{
    void *(*create)(const unsigned int uSampleRate, const unsigned int uChannels, const unsigned int uBitRate);
    void (*terminate)(void *pHandle);
    int (*setParameter)(void *pHandle, char *pKey, void *pValue);
    int (*getParameter)(void *pHandle, char *pKey, void *pValue);
    int (*encode)(void *pHandle, uint8_t *pPcmBuf, size_t uPcmBufLen, uint8_t *pEncBuf, size_t uEncBufSize, size_t *puEncBufLen, size_t *puPcmBufUsed, uint64_t *puTimestampMs);
} AudioEncoder_t;

#if USE_AUDIO_G711
#include "alaw_encoder.h"
static AudioEncoder_t xAudioEncoder = {
    .create = AlawEncoder_create,
    .terminate = AlawEncoder_terminate,
    .setParameter = AlawEncoder_setParameter,
    .getParameter = AlawEncoder_getParameter,
    .encode = AlawEncoder_encode
};
#endif /* USE_AUDIO_G711 */

#if USE_AUDIO_AAC
#include "aac_encoder.h"
static AudioEncoder_t xAudioEncoder = {
    .create = AacEncoder_create,
    .terminate = AacEncoder_terminate,
    .setParameter = AacEncoder_setParameter,
    .getParameter = AacEncoder_getParameter,
    .encode = AacEncoder_encode
};
#endif /* USE_AUDIO_AAC */

typedef struct AudioConfiguration
{
    /* Configuration for T31 audio driver */
    int devID;
    int chnID;
    IMPAudioIOAttr attr;
    IMPAudioIChnParam chnParam;
    int chnVol;
    int aigain;

    /* Configuration for Audio encoder */
    void *pEncoderHandle;
    uint8_t *pPcmBuf;
    size_t uPcmBufSize;
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
} T31Audio_t;

static void prvSleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static unsigned int prvMapSampleRate(IMPAudioSampleRate sr)
{
    unsigned int uSampleRate = 0;

    switch(sr)
    {
        case AUDIO_SAMPLE_RATE_8000: uSampleRate = 8000; break;
        case AUDIO_SAMPLE_RATE_16000: uSampleRate = 16000; break;
        case AUDIO_SAMPLE_RATE_24000: uSampleRate = 24000; break;
        case AUDIO_SAMPLE_RATE_32000: uSampleRate = 32000; break;
        case AUDIO_SAMPLE_RATE_44100: uSampleRate = 44100; break;
        case AUDIO_SAMPLE_RATE_48000: uSampleRate = 48000; break;
        case AUDIO_SAMPLE_RATE_96000: uSampleRate = 96000; break;
    }

    return uSampleRate;
}

static unsigned int prvMapChannelNumber(IMPAudioSoundMode sm)
{
    unsigned int uChannels = 0;

    switch(sm)
    {
        case AUDIO_SOUND_MODE_MONO:
            uChannels = 1;
            break;
        case AUDIO_SOUND_MODE_STEREO:
            uChannels = 2;
            break;
    }

    return uChannels;
}

static unsigned int prvMapBitWidth(IMPAudioBitWidth bw)
{
    unsigned int uBitWidth = 0;

    switch (bw)
    {
        case AUDIO_BIT_WIDTH_16:
            uBitWidth = 16;
            break;
    }

    return uBitWidth;
}

static int audioConfigurationInit(AudioConfiguration_t *pConf)
{
    int res = ERRNO_NONE;
    unsigned int uSampleRate = 0;
    unsigned int uChannels = 0;
    unsigned int uBitRate = 0;

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

        uSampleRate = prvMapSampleRate(pConf->attr.samplerate);
        uChannels = prvMapChannelNumber(pConf->attr.soundmode);
        uBitRate = uSampleRate * uChannels * prvMapBitWidth(pConf->attr.bitwidth);

        if ((pConf->pEncoderHandle = xAudioEncoder.create(uSampleRate, uChannels, uBitRate)) == NULL)
        {
            res = ERRNO_FAIL;
        }

        pConf->pPcmBuf = NULL;
        pConf->uPcmBufSize = 0;
    }

    return res;
}

static void audioConfigurationDeinit(AudioConfiguration_t *pConf)
{
    if (pConf != NULL)
    {
        xAudioEncoder.terminate(pConf->pEncoderHandle);
        if (pConf->pPcmBuf != NULL)
        {
            free(pConf->pPcmBuf);
        }
    }
}

static int checkAndInitBuf(uint8_t **ppBuf, size_t *puBufSize, size_t uNeededSize)
{
    int res = ERRNO_NONE;

    if (*ppBuf == NULL)
    {
        if ((*ppBuf = (uint8_t *)malloc(uNeededSize)) == NULL)
        {
            printf("%s(): OOM: ppBuf\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else
        {
            *puBufSize = uNeededSize;
        }
    }
    else
    {
        if (*puBufSize < uNeededSize)
        {
            if ((*ppBuf = (uint8_t *)realloc(*ppBuf, uNeededSize)) == NULL)
            {
                printf("%s(): OOM: ppBuf\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }
            else
            {
                *puBufSize = uNeededSize;
            }
        }
    }

    return res;
}

int sendAudioFrame(T31Audio_t *pAudio, IMPAudioFrame *pFrame)
{
    int res = ERRNO_NONE;
    AudioConfiguration_t *pConf = NULL;
    uint8_t *pEncBuf = NULL;
    size_t uEncBufSize = 0;
    size_t uEncBufLen = 0;
    size_t uPcmBufUsed = 0;
    uint8_t *pPcmSrc = NULL;
    size_t uPcmLen = 0;
    uint64_t uTimestampMs = 0;

    if (pAudio == NULL || pFrame == NULL || pFrame->virAddr == NULL || pFrame->len <= 0)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        pPcmSrc = (uint8_t *)(pFrame->virAddr);
        uPcmLen = pFrame->len;
        pConf = &(pAudio->xAudioConf);
        while (uPcmLen > 0 && res == ERRNO_NONE)
        {
            if (xAudioEncoder.encode(pConf->pEncoderHandle, pPcmSrc, uPcmLen, NULL, 0, &uEncBufLen, &uPcmBufUsed, &uTimestampMs) != 0)
            {
                printf("%s(): Failed to get encode buffer length\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }
            else if (uEncBufLen == 0)
            {
                /* Do nothing here because the PCM data is not enough to do the encoding. The audio encoder is responsible to buffer these PCM data until next time. */
                break;
            }
            else if (checkAndInitBuf(&(pConf->pPcmBuf), &(pConf->uPcmBufSize), uPcmLen) != 0 ||
                     checkAndInitBuf(&pEncBuf, &uEncBufSize, uEncBufLen) != 0)
            {
                printf("%s(): Failed to init pcm and enc buf\n", __FUNCTION__ );
                res = ERRNO_FAIL;
            }
            else
            {
                memcpy(pConf->pPcmBuf, pPcmSrc, uPcmLen);
                if (xAudioEncoder.encode(pConf->pEncoderHandle, pConf->pPcmBuf, uPcmLen, pEncBuf, uEncBufSize, &uEncBufLen, &uPcmBufUsed, &uTimestampMs) != 0)
                {
                    printf("%s(): Failed to encode\n", __FUNCTION__);
                    res = ERRNO_FAIL;
                }
                else
                {
                    KvsApp_addFrame(pAudio->kvsAppHandle, pEncBuf, uEncBufSize, uEncBufLen, uTimestampMs, TRACK_AUDIO);
                    uPcmLen -= uPcmBufUsed;
                    pPcmSrc += uPcmBufUsed;
                }
            }
        }
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

        audioConfigurationDeinit(&(pAudio->xAudioConf));
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
                pAudioTrackInfo->uFrequency = prvMapSampleRate(pAudio->xAudioConf.attr.samplerate);
                pAudioTrackInfo->uChannelNumber = prvMapChannelNumber(pAudio->xAudioConf.attr.soundmode);

#if USE_AUDIO_G711
                if (Mkv_generatePcmCodecPrivateData(AUDIO_PCM_OBJECT_TYPE, pAudioTrackInfo->uFrequency, pAudioTrackInfo->uChannelNumber, &pCodecPrivateData, &uCodecPrivateDataLen) != 0)
#endif
#if USE_AUDIO_AAC
                if (Mkv_generateAacCodecPrivateData(AUDIO_MPEG_OBJECT_TYPE, pAudioTrackInfo->uFrequency, pAudioTrackInfo->uChannelNumber, &pCodecPrivateData, &uCodecPrivateDataLen) != 0)
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
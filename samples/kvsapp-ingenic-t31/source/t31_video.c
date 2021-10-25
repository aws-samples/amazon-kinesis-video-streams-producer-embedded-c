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
#include <imp/imp_encoder.h>
#include <imp/imp_system.h>

#include "sample-common.h"

/* KVS headers */
#include "kvs/kvsapp.h"
#include "kvs/port.h"

#include "sample_config.h"
#include "t31_video.h"

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

typedef struct T31Video
{
    pthread_mutex_t lock;

    pthread_t tid;
    bool isTerminating;
    bool isTerminated;

    KvsAppHandle kvsAppHandle;
} T31Video_t;

extern struct chn_conf chn[];

static void prvSleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}

static int getPacket(IMPEncoderStream *pStream, IMPEncoderPack *pPack, uint8_t *pPacketBuf, size_t uPacketSize)
{
    int res = ERRNO_NONE;

    if (pStream == NULL || pPack == NULL || pPacketBuf == NULL || uPacketSize == 0)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else if (pPack->length == 0)
    {
        printf("%s(): Invalid packet length\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        /*  virAddr is a ringbuffer, and the packet may be cut into 2 pieces. */
        uint32_t uRemainingSize = pStream->streamSize - pPack->offset;

        if (uRemainingSize < pPack->length)
        {
            /* The packet is cut into 2 pieces. */
            memcpy(pPacketBuf, (uint8_t *)(pStream->virAddr + pPack->offset), uRemainingSize);
            memcpy(pPacketBuf + uRemainingSize, (uint8_t *)(pStream->virAddr), pPack->length - uRemainingSize);
        }
        else
        {
            /* The packet is a complete one. */
            memcpy(pPacketBuf, (uint8_t *)(pStream->virAddr + pPack->offset), pPack->length);
        }
    }

    return res;
}

static int sendVideoFrame(T31Video_t *pVideo, IMPEncoderStream *pStream)
{
    int res = ERRNO_NONE;
    IMPEncoderPack *pPack = NULL;
    uint8_t *pPacketBuf = NULL;
    size_t uPacketLen = 0;

    if (pStream == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        for (int i = 0; i < pStream->packCount; i++)
        {
            pPack = &pStream->pack[i];
            uPacketLen += pPack->length;
        }

        if ((pPacketBuf = (uint8_t *)malloc(uPacketLen)) == NULL)
        {
            printf("%s(): OOM: pPacketBuf, size:%zu\n", __FUNCTION__, uPacketLen);
            res = ERRNO_FAIL;
        }
        else
        {
            size_t offset = 0;
            for (int i = 0; i < pStream->packCount; i++)
            {
                pPack = &pStream->pack[i];
                getPacket(pStream, pPack, pPacketBuf + offset, uPacketLen - offset);
                offset += pPack->length;
            }
            KvsApp_addFrame(pVideo->kvsAppHandle, pPacketBuf, uPacketLen, uPacketLen, getEpochTimestampInMs(), TRACK_VIDEO);
        }
    }

    return res;
}

static int doVideoStreaming(int chnNum, T31Video_t *pVideo)
{
    int res = ERRNO_NONE;
    IMPEncoderStream stream = {0};

    if (IMP_Encoder_StartRecvPic(chnNum) < 0)
    {
        printf("%s(): IMP_Encoder_StartRecvPic(%d) failed\n", __FUNCTION__, chnNum);
        res = ERRNO_FAIL;
    }
    else
    {
        while (1)
        {
            if (pVideo->isTerminating)
            {
                break;
            }
            else if (IMP_Encoder_PollingStream(chnNum, 1000) < 0)
            {
                printf("%s(): IMP_Encoder_PollingStream(%d) timeout\n", __FUNCTION__, chnNum);
                continue;
            }
            else if (IMP_Encoder_GetStream(chnNum, &stream, 1) < 0)
            {
                printf("%s(): IMP_Encoder_GetStream(%d) failed\n", __FUNCTION__, chnNum);
                res = ERRNO_FAIL;
                break;
            }
            else
            {
                if (sendVideoFrame(pVideo, &stream) != 0)
                {
                    printf("%s(): Failed to send video frame\n", __FUNCTION__);
                }
                IMP_Encoder_ReleaseStream(chnNum, &stream);
            }
        }

        if (IMP_Encoder_StopRecvPic(chnNum) < 0)
        {
            printf("%s(): IMP_Encoder_StopRecvPic(%d) failed\n", __FUNCTION__, chnNum);
            res = ERRNO_FAIL;
        }
    }

    return res;
}

static void *videoThread(void *arg)
{
    int res = ERRNO_NONE;
    int i = 0;
    int selectedChannel = -1;
    T31Video_t *pVideo = (T31Video_t *)arg;

    chn[0].payloadType = IMP_ENC_PROFILE_AVC_MAIN;

    if (pVideo == NULL)
    {
        printf("%s(): Invalid parameter\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else if (sample_system_init() < 0 || sample_framesource_init() < 0)
    {
        printf("%s(): IMP_System_Init failed\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        /* Step.3 Encoder init */
        for (i = 0; i < FS_CHN_NUM; i++)
        {
            if (chn[i].enable)
            {
                if (IMP_Encoder_CreateGroup(chn[i].index) < 0)
                {
                    printf("%s(): IMP_Encoder_CreateGroup(%d) error !\n", __FUNCTION__, chn[i].index);
                    res = ERRNO_FAIL;
                }
                else
                {
                    selectedChannel = i;
                }
                break;
            }
        }

        if (res != ERRNO_NONE)
        {
            printf("%s(): Encoder init failed\n", __FUNCTION__);
        }
        else if (sample_encoder_init() < 0 || IMP_System_Bind(&chn[selectedChannel].framesource_chn, &chn[selectedChannel].imp_encoder) < 0 || sample_framesource_streamon() < 0)
        {
            printf("%s(): Encoder init failed\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else
        {
            if (doVideoStreaming(selectedChannel, pVideo) != ERRNO_NONE)
            {
                printf("%s(): ImpStreamOn failed\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }

            if (sample_framesource_streamoff() < 0 || IMP_System_UnBind(&chn[i].framesource_chn, &chn[i].imp_encoder) < 0 || sample_encoder_exit() < 0 ||
                sample_framesource_exit() < 0 || sample_system_exit() < 0)
            {
                printf("%s(): Failed to release video resource\n", __FUNCTION__);
                res = ERRNO_FAIL;
            }
        }
    }

    if (pVideo != NULL && pVideo->isTerminating)
    {
        pVideo->isTerminated = true;
    }

    return (void *)(intptr_t)res;
}

T31VideoHandle T31Video_create(KvsAppHandle kvsAppHandle)
{
    int res = ERRNO_NONE;
    T31Video_t *pVideo = NULL;

    if ((pVideo = (T31Video_t *)malloc(sizeof(T31Video_t))) == NULL)
    {
        printf("%s(): OOM: pVideo\n", __FUNCTION__);
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pVideo, 0, sizeof(T31Video_t));

        pVideo->isTerminating = false;
        pVideo->isTerminated = false;

        pVideo->kvsAppHandle = kvsAppHandle;

        if (pthread_mutex_init(&(pVideo->lock), NULL) != 0)
        {
            printf("%s(): Failed to initialize lock\n", __FUNCTION__);
            res = ERRNO_FAIL;
        }
        else
        {
            if (pthread_create(&(pVideo->tid), NULL, videoThread, pVideo) != 0)
            {
                printf("%s(): Failed to create video thread\n", __FUNCTION__);
                T31Video_terminate(pVideo);
                pVideo = NULL;
            }
        }
    }
    return pVideo;
}

void T31Video_terminate(T31VideoHandle handle)
{
    T31Video_t *pVideo = (T31Video_t *)handle;

    if (pVideo != NULL)
    {
        pVideo->isTerminating = true;
        while (!pVideo->isTerminated)
        {
            prvSleepInMs(10);
        }

        pthread_join(pVideo->tid, NULL);

        pthread_mutex_destroy(&(pVideo->lock));
        free(pVideo);
    }
}

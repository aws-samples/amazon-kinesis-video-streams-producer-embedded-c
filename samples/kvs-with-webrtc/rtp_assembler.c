#include "rtp_assembler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#define ERRNO_NONE 0
#define ERRNO_FAIL __LINE__

#define MIN_RTP_HEADER_LENGTH 12

#define MAX_RTP_TRACK_SIZE 2
#define RTP_FRAME_BUFFER_INIT_SIZE 1024

/* Refer these clockrate in PeerConnection/SessionDescription.h */
#define CLOCKRATE_MULAW     ((uint64_t)8000)
#define CLOCKRATE_ALAW      ((uint64_t)8000)
#define CLOCKRATE_OPUS      ((uint64_t)48000)
#define CLOCKRATE_VP8       ((uint64_t)90000)
#define CLOCKRATE_H264      ((uint64_t)90000)

#define RTP_NAL_HDR_SIZE                (4)

#define RTP_NAL_TYPE_SINGLE_MIN         (1)
#define RTP_NAL_TYPE_SINGLE_MAX         (23)
#define RTP_NAL_TYPE_STAP_A             (24)
#define RTP_NAL_TYPE_FU_A               (28)

typedef struct RtpTrack
{
    uint8_t *pFrame;
    size_t uFrameLen;
    size_t uBufSize;

    bool isFrameComplete;

    uint8_t uPayloadType;
    uint32_t uTimestamp;
} RtpTrack_t;

typedef struct RtpAssembler
{
    RtpTrack_t tracks[MAX_RTP_TRACK_SIZE];
    size_t uTrackCount;
} RtpAssembler_t;

static uint16_t get2BytesBigEndian(const uint8_t *pData)
{
    uint16_t val = pData[0];
    val = (val << 8) | pData[1];
    return val;
}

static uint32_t get4BytesBigEndian(const uint8_t *pData)
{
    uint32_t val = pData[0];
    val = (val << 8) | pData[1];
    val = (val << 8) | pData[2];
    val = (val << 8) | pData[3];
    return val;
}

static int parseRtpPacket(uint8_t *pPkt, size_t uLen, uint8_t *puMarker, uint8_t *puPayloadType, uint16_t *puSequenceNumber, uint32_t *puTimestamp, uint32_t *puSsrc, uint8_t **ppPayload, size_t *puPayloadLen)
{
    int res = ERRNO_NONE;
    uint8_t *pCur = pPkt;
    uint8_t uVersion = 0;
    uint8_t uPadding = 0;
    uint8_t uExtension = 0;
    uint8_t uCsrcCount = 0;
    uint8_t uMarker = 0;
    uint8_t uPayloadType = 0;
    uint16_t uSequenceNumber = 0;
    uint32_t uTimestamp = 0;
    uint32_t uSsrc = 0;
    uint16_t uExtensionLength = 0;
    uint8_t *pPayload = NULL;
    size_t uPayloadLen = 0;

    if (pPkt == NULL || uLen < MIN_RTP_HEADER_LENGTH || puMarker == NULL || puPayloadType == NULL || puSequenceNumber == NULL || puTimestamp == NULL || puSsrc == NULL || ppPayload == NULL || puPayloadLen == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        uVersion = ((*pCur) >> 6) & 0x03;
        uPadding = ((*pCur) >> 5) & 0x01;
        uExtension = ((*pCur) >> 4) & 0x01;
        uCsrcCount = (*pCur) & 0x0F;
        pCur++;

        uMarker = ((*pCur) >> 7) & 0x01;
        uPayloadType = (*pCur) & 0x7F;
        pCur++;

        uSequenceNumber = get2BytesBigEndian(pCur);
        pCur += 2;

        uTimestamp = get4BytesBigEndian(pCur);
        pCur += 4;

        uSsrc = get4BytesBigEndian(pCur);
        pCur += 4;

        /* Discard CSRC */
        pCur += uCsrcCount * 4;

        if (uLen < (pCur - pPkt))
        {
            res = ERRNO_FAIL;
        }
        else
        {
            if (uExtension)
            {
                /* Discard extension profile */
                pCur += 2;

                uExtensionLength = get2BytesBigEndian(pCur);
                pCur += 2;

                /* Discard extension payload */
                pCur += uExtensionLength;
            }

            pPayload = pCur;
            uPayloadLen = uLen - (pCur - pPkt);

            /* Set output parameters */
            *puMarker = uMarker;
            *puPayloadType = uPayloadType;
            *puSequenceNumber = uSequenceNumber;
            *puTimestamp = uTimestamp;
            *puSsrc = uSsrc;
            *ppPayload = pPayload;
            *puPayloadLen = uPayloadLen;

#if 0
            printf("RTP(%zu): ", uLen);
            for (size_t i=0; i<uLen; i++)
            {
                printf("%02X ", pPkt[i]);
            }
            printf("\n");
            printf("M:%" PRIu8 " PT:%" PRIu8 " HdrLen:%ld\n",
                   uMarker, uPayloadType, pPayload - pPkt);
#endif
        }
    }

    return res;
}

static int initRtpTrack(RtpTrack_t *pTrack, uint8_t uPayloadType)
{
    int res = ERRNO_NONE;

    if (pTrack == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pTrack, 0, sizeof(RtpTrack_t));

        if ((pTrack->pFrame = (uint8_t *)malloc(RTP_FRAME_BUFFER_INIT_SIZE)) == NULL)
        {
            printf("OOM: RTP pFrame\n");
            res = ERRNO_FAIL;
        }
        else
        {
            pTrack->uFrameLen = 0;
            pTrack->uBufSize = RTP_FRAME_BUFFER_INIT_SIZE;
            pTrack->isFrameComplete = false;

            pTrack->uPayloadType = uPayloadType;
            pTrack->uTimestamp = 0;
        }
    }

    return res;
}

static uint64_t getClockRate(uint8_t uPayloadType)
{
    uint64_t uClockRate = 1000;

    if (uPayloadType == RTP_PAYLOAD_TYPE_H264)
    {
        return CLOCKRATE_H264;
    }
    else if (uPayloadType == RTP_PAYLOAD_TYPE_MULAW)
    {
        return CLOCKRATE_MULAW;
    }
    else if (uPayloadType == RTP_PAYLOAD_TYPE_ALAW)
    {
        return CLOCKRATE_ALAW;
    }
    else if (uPayloadType == RTP_PAYLOAD_TYPE_OPUS)
    {
        return CLOCKRATE_OPUS;
    }
    else if (uPayloadType == RTP_PAYLOAD_TYPE_VP8)
    {
        return CLOCKRATE_VP8;
    }

    return uClockRate;
}

static int pushRtpIntoH264Track(RtpTrack_t *pTrack, uint8_t *pPayload, size_t uPayloadLen)
{
    int res = ERRNO_NONE;

    uint8_t *pBuf = pTrack->pFrame + pTrack->uFrameLen;
    uint8_t uNalNRI = 0;
    uint8_t uNalType = 0;

    uint8_t uH264StartBit = 0;
    uint8_t uH264EndBit = 0;
    uint8_t uH264Type = 0;
    uint8_t uH264Key = 0;

    uNalNRI = (pPayload[0] & 0x60) >> 5;
    uNalType = pPayload[0] & 0x1F;

    uH264StartBit = (pPayload[1] & 0x80) >> 7;
    uH264EndBit = (pPayload[1] & 0x40) >> 6;
    uH264Type = pPayload[1] & 0x1F;
    uH264Key = (uNalNRI << 5) | uH264Type;

    if (uNalType >= RTP_NAL_TYPE_SINGLE_MIN && uNalType <= RTP_NAL_TYPE_SINGLE_MAX)
    {
        pBuf[0] = 0x00;
        pBuf[1] = 0x00;
        pBuf[2] = 0x00;
        pBuf[3] = 0x01;
        memcpy(pBuf + 4, pPayload, uPayloadLen);
        pTrack->uFrameLen += uPayloadLen + 4;
    }
    else if (uNalType == RTP_NAL_TYPE_FU_A)
    {
        if (uH264StartBit)
        {
            pBuf[0] = 0x00;
            pBuf[1] = 0x00;
            pBuf[2] = 0x00;
            pBuf[3] = 0x01;
            pBuf[4] = uH264Key;
            memcpy(pBuf + 5, pPayload + 2, uPayloadLen - 2);
            pTrack->uFrameLen += uPayloadLen - 2 + 5;
        }
        else
        {
            memcpy(pBuf, pPayload + 2, uPayloadLen - 2);
            pTrack->uFrameLen += uPayloadLen - 2;
        }
    }
    else
    {
        res = ERRNO_FAIL;
    }

    return res;
}

static int pushRtpIntoTrack(RtpTrack_t *pTrack, uint8_t *pPayload, size_t uPayloadLen)
{
    int res = ERRNO_NONE;

    if (pTrack->uPayloadType == RTP_PAYLOAD_TYPE_H264)
    {
        res = pushRtpIntoH264Track(pTrack, pPayload, uPayloadLen);
    }

    return res;
}

RtpAssemblerHandle RtpAssembler_create()
{
    RtpAssembler_t *pRtpAssembler = NULL;

    if ((pRtpAssembler = (RtpAssembler_t *)malloc(sizeof(RtpAssembler_t))) == NULL)
    {
        printf("OOM: pRtpAssembler\n");
    }
    else
    {
        memset(pRtpAssembler, 0, sizeof(RtpAssembler_t));

        pRtpAssembler->uTrackCount = 0;
    }

    return pRtpAssembler;
}

void RtpAssembler_terminate(RtpAssemblerHandle handle)
{
    RtpAssembler_t *pRtpAssembler = (RtpAssembler_t *)handle;

    if (pRtpAssembler != NULL)
    {
        for (size_t i=0; i<pRtpAssembler->uTrackCount; i++)
        {
            if (pRtpAssembler->tracks[i].pFrame != NULL)
            {
                free(pRtpAssembler->tracks[i].pFrame);
            }
        }
        free(pRtpAssembler);
    }
}

int RtpAssembler_pushRtpPacket(RtpAssemblerHandle handle, uint8_t *pRtpPacket, size_t uRtpPacketLen)
{
    int res = ERRNO_NONE;
    RtpAssembler_t *pRtpAssembler = (RtpAssembler_t *)handle;
    uint8_t uMarker = 0;
    uint8_t uPayloadType = 0;
    uint16_t uSequenceNumber = 0;
    uint32_t uTimestamp = 0;
    uint32_t uSsrc = 0;
    uint8_t *pPayload = NULL;
    size_t uPayloadLen = 0;
    RtpTrack_t *pTrack = NULL;

    if (pRtpAssembler == NULL || pRtpPacket == NULL || uRtpPacketLen == 0)
    {
        res = ERRNO_FAIL;
    }
    else if (parseRtpPacket(pRtpPacket, uRtpPacketLen, &uMarker, &uPayloadType, &uSequenceNumber, &uTimestamp, &uSsrc, &pPayload, &uPayloadLen) != 0)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        /* find the tracks that this RTP packet belongs */
        for (size_t i=0; i<pRtpAssembler->uTrackCount; i++)
        {
            if (pRtpAssembler->tracks[i].uPayloadType == uPayloadType)
            {
                pTrack = &(pRtpAssembler->tracks[i]);
                break;
            }
        }

        /* Create a new track if this RTP packet does not belong to any track. */
        if (pTrack == NULL)
        {
            if (pRtpAssembler->uTrackCount == MAX_RTP_TRACK_SIZE)
            {
                printf("Exceed max RTP track size\n");
                res = ERRNO_FAIL;
            }
            else
            {
                pTrack = &(pRtpAssembler->tracks[pRtpAssembler->uTrackCount]);
                pRtpAssembler->uTrackCount++;

                if (initRtpTrack(pTrack, uPayloadType) != ERRNO_NONE)
                {
                    printf("Failed to init RTP track\n");
                    res = ERRNO_FAIL;
                }
            }
        }

        /* Check if buffer size is enough. */
        if (res == ERRNO_NONE)
        {
            if (pTrack->uFrameLen + uPayloadLen + RTP_NAL_HDR_SIZE > pTrack->uBufSize)
            {
                size_t uNewBufSize = pTrack->uBufSize;
                while (uNewBufSize < pTrack->uFrameLen + uPayloadLen + RTP_NAL_HDR_SIZE)
                {
                    uNewBufSize *= 2;
                }

                uint8_t *pNewBuf = realloc(pTrack->pFrame, uNewBufSize);
                if (pNewBuf == NULL)
                {
                    printf("Failed to increase RTP frame buffer\n");
                    res = ERRNO_FAIL;
                }
                else
                {
                    pTrack->pFrame = pNewBuf;
                    pTrack->uBufSize = uNewBufSize;
                }
            }
        }

        /* Push payload into track */
        if (res == ERRNO_NONE)
        {
            pushRtpIntoTrack(pTrack, pPayload, uPayloadLen);
            if (pTrack->uTimestamp == 0)
            {
                pTrack->uTimestamp = uTimestamp;
            }
            if (uMarker)
            {
                pTrack->isFrameComplete = true;
            }
        }
    }

    return res;
}

bool RtpAssembler_isFrameAvailable(RtpAssemblerHandle handle)
{
    bool isFrameAvailable = false;
    RtpAssembler_t *pRtpAssembler = (RtpAssembler_t *)handle;

    if (pRtpAssembler != NULL)
    {
        for (size_t i=0; i<pRtpAssembler->uTrackCount; i++)
        {
            if (pRtpAssembler->tracks[i].isFrameComplete)
            {
                isFrameAvailable = true;
                break;
            }
        }
    }

    return isFrameAvailable;
}

int RtpAssembler_getFrame(RtpAssemblerHandle handle, uint8_t **ppFrame, size_t *puFrameLen, uint8_t *puPayloadType, uint64_t *puTimestampMs)
{
    int res = ERRNO_NONE;
    RtpAssembler_t *pRtpAssembler = (RtpAssembler_t *)handle;
    RtpTrack_t *pRtpTrack = NULL;
    uint8_t *pFrame = NULL;
    size_t uFrameLen = 0;
    uint8_t uPayloadType = 0;
    uint64_t uTimestampMs = 0;

    if (pRtpAssembler == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        /* Find witch track has available frame. */
        for (size_t i=0; i<pRtpAssembler->uTrackCount; i++)
        {
            if (pRtpAssembler->tracks[i].isFrameComplete)
            {
                pRtpTrack = &(pRtpAssembler->tracks[i]);
                break;
            }
        }

        if (pRtpTrack == NULL)
        {
            res = ERRNO_FAIL;
        }
        else if ((pFrame = (uint8_t *)malloc(pRtpTrack->uFrameLen)) == NULL)
        {
            printf("OOM: RTP pFrame\n");
            res = ERRNO_FAIL;
        }
        else
        {
            /* Get the frame from track */
            uFrameLen = pRtpTrack->uFrameLen;
            memcpy(pFrame, pRtpTrack->pFrame, uFrameLen);
            uPayloadType = pRtpTrack->uPayloadType;
            uTimestampMs = pRtpTrack->uTimestamp;

            /* Convert RTP timestamp to milliseconds. */
            uTimestampMs = uTimestampMs * 1000 / getClockRate(uPayloadType);

            /* Reset track status */
            pRtpTrack->uFrameLen = 0;
            pRtpTrack->uTimestamp = 0;
            pRtpTrack->isFrameComplete = false;

            /* Set results to output parameters */
            *ppFrame = pFrame;
            *puFrameLen = uFrameLen;
            *puPayloadType = uPayloadType;
            *puTimestampMs = uTimestampMs;
        }
    }

    return res;
}

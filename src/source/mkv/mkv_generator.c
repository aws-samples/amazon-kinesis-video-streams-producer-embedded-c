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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Third party headers */
#include "azure_c_shared_utility/xlogging.h"

/* Public headers */
#include "kvs/errors.h"
#include "kvs/mkv_generator.h"
#include "kvs/nalu.h"
#include "kvs/port.h"

/* Internal headers */
#include "os/allocator.h"
#include "os/endian.h"

#define MKV_LENGTH_INDICATOR_1_BYTE (0x80)
#define MKV_LENGTH_INDICATOR_2_BYTE (0x4000)
#define MKV_LENGTH_INDICATOR_3_BYTE (0x200000)
#define MKV_LENGTH_INDICATOR_4_BYTE (0x10000000)

/* The offset of UUID value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_UID_OFFSET (9)

/* The offset of Title value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_TITLE_OFFSET (40)

/* The offset of MuxingApp value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_MUXING_APP_OFFSET (59)

/* The offset of WritingApp value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_WRITING_APP_OFFSET (78)

/* The offset of length field in gSegmentTrackHeader */
#define MKV_SEGMENT_TRACK_LENGTH_OFFSET (4)

/* The size of type and length in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE (5)

/* The offset of length field in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET (1)

/* The offset of track number in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET (7)

/* The offset of track UID in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET (11)

/* The offset of track type in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET (21)

/* The offset of track name in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET (25)

/* The offset of track codec in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET (1)

/* The offset of width field in gSegmentTrackEntryVideoHeader */
#define MKV_SEGMENT_TRACK_ENTRY_VIDEO_WIDTH_OFFSET (7)

/* The offset of height field in gSegmentTrackEntryVideoHeader */
#define MKV_SEGMENT_TRACK_ENTRY_VIDEO_HEIGHT_OFFSET (11)

/* The size of type and length in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_HEADER_SIZE (5)

/* The offset of track codec in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_LEN_OFFSET (1)

/* The offset of frequency in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_FREQUENCY_OFFSET (7)

/* The offset of channel number in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_CHANNEL_NUMBER_OFFSET (17)

/* The offset of value field in gSegmentTrackEntryAudioHeaderBitsPerSample */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_BPS_OFFSET (2)

/* The offset of length field in gSegmentTrackEntryCodecPrivateHeader */
#define MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET (2)

/* The size of simple block header */
#define SIMPLE_BLOCK_HEADER_SIZE (4)

/* The offset of frame size offset in gClusterSimpleBlock.  This field is 8 bytes long, but we only use least 4 bytes. */
#define MKV_CLUSTER_SIMPLE_BLOCK_FRAME_SIZE_OFFSET (5)

/* The offset of track number in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_TRACK_NUMBER_OFFSET (9)

/* The offset of delta timestamp in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_DELTA_TIMESTAMP_OFFSET (10)

/* The offset of property in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_PROPERTY_OFFSET (12)

/* In H264 extended profile, the size except sps and pps. */
#define MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE (11)

/* It's a pre-defined MKV header of EBML document. EBML is used for the first frame in a streaming. There is no
 * configurable field in this header. */
static uint8_t gEbmlHeader[] = {
    0x1A, 0x45, 0xDF, 0xA3, // EBML Header
    0xA3,                   // len = 35

    0x42, 0x86, // EBML Version
    0x81,       // len = 1
    0x01,       // EBMLVersion = 0x01

    0x42, 0xF7, // EBML Read Version
    0x81,       // len = 1
    0x01,       // EBMLReadVersion = 0x01

    0x42, 0xF2, // EBML Max ID Length
    0x81,       // len = 1
    0x04,       // EBMLMaxIDLength = 0x04

    0x42, 0xF3, // EBML Max Size Length
    0x81,       // len = 1
    0x08,       // EBMLMaxSizeLength = 0x08

    0x42, 0x82,                                     // Doc Type
    0x88,                                           // len = 8
    0x6D, 0x61, 0x74, 0x72, 0x6F, 0x73, 0x6B, 0x61, // DocType = "matroska"

    0x42, 0x87, // Doc Type Version
    0x81,       // len = 1
    0x02,       // DocTypeVersion = 0x02

    0x42, 0x85, // Doc Type Read Version
    0x81,       // len = 1
    0x02        // DocTypeReadVersion = 0x02
};
static const uint32_t gEbmlHeaderSize = sizeof(gEbmlHeader);

static uint8_t gSegmentHeader[] = {
    0x18,
    0x53,
    0x80,
    0x67, // Segment (L0)
    0xFF, // len = -1 for unknown
};
static const uint32_t gSegmentHeaderSize = sizeof(gSegmentHeader);

static uint8_t gSegmentInfoHeader[] = {
    0x15, 0x49, 0xA9, 0x66, // Info (L1)
    0x40, 0x58,             // len = 88

    0x73, 0xA4,                                                                                     // SegmentUID (L2)
    0x90,                                                                                           // len = 16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // SegmentUID

    0x2A, 0xD7, 0xB1,                               // TimestampScale (L2)
    0x88,                                           // len = 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x42, 0x40, // TimestampScale = 1000000ns = 1ms

    0x7B, 0xA9, // Title (L2)
    0x90,       // len = 16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x4D, 0x80, // MuxingApp (L2)
    0x90,       // len = 16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x57, 0x41, // WritingApp (L2)
    0x90,       // len = 16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint32_t gSegmentInfoHeaderSize = sizeof(gSegmentInfoHeader);

static uint8_t gSegmentTrackHeader[] = {
    0x16,
    0x54,
    0xAE,
    0x6B, // Tracks (L1)
    0x10,
    0x00,
    0x00,
    0x00 // len - will be fixed up
};
static const uint32_t gSegmentTrackHeaderSize = sizeof(gSegmentTrackHeader);

static uint8_t gSegmentTrackEntryHeader[] = {
    0xAE,                   // TrackEntry (L2)
    0x10, 0x00, 0x00, 0x00, // len - will be fixed up

    0xD7, // TrackNumber (L3)
    0x81, // len = 1
    0x01, // TrackNumber = 0x01

    0x73, 0xC5,                                     // TrackUID (L3)
    0x88,                                           // len = 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // TrackUID = 1 - placeholder for 8 bytes

    0x83, // TrackType (L3)
    0x81, // len = 1
    0x01, // TrackType = 0x01 = video

    0x53, 0x6E,                                                                                    // Name (L3)
    0x90,                                                                                          // len = 16
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Track name  - placeholder for 16 bytes
};
static const uint32_t gSegmentTrackEntryHeaderSize = sizeof(gSegmentTrackEntryHeader);

static uint8_t gSegmentTrackEntryCodecHeader[] = {
    0x86, // CodecID (L3)
    0x40,
    0x00 // len - will be fixed up
         // Codec value follows
};
static const uint32_t gSegmentTrackEntryCodecHeaderSize = sizeof(gSegmentTrackEntryCodecHeader);

static uint8_t gSegmentTrackEntryVideoHeader[] = {
    0xE0, // Video (L3)
    0x10,
    0x00,
    0x00,
    0x08, // len = 8

    0xB0, // PixelWidth (L4)
    0x82, // len = 2
    0x00,
    0x00, // Pixel Width - a placeholder

    0xBA, // PixelHeight (L4)
    0x82, // len = 2
    0x00,
    0x00 // Pixel Height - a placeholder
};
static const uint32_t gSegmentTrackEntryVideoHeaderSize = sizeof(gSegmentTrackEntryVideoHeader);

static uint8_t gSegmentTrackEntryAudioHeader[] = {
    0xE1, // Audio (L3)
    0x10,
    0x00,
    0x00,
    0x0D, // len = 13

    0xB5, // Audio (L4)
    0x88, // len = 8
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // freq in double format - a placeholder

    0x9F, // channels (L4)
    0x81, // len = 1
    0x00  // channel number - a placeholder
};
static const uint32_t gSegmentTrackEntryAudioHeaderSize = sizeof(gSegmentTrackEntryAudioHeader);

static uint8_t gSegmentTrackEntryAudioHeaderBitsPerSample[] = {
    0x62,
    0x64, // bits per sample (L4)
    0x81, // len = 1
    0x00  // bps - a placeholder
};
static const uint8_t gSegmentTrackEntryAudioHeaderBitsPerSampleSize = sizeof(gSegmentTrackEntryAudioHeaderBitsPerSample);

static uint8_t gSegmentTrackEntryCodecPrivateHeader[] = {
    0x63,
    0xA2, // CodecPrivate (L3)
    0x10,
    0x00,
    0x00,
    0x00 // len - will be fixed up
         // Codec private data follows
};
static const uint32_t gSegmentTrackEntryCodecPrivateHeaderSize = sizeof(gSegmentTrackEntryCodecPrivateHeader);

static uint8_t gClusterHeader[] = {
    0x1F,
    0x43,
    0xB6,
    0x75, // Cluster (L1)
    0xFF, // len = -1, unknown

    0xE7, // Timestamp (L2)
    0x88, // len = 8
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // epoch time with time unit equals timescale - a placeholder

    0xA7, // Position (L2)
    0x81, // len = 1
    0x00  // Position = 0x00
};
static const uint32_t gClusterHeaderSize = sizeof(gClusterHeader);

static uint8_t gClusterSimpleBlock[] = {
    0xA3, // SimpleBlock (L2)
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // len = SimpleBlock Header 4 bytes + raw data size - will be fixed up
    0x81, // track Number, 0x81 is track number 1 - a placeholder
    0x00,
    0x00, // timecode, relative to cluster timecode - INT16 - needs to be fixed up
    0x00  // Flags - needs to be fixed up
          // Frame data follows
};
static const uint32_t gClusterSimpleBlockSize = sizeof(gClusterSimpleBlock);

// Sampling Frequency in Hz
static uint32_t gMkvAACSamplingFrequencies[] = {
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000,
    7350,
};
static uint32_t gMkvAACSamplingFrequenciesCount = sizeof(gMkvAACSamplingFrequencies) / sizeof(uint32_t);

/*-----------------------------------------------------------*/

static int prvCreateVideoTrackEntry(VideoTrackInfo_t *pVideoTrackInfo, uint8_t **ppBuf, uint32_t *puBufLen)
{
    int res = KVS_ERRNO_NONE;
    uint32_t uHeaderLen = 0;
    uint8_t *pHeader = NULL;
    uint8_t *pIdx = NULL;
    bool bHasCodecPrivateData = false;

    if (pVideoTrackInfo == NULL || pVideoTrackInfo->pCodecName == NULL || ppBuf == NULL || puBufLen == NULL)
    {
        LogError("Invalid arguments");
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        bHasCodecPrivateData = (pVideoTrackInfo->pCodecPrivate != NULL && pVideoTrackInfo->uCodecPrivateLen != 0);

        /* Calculate header length */
        uHeaderLen = gSegmentTrackEntryHeaderSize;
        uHeaderLen += gSegmentTrackEntryCodecHeaderSize + strlen(pVideoTrackInfo->pCodecName);
        uHeaderLen += gSegmentTrackEntryVideoHeaderSize;
        if (bHasCodecPrivateData)
        {
            uHeaderLen += gSegmentTrackEntryCodecPrivateHeaderSize;
            uHeaderLen += pVideoTrackInfo->uCodecPrivateLen;
        }

        if ((pHeader = (uint8_t *)kvsMalloc(uHeaderLen)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
            LogError("OOM: video track entry header");
        }
        else
        {
            pIdx = pHeader;

            memcpy(pIdx, gSegmentTrackEntryHeader, gSegmentTrackEntryHeaderSize);
            *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET) = (uint8_t)TRACK_VIDEO;
            PUT_UNALIGNED_8_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET, TRACK_VIDEO);
            *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET) = (uint8_t)TRACK_VIDEO;
            snprintf((char *)(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET), TRACK_NAME_MAX_LEN, "%s", pVideoTrackInfo->pTrackName);
            pIdx += gSegmentTrackEntryHeaderSize;

            memcpy(pIdx, gSegmentTrackEntryCodecHeader, gSegmentTrackEntryCodecHeaderSize);
            PUT_UNALIGNED_2_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET, MKV_LENGTH_INDICATOR_2_BYTE | strlen(pVideoTrackInfo->pCodecName));
            pIdx += gSegmentTrackEntryCodecHeaderSize;

            memcpy(pIdx, pVideoTrackInfo->pCodecName, strlen(pVideoTrackInfo->pCodecName));
            pIdx += strlen(pVideoTrackInfo->pCodecName);

            memcpy(pIdx, gSegmentTrackEntryVideoHeader, gSegmentTrackEntryVideoHeaderSize);
            PUT_UNALIGNED_2_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_VIDEO_WIDTH_OFFSET, pVideoTrackInfo->uWidth);
            PUT_UNALIGNED_2_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_VIDEO_HEIGHT_OFFSET, pVideoTrackInfo->uHeight);
            pIdx += gSegmentTrackEntryVideoHeaderSize;

            if (bHasCodecPrivateData)
            {
                memcpy(pIdx, gSegmentTrackEntryCodecPrivateHeader, gSegmentTrackEntryCodecPrivateHeaderSize);
                PUT_UNALIGNED_4_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | pVideoTrackInfo->uCodecPrivateLen);
                pIdx += gSegmentTrackEntryCodecPrivateHeaderSize;

                memcpy(pIdx, pVideoTrackInfo->pCodecPrivate, pVideoTrackInfo->uCodecPrivateLen);
                pIdx += pVideoTrackInfo->uCodecPrivateLen;

                PUT_UNALIGNED_4_byte_BE(pHeader + MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | (uHeaderLen - MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE));

                *ppBuf = pHeader;
                *puBufLen = uHeaderLen;
            }
        }
    }

    return res;
}

/*-----------------------------------------------------------*/

static int prvCreateAudioTrackEntry(AudioTrackInfo_t *pAudioTrackInfo, uint8_t **ppBuf, uint32_t *pBufLen)
{
    int res = KVS_ERRNO_NONE;
    uint32_t uHeaderLen = 0;
    uint8_t *pHeader = NULL;
    uint8_t *pIdx = NULL;
    uint32_t uAudioHeaderLen = 0;
    bool bHasCodecPrivateData = false;
    bool bHasBitsPerSampleField = false;
    double audioFrequency = 0.0;

    if (pAudioTrackInfo == NULL || ppBuf == NULL || pBufLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        bHasCodecPrivateData = (pAudioTrackInfo->pCodecPrivate != NULL && pAudioTrackInfo->uCodecPrivateLen != 0);
        bHasBitsPerSampleField = (pAudioTrackInfo->uBitsPerSample > 0);

        /* Calculate header length */
        uHeaderLen = gSegmentTrackEntryHeaderSize;
        uHeaderLen += gSegmentTrackEntryCodecHeaderSize + strlen(pAudioTrackInfo->pCodecName);
        uHeaderLen += gSegmentTrackEntryAudioHeaderSize;
        if (bHasBitsPerSampleField)
        {
            uHeaderLen += gSegmentTrackEntryAudioHeaderBitsPerSampleSize;
        }
        if (bHasCodecPrivateData)
        {
            uHeaderLen += gSegmentTrackEntryCodecPrivateHeaderSize;
            uHeaderLen += pAudioTrackInfo->uCodecPrivateLen;
        }

        if ((pHeader = (uint8_t *)kvsMalloc(uHeaderLen)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
            LogError("OOM: audio track entry header");
        }
        else
        {
            pIdx = pHeader;

            memcpy(pIdx, gSegmentTrackEntryHeader, gSegmentTrackEntryHeaderSize);
            *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET) = (uint8_t)TRACK_AUDIO;
            PUT_UNALIGNED_8_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET, TRACK_AUDIO);
            *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET) = (uint8_t)TRACK_AUDIO;
            snprintf((char *)(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET), TRACK_NAME_MAX_LEN, "%s", pAudioTrackInfo->pTrackName);
            pIdx += gSegmentTrackEntryHeaderSize;

            memcpy(pIdx, gSegmentTrackEntryCodecHeader, gSegmentTrackEntryCodecHeaderSize);
            PUT_UNALIGNED_2_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET, MKV_LENGTH_INDICATOR_2_BYTE | strlen(pAudioTrackInfo->pCodecName));
            pIdx += gSegmentTrackEntryCodecHeaderSize;

            memcpy(pIdx, pAudioTrackInfo->pCodecName, strlen(pAudioTrackInfo->pCodecName));
            pIdx += strlen(pAudioTrackInfo->pCodecName);

            memcpy(pIdx, gSegmentTrackEntryAudioHeader, gSegmentTrackEntryAudioHeaderSize);
            audioFrequency = (double)pAudioTrackInfo->uFrequency;
            PUT_UNALIGNED_8_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_FREQUENCY_OFFSET, *((uint64_t *)(&audioFrequency)));
            *(pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_CHANNEL_NUMBER_OFFSET) = pAudioTrackInfo->uChannelNumber;
            if (bHasBitsPerSampleField)
            {
                /* Adjust the length field of Audio header. */
                uAudioHeaderLen = gSegmentTrackEntryAudioHeaderSize + gSegmentTrackEntryAudioHeaderBitsPerSampleSize - MKV_SEGMENT_TRACK_ENTRY_AUDIO_HEADER_SIZE;
                PUT_UNALIGNED_4_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | uAudioHeaderLen);
            }
            pIdx += gSegmentTrackEntryAudioHeaderSize;

            if (bHasBitsPerSampleField)
            {
                memcpy(pIdx, gSegmentTrackEntryAudioHeaderBitsPerSample, gSegmentTrackEntryAudioHeaderBitsPerSampleSize);
                *(pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_BPS_OFFSET) = pAudioTrackInfo->uBitsPerSample;
                pIdx += gSegmentTrackEntryAudioHeaderBitsPerSampleSize;
            }

            if (bHasCodecPrivateData)
            {
                memcpy(pIdx, gSegmentTrackEntryCodecPrivateHeader, gSegmentTrackEntryCodecPrivateHeaderSize);
                PUT_UNALIGNED_4_byte_BE(pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | pAudioTrackInfo->uCodecPrivateLen);
                pIdx += gSegmentTrackEntryCodecPrivateHeaderSize;

                memcpy(pIdx, pAudioTrackInfo->pCodecPrivate, pAudioTrackInfo->uCodecPrivateLen);
                pIdx += pAudioTrackInfo->uCodecPrivateLen;
            }

            PUT_UNALIGNED_4_byte_BE(pHeader + MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | (uHeaderLen - MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE));

            *ppBuf = pHeader;
            *pBufLen = uHeaderLen;
        }
    }

    return res;
}

/*-----------------------------------------------------------*/

int Mkv_initializeHeaders(MkvHeader_t *pMkvHeader, VideoTrackInfo_t *pVideoTrackInfo, AudioTrackInfo_t *pAudioTrackInfo)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pIdx = NULL;
    uint32_t uHeaderLen = 0;
    uint8_t pSegmentUuid[UUID_LEN] = {0};
    uint32_t uSegmentTracksLen = 0;

    uint8_t *pSegmentVideo = NULL;
    uint32_t uSegmentVideoLen = 0;

    bool bHasAudioTrack = false;
    uint8_t *pSegmentAudio = NULL;
    uint32_t uSegmentAudioLen = 0;

    size_t i = 0;

    if (pMkvHeader == NULL || pVideoTrackInfo == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        memset(pMkvHeader, 0, sizeof(MkvHeader_t));

        for (i = 0; i < UUID_LEN; i++)
        {
            pSegmentUuid[i] = getRandomNumber();
        }

        bHasAudioTrack = (pAudioTrackInfo != NULL) ? true : false;

        if ((res = prvCreateVideoTrackEntry(pVideoTrackInfo, &pSegmentVideo, &uSegmentVideoLen)) != KVS_ERRNO_NONE ||
            (bHasAudioTrack && (res = prvCreateAudioTrackEntry(pAudioTrackInfo, &pSegmentAudio, &uSegmentAudioLen)) != KVS_ERRNO_NONE))
        {
            LogError("Failed to create track entries");
            /* Propagate the res error */
        }
        else
        {
            /* Calculate size of EBML and Segment header. */
            uHeaderLen = gEbmlHeaderSize;
            uHeaderLen += gSegmentHeaderSize;
            uHeaderLen += gSegmentInfoHeaderSize;
            uHeaderLen += gSegmentTrackHeaderSize;

            uHeaderLen += uSegmentVideoLen;
            uHeaderLen += (bHasAudioTrack) ? uSegmentAudioLen : 0;

            /* Allocate memory for EBML and Segment header. */
            if ((pMkvHeader->pHeader = (uint8_t *)kvsMalloc(uHeaderLen)) == NULL)
            {
                LogError("OOM: MKV Header");
                res = KVS_ERROR_OUT_OF_MEMORY;
            }
            else
            {
                pIdx = pMkvHeader->pHeader;

                memcpy(pIdx, gEbmlHeader, gEbmlHeaderSize);
                pIdx += gEbmlHeaderSize;

                memcpy(pIdx, gSegmentHeader, gSegmentHeaderSize);
                pIdx += gSegmentHeaderSize;

                memcpy(pIdx, gSegmentInfoHeader, gSegmentInfoHeaderSize);
                memcpy(pIdx + MKV_SEGMENT_INFO_UID_OFFSET, pSegmentUuid, UUID_LEN);
                snprintf((char *)(pIdx + MKV_SEGMENT_INFO_TITLE_OFFSET), SEGMENT_TITLE_MAX_LEN, "%s", SEGMENT_TITLE);
                snprintf((char *)(pIdx + MKV_SEGMENT_INFO_MUXING_APP_OFFSET), MUXING_APP_MAX_LEN, "%s", MUXING_APP);
                snprintf((char *)(pIdx + MKV_SEGMENT_INFO_WRITING_APP_OFFSET), WRITING_APP_MAX_LEN, "%s", WRITING_APP);
                pIdx += gSegmentInfoHeaderSize;

                memcpy(pIdx, gSegmentTrackHeader, gSegmentTrackHeaderSize);
                uSegmentTracksLen = uSegmentVideoLen;
                uSegmentTracksLen += (bHasAudioTrack) ? uSegmentAudioLen : 0;
                PUT_UNALIGNED_4_byte_BE(pIdx + MKV_SEGMENT_TRACK_LENGTH_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | uSegmentTracksLen);
                pIdx += gSegmentTrackHeaderSize;

                memcpy(pIdx, pSegmentVideo, uSegmentVideoLen);
                pIdx += uSegmentVideoLen;

                if (bHasAudioTrack)
                {
                    memcpy(pIdx, pSegmentAudio, uSegmentAudioLen);
                    pIdx += uSegmentAudioLen;
                }

                pMkvHeader->uHeaderLen = uHeaderLen;
            }
        }
    }

    if (pSegmentVideo != NULL)
    {
        kvsFree(pSegmentVideo);
    }

    if (pSegmentAudio != NULL)
    {
        kvsFree(pSegmentAudio);
    }

    return res;
}

/*-----------------------------------------------------------*/

void Mkv_terminateHeaders(MkvHeader_t *pMkvHeader)
{
    if (pMkvHeader != NULL)
    {
        if (pMkvHeader->pHeader != NULL)
        {
            kvsFree(pMkvHeader->pHeader);
            pMkvHeader->pHeader = NULL;
        }
        pMkvHeader->uHeaderLen = 0;
    }
}

/*-----------------------------------------------------------*/

size_t Mkv_getClusterHdrLen(MkvClusterType_t xType)
{
    size_t uLen = 0;

    if (xType == MKV_CLUSTER)
    {
        uLen = gClusterHeaderSize + gClusterSimpleBlockSize;
    }
    else if (xType == MKV_SIMPLE_BLOCK)
    {
        uLen = gClusterSimpleBlockSize;
    }

    return uLen;
}

int Mkv_initializeClusterHdr(
    uint8_t *pMkvHeader,
    size_t uMkvHeaderSize,
    MkvClusterType_t xType,
    size_t uFrameSize,
    TrackType_t xTrackType,
    bool bIsKeyFrame,
    uint64_t uAbsoluteTimestamp,
    uint16_t uDeltaTimestamp)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pIdx = NULL;
    size_t uMkvHeaderLen = Mkv_getClusterHdrLen(xType);

    if (pMkvHeader == NULL || uMkvHeaderLen > uMkvHeaderSize)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if (xType == MKV_CLUSTER)
    {
        pIdx = pMkvHeader;
        memcpy(pIdx, gClusterHeader, gClusterHeaderSize);
        PUT_UNALIGNED_8_byte_BE(pIdx + 7, uAbsoluteTimestamp);
        pIdx += gClusterHeaderSize;

        memcpy(pIdx, gClusterSimpleBlock, gClusterSimpleBlockSize);
        *(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_TRACK_NUMBER_OFFSET) = MKV_LENGTH_INDICATOR_1_BYTE | ((uint8_t)xTrackType & 0xFF);
        PUT_UNALIGNED_4_byte_BE(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_FRAME_SIZE_OFFSET, SIMPLE_BLOCK_HEADER_SIZE + uFrameSize);
        *(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_PROPERTY_OFFSET) = 0x80;
        pIdx += gClusterSimpleBlockSize;
    }
    else if (xType == MKV_SIMPLE_BLOCK)
    {
        pIdx = pMkvHeader;
        memcpy(pIdx, gClusterSimpleBlock, gClusterSimpleBlockSize);
        *(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_TRACK_NUMBER_OFFSET) = MKV_LENGTH_INDICATOR_1_BYTE | ((uint8_t)xTrackType & 0xFF);
        PUT_UNALIGNED_4_byte_BE(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_FRAME_SIZE_OFFSET, SIMPLE_BLOCK_HEADER_SIZE + uFrameSize);
        PUT_UNALIGNED_2_byte_BE(pIdx + MKV_CLUSTER_SIMPLE_BLOCK_DELTA_TIMESTAMP_OFFSET, uDeltaTimestamp);
        pIdx += gClusterSimpleBlockSize;
    }
    else
    {
        res = KVS_ERROR_MKV_UNKNOWN_CLUSTER_TYPE;
        LogError("Unknown MKV cluster type");
    }

    return res;
}

/*-----------------------------------------------------------*/
int Mkv_generateH264CodecPrivateDataFromAnnexBNalus(uint8_t *pAnnexBBuf, size_t uAnnexBLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pSps = NULL;
    size_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    size_t uPpsLen = 0;
    uint8_t *pCpdIdx = NULL;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateLen = 0;

    if (pAnnexBBuf == NULL || uAnnexBLen == 0 || ppCodecPrivateData == NULL || puCodecPrivateDataLen == NULL)
    {
        LogError("Invalid argument");
        res = KVS_ERRNO_FAIL;
    }
    else if (
        (res = NALU_getNaluFromAnnexBNalus(pAnnexBBuf, uAnnexBLen, NALU_TYPE_SPS, &pSps, &uSpsLen)) != KVS_ERRNO_NONE ||
        (res = NALU_getNaluFromAnnexBNalus(pAnnexBBuf, uAnnexBLen, NALU_TYPE_PPS, &pPps, &uPpsLen)) != KVS_ERRNO_NONE)
    {
        LogInfo("Failed to get SPS and PPS from Annex-B NALU");
        /* Propagate the res error */
    }
    else
    {
        uCodecPrivateLen = MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE + uSpsLen + uPpsLen;

        if ((pCodecPrivateData = (uint8_t *)kvsMalloc(uCodecPrivateLen)) == NULL)
        {
            LogError("OOM: H264 codec private data");
            res = KVS_ERROR_OUT_OF_MEMORY;
        }
        else
        {
            pCpdIdx = pCodecPrivateData;
            *(pCpdIdx++) = 0x01; /* Version */
            *(pCpdIdx++) = pSps[1];
            *(pCpdIdx++) = pSps[2];
            *(pCpdIdx++) = pSps[3];
            *(pCpdIdx++) = 0xFF; /* '111111' reserved + '11' lengthSizeMinusOne which is 3 (i.e. AVCC header size = 4) */

            *(pCpdIdx++) = 0xE1; /* '111' reserved + '00001' numOfSequenceParameterSets which is 1 */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uSpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pSps, uSpsLen);
            pCpdIdx += uSpsLen;

            *(pCpdIdx++) = 0x01; /* 1 numOfPictureParameterSets */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uPpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pPps, uPpsLen);
            pCpdIdx += uPpsLen;

            *ppCodecPrivateData = pCodecPrivateData;
            *puCodecPrivateDataLen = uCodecPrivateLen;
        }
    }

    return res;
}

/*-----------------------------------------------------------*/
int Mkv_generateH264CodecPrivateDataFromAvccNalus(uint8_t *pAvccBuf, size_t uAvccLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pSps = NULL;
    size_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    size_t uPpsLen = 0;
    uint8_t *pCpdIdx = NULL;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateLen = 0;

    if (pAvccBuf == NULL || uAvccLen == 0 || ppCodecPrivateData == NULL || puCodecPrivateDataLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if (
        (res = NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_SPS, &pSps, &uSpsLen)) != KVS_ERRNO_NONE ||
        (res = NALU_getNaluFromAvccNalus(pAvccBuf, uAvccLen, NALU_TYPE_PPS, &pPps, &uPpsLen)) != KVS_ERRNO_NONE)
    {
        LogInfo("Failed to get SPS and PPS from AVCC NALU");
        /* Propagate the res error */
    }
    else
    {
        uCodecPrivateLen = MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE + uSpsLen + uPpsLen;

        if ((pCodecPrivateData = (uint8_t *)kvsMalloc(uCodecPrivateLen)) == NULL)
        {
            LogError("OOM: H264 codec private data");
            res = KVS_ERROR_OUT_OF_MEMORY;
        }
        else
        {
            pCpdIdx = pCodecPrivateData;
            *(pCpdIdx++) = 0x01; /* Version */
            *(pCpdIdx++) = pSps[1];
            *(pCpdIdx++) = pSps[2];
            *(pCpdIdx++) = pSps[3];
            *(pCpdIdx++) = 0xFF; /* '111111' reserved + '11' lengthSizeMinusOne which is 3 (i.e. AVCC header size = 4) */

            *(pCpdIdx++) = 0xE1; /* '111' reserved + '00001' numOfSequenceParameterSets which is 1 */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uSpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pSps, uSpsLen);
            pCpdIdx += uSpsLen;

            *(pCpdIdx++) = 0x01; /* 1 numOfPictureParameterSets */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uPpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pPps, uPpsLen);
            pCpdIdx += uPpsLen;

            *ppCodecPrivateData = pCodecPrivateData;
            *puCodecPrivateDataLen = uCodecPrivateLen;
        }
    }

    return res;
}

/*-----------------------------------------------------------*/

int Mkv_generateH264CodecPrivateDataFromSpsPps(uint8_t *pSps, size_t uSpsLen, uint8_t *pPps, size_t uPpsLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pCpdIdx = NULL;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateLen = 0;

    if (pSps == NULL || uSpsLen == 0 || pPps == NULL || uPpsLen == 0 || ppCodecPrivateData == NULL || puCodecPrivateDataLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        uCodecPrivateLen = MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE + uSpsLen + uPpsLen;

        if ((pCodecPrivateData = (uint8_t *)kvsMalloc(uCodecPrivateLen)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
            LogError("OOM: H264 codec private data");
        }
        else
        {
            pCpdIdx = pCodecPrivateData;
            *(pCpdIdx++) = 0x01; /* Version */
            *(pCpdIdx++) = pSps[1];
            *(pCpdIdx++) = pSps[2];
            *(pCpdIdx++) = pSps[3];
            *(pCpdIdx++) = 0xFF; /* '111111' reserved + '11' lengthSizeMinusOne which is 3 (i.e. AVCC header size = 4) */

            *(pCpdIdx++) = 0xE1; /* '111' reserved + '00001' numOfSequenceParameterSets which is 1 */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uSpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pSps, uSpsLen);
            pCpdIdx += uSpsLen;

            *(pCpdIdx++) = 0x01; /* 1 numOfPictureParameterSets */
            PUT_UNALIGNED_2_byte_BE(pCpdIdx, uPpsLen);
            pCpdIdx += 2;
            memcpy(pCpdIdx, pPps, uPpsLen);
            pCpdIdx += uPpsLen;

            *ppCodecPrivateData = pCodecPrivateData;
            *puCodecPrivateDataLen = uCodecPrivateLen;
        }
    }

    return res;
}

/*-----------------------------------------------------------*/

int Mkv_generateAacCodecPrivateData(Mpeg4AudioObjectTypes_t objectType, uint32_t frequency, uint16_t channel, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateLen = 0;
    uint16_t uAacCodecPrivateData = 0;
    bool bSamplingFreqFound = false;
    uint16_t uSamplingFreqIndex = 0;
    uint16_t i = 0;

    if (ppCodecPrivateData == NULL || puCodecPrivateDataLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        for (i = 0; i < gMkvAACSamplingFrequenciesCount; i++)
        {
            if (gMkvAACSamplingFrequencies[i] == frequency)
            {
                uSamplingFreqIndex = i;
                bSamplingFreqFound = true;
                break;
            }
        }

        if (!bSamplingFreqFound)
        {
            res = KVS_ERROR_MKV_INVALID_AUDIO_FREQUENCY;
            LogError("Invalid audio sampling frequency");
        }
        else if ((pCodecPrivateData = (uint8_t *)kvsMalloc(MKV_AAC_CPD_SIZE_BYTE)) == NULL)
        {
            res = KVS_ERROR_OUT_OF_MEMORY;
            LogError("OOM: AAC codec private data");
        }
        else
        {
            uCodecPrivateLen = MKV_AAC_CPD_SIZE_BYTE;
            uAacCodecPrivateData = (((uint16_t)objectType) << 11) | (uSamplingFreqIndex << 7) | (channel << 3);
            PUT_UNALIGNED_2_byte_BE(pCodecPrivateData, uAacCodecPrivateData);

            *ppCodecPrivateData = pCodecPrivateData;
            *puCodecPrivateDataLen = uCodecPrivateLen;
        }
    }

    return res;
}

/*-----------------------------------------------------------*/

int Mkv_generatePcmCodecPrivateData(PcmFormatCode_t format, uint32_t uSamplingRate, uint16_t channels, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pCodecPrivateData = NULL;
    size_t uCodecPrivateLen = 0;
    uint8_t *pIdx = NULL;
    uint32_t uAvgBytesPerSecond = 0;
    uint16_t uBitsPerSample = 0;

    if (ppCodecPrivateData == NULL || puCodecPrivateDataLen == NULL || (uSamplingRate < MIN_PCM_SAMPLING_RATE || uSamplingRate > MAX_PCM_SAMPLING_RATE) ||
        (channels != 1 && channels != 2))
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((pCodecPrivateData = (uint8_t *)kvsMalloc(MKV_PCM_CPD_SIZE_BYTE)) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
        LogError("OOM: PCM codec private data");
    }
    else
    {
        uCodecPrivateLen = MKV_PCM_CPD_SIZE_BYTE;
        memset(pCodecPrivateData, 0, uCodecPrivateLen);

        uAvgBytesPerSecond = channels * uSamplingRate;
        uBitsPerSample = channels * 8;

        pIdx = pCodecPrivateData;
        PUT_UNALIGNED_2_byte_LE(pIdx, format);
        pIdx += 2;
        PUT_UNALIGNED_2_byte_LE(pIdx, channels);
        pIdx += 2;
        PUT_UNALIGNED_4_byte_LE(pIdx, uSamplingRate);
        pIdx += 4;
        PUT_UNALIGNED_4_byte_LE(pIdx, uAvgBytesPerSecond);
        pIdx += 4;
        PUT_UNALIGNED_2_byte_LE(pIdx, 0);
        pIdx += 2;
        PUT_UNALIGNED_2_byte_LE(pIdx, uBitsPerSample);
        pIdx += 2;
        PUT_UNALIGNED_2_byte_LE(pIdx, 0);
        pIdx += 2;

        *ppCodecPrivateData = pCodecPrivateData;
        *puCodecPrivateDataLen = uCodecPrivateLen;
    }

    return res;
}

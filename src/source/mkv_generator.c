/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "mkv_generator.h"
#include "kvs_errors.h"
#include "kvs_port.h"

#define MKV_LENGTH_INDICATOR_1_BYTE                         ( 0x80 )
#define MKV_LENGTH_INDICATOR_2_BYTE                         ( 0x4000 )
#define MKV_LENGTH_INDICATOR_3_BYTE                         ( 0x200000 )
#define MKV_LENGTH_INDICATOR_4_BYTE                         ( 0x10000000 )

#define MKV_VIDEO_TRACK_UID                                 ( 1 )
#define MKV_VIDEO_TRACK_TYPE                                ( 1 )

#define MKV_AUDIO_TRACK_UID                                 ( 2 )
#define MKV_AUDIO_TRACK_TYPE                                ( 2 )

/* The offset of UUID value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_UID_OFFSET                         ( 9 )

/* The offset of Title value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_TITLE_OFFSET                       ( 40 )

/* The offset of MuxingApp value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_MUXING_APP_OFFSET                  ( 59 )

/* The offset of WritingApp value in gSegmentInfoHeader */
#define MKV_SEGMENT_INFO_WRITING_APP_OFFSET                 ( 78 )

/* The offset of length field in gSegmentTrackHeader */
#define MKV_SEGMENT_TRACK_LENGTH_OFFSET                     ( 4 )

/* The size of type and length in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE                 ( 5 )

/* The offset of length field in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET                  ( 1 )

/* The offset of track number in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET         ( 7 )

/* The offset of track UID in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET            ( 11 )

/* The offset of track type in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET           ( 21 )

/* The offset of track name in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET           ( 25 )

/* The offset of track codec in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET            ( 1 )

/* The offset of width field in gSegmentTrackEntryVideoHeader */
#define MKV_SEGMENT_TRACK_ENTRY_VIDEO_WIDTH_OFFSET          ( 7 )

/* The offset of height field in gSegmentTrackEntryVideoHeader */
#define MKV_SEGMENT_TRACK_ENTRY_VIDEO_HEIGHT_OFFSET         ( 11 )

/* The size of type and length in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_HEADER_SIZE           ( 5 )

/* The offset of track codec in gSegmentTrackEntryHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_LEN_OFFSET            ( 1 )

/* The offset of frequency in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_FREQUENCY_OFFSET      ( 7 )

/* The offset of channel number in gSegmentTrackEntryAudioHeader */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_CHANNEL_NUMBER_OFFSET ( 17 )

/* The offset of value field in gSegmentTrackEntryAudioHeaderBitsPerSample */
#define MKV_SEGMENT_TRACK_ENTRY_AUDIO_BPS_OFFSET            ( 2 )

/* The offset of length field in gSegmentTrackEntryCodecPrivateHeader */
#define MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET    ( 2 )

/* The size of simple block header */
#define SIMPLE_BLOCK_HEADER_SIZE                            ( 4 )

/* The offset of frame size offset in gClusterSimpleBlock.  This field is 8 bytes long, but we only use least 4 bytes. */
#define MKV_CLUSTER_SIMPLE_BLOCK_FRAME_SIZE_OFFSET          ( 5 )

/* The offset of track number in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_TRACK_NUMBER_OFFSET        ( 9 )

/* The offset of delta timestamp in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_DELTA_TIMESTAMP_OFFSET     ( 10 )

/* The offset of property in gClusterSimpleBlock */
#define MKV_CLUSTER_SIMPLE_BLOCK_PROPERTY_OFFSET            ( 12 )

/* In H264 extended profile, the size except sps and pps. */
#define MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE       ( 11 )

/* It's a pre-defined MKV header of EBML document. EBML is used for the first frame in a streaming. There is no
 * configurable field in this header. */
static uint8_t gEbmlHeader[] =
{
    0x1A, 0x45, 0xDF, 0xA3, // EBML Header
    0xA3, // len = 35

        0x42, 0x86, // EBML Version
        0x81, // len = 1
        0x01, // EBMLVersion = 0x01

        0x42, 0xF7, // EBML Read Version
        0x81, // len = 1
        0x01, // EBMLReadVersion = 0x01

        0x42, 0xF2, // EBML Max ID Length
        0x81, // len = 1
        0x04, // EBMLMaxIDLength = 0x04

        0x42, 0xF3, // EBML Max Size Length
        0x81, // len = 1
        0x08, // EBMLMaxSizeLength = 0x08

        0x42, 0x82, // Doc Type
        0x88, // len = 8
        0x6D, 0x61, 0x74, 0x72, 0x6F, 0x73, 0x6B, 0x61, // DocType = "matroska"

        0x42, 0x87, // Doc Type Version
        0x81, // len = 1
        0x02, // DocTypeVersion = 0x02

        0x42, 0x85, // Doc Type Read Version
        0x81, // len = 1
        0x02 // DocTypeReadVersion = 0x02
};
static const uint32_t gEbmlHeaderSize = sizeof( gEbmlHeader );

static uint8_t gSegmentHeader[] =
{
    0x18, 0x53, 0x80, 0x67, // Segment (L0)
    0xFF, // len = -1 for unknown
};
static const uint32_t gSegmentHeaderSize = sizeof( gSegmentHeader );

static uint8_t gSegmentInfoHeader[] =
{
        0x15, 0x49, 0xA9, 0x66, // Info (L1)
        0x40, 0x58, // len = 88

            0x73, 0xA4, // SegmentUID (L2)
            0x90, // len = 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // SegmentUID

            0x2A, 0xD7, 0xB1, // TimestampScale (L2)
            0x88, // len = 8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x42, 0x40, // TimestampScale = 1000000ns = 1ms

            0x7B, 0xA9, // Title (L2)
            0x90, // len = 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

            0x4D, 0x80, // MuxingApp (L2)
            0x90, // len = 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

            0x57, 0x41, // WritingApp (L2)
            0x90, // len = 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint32_t gSegmentInfoHeaderSize = sizeof( gSegmentInfoHeader );

static uint8_t gSegmentTrackHeader[] =
{
        0x16, 0x54, 0xAE, 0x6B, // Tracks (L1)
        0x10, 0x00, 0x00, 0x00  // len - will be fixed up
};
static const uint32_t gSegmentTrackHeaderSize = sizeof( gSegmentTrackHeader );

static uint8_t gSegmentTrackEntryHeader[] =
{
            0xAE, // TrackEntry (L2)
            0x10, 0x00, 0x00, 0x00, // len - will be fixed up

                0xD7, // TrackNumber (L3)
                0x81, // len = 1
                0x01, // TrackNumber = 0x01

                0x73, 0xC5, // TrackUID (L3)
                0x88, // len = 8
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // TrackUID = 1 - placeholder for 8 bytes

                0x83, // TrackType (L3)
                0x81, // len = 1
                0x01, // TrackType = 0x01 = video

                0x53, 0x6E, // Name (L3)
                0x90, // len = 16
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Track name  - placeholder for 16 bytes
};
static const uint32_t gSegmentTrackEntryHeaderSize = sizeof( gSegmentTrackEntryHeader );

static uint8_t gSegmentTrackEntryCodecHeader[] =
{
                0x86, // CodecID (L3)
                0x40, 0x00 // len - will be fixed up
                // Codec value follows
};
static const uint32_t gSegmentTrackEntryCodecHeaderSize = sizeof( gSegmentTrackEntryCodecHeader );

static uint8_t gSegmentTrackEntryVideoHeader[] =
{
                0xE0, // Video (L3)
                0x10, 0x00, 0x00, 0x08, // len = 8

                    0xB0, // PixelWidth (L4)
                    0x82, // len = 2
                    0x00, 0x00, // Pixel Width - a placeholder

                    0xBA, // PixelHeight (L4)
                    0x82, // len = 2
                    0x00, 0x00  // Pixel Height - a placeholder
};
static const uint32_t gSegmentTrackEntryVideoHeaderSize = sizeof( gSegmentTrackEntryVideoHeader );

static uint8_t gSegmentTrackEntryAudioHeader[] =
{
                0xE1, // Audio (L3)
                0x10, 0x00, 0x00, 0x0D, // len = 13

                    0xB5, // Audio (L4)
                    0x88, // len = 8
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // freq in double format - a placeholder

                    0x9F, // channels (L4)
                    0x81, // len = 1
                    0x00  // channel number - a placeholder
};
static const uint32_t gSegmentTrackEntryAudioHeaderSize = sizeof( gSegmentTrackEntryAudioHeader );

static uint8_t gSegmentTrackEntryAudioHeaderBitsPerSample[] =
{
                    0x62, 0x64, // bits per sample (L4)
                    0x81,       // len = 1
                    0x00        // bps - a placeholder
};
static const uint8_t gSegmentTrackEntryAudioHeaderBitsPerSampleSize = sizeof( gSegmentTrackEntryAudioHeaderBitsPerSample );

static uint8_t gSegmentTrackEntryCodecPrivateHeader[] =
        {
                0x63, 0xA2, // CodecPrivate (L3)
                0x10, 0x00, 0x00, 0x00 // len - will be fixed up
                // Codec private data follows
        };
static const uint32_t gSegmentTrackEntryCodecPrivateHeaderSize = sizeof( gSegmentTrackEntryCodecPrivateHeader );

static uint8_t gClusterHeader[] =
{
        0x1F, 0x43, 0xB6, 0x75, // Cluster (L1)
        0xFF, // len = -1, unknown

            0xE7, // Timestamp (L2)
            0x88, // len = 8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // epoch time with time unit equals timescale - a placeholder

            0xA7, // Position (L2)
            0x81, // len = 1
            0x00  // Position = 0x00
};
static const uint32_t gClusterHeaderSize = sizeof( gClusterHeader );

static uint8_t gClusterSimpleBlock[] =
{
            0xA3, // SimpleBlock (L2)
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // len = SimpleBlock Header 4 bytes + raw data size - will be fixed up
            0x81, // track Number, 0x81 is track number 1 - a placeholder
            0x00, 0x00, // timecode, relative to cluster timecode - INT16 - needs to be fixed up
            0x00  // Flags - needs to be fixed up
            // Frame data follows
};
static const uint32_t gClusterSimpleBlockSize = sizeof( gClusterSimpleBlock );

// Sampling Frequency in Hz
static uint32_t gMkvAACSamplingFrequencies[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350,
};
static uint32_t gMkvAACSamplingFrequenciesCount = sizeof( gMkvAACSamplingFrequencies ) / sizeof( uint32_t );

typedef struct
{
    uint32_t uRbspBeginIdx;
    uint32_t uRbspLen;
} NalRbsp_t;

/*-----------------------------------------------------------*/

static int32_t createVideoTrackEntry( VideoTrackInfo_t * pVideoTrackInfo,
                                      uint8_t ** ppBuf,
                                      uint32_t * pBufLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uHeaderLen = 0;
    uint8_t *pHeader = NULL;
    uint8_t *pIdx = NULL;
    bool bHasCodecPrivateData = false;

    if( pVideoTrackInfo == NULL || ppBuf == NULL || pBufLen == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( pVideoTrackInfo->pCodecPrivate != NULL && pVideoTrackInfo->uCodecPrivateLen != 0 )
        {
            bHasCodecPrivateData = true;
        }

        /* Calculate header length */
        uHeaderLen = gSegmentTrackEntryHeaderSize;
        uHeaderLen += gSegmentTrackEntryCodecHeaderSize + strlen( pVideoTrackInfo->pCodecName );
        uHeaderLen += gSegmentTrackEntryVideoHeaderSize;
        if( bHasCodecPrivateData )
        {
            uHeaderLen += gSegmentTrackEntryCodecPrivateHeaderSize;
            uHeaderLen += pVideoTrackInfo->uCodecPrivateLen;
        }

        /* Allocate memory for this track entry */
        pHeader = ( uint8_t * )sysMalloc( uHeaderLen );
        if( pHeader == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pIdx = pHeader;

        memcpy( pIdx, gSegmentTrackEntryHeader, gSegmentTrackEntryHeaderSize );
        *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET) = ( uint8_t )MKV_VIDEO_TRACK_NUMBER;
        PUT_UNALIGNED_8_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET, MKV_VIDEO_TRACK_UID );
        *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET) = ( uint8_t )MKV_VIDEO_TRACK_TYPE;
        snprintf(( char * )( pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET ), TRACK_NAME_MAX_LEN, "%s", pVideoTrackInfo->pTrackName );
        pIdx += gSegmentTrackEntryHeaderSize;

        memcpy( pIdx, gSegmentTrackEntryCodecHeader, gSegmentTrackEntryCodecHeaderSize );
        PUT_UNALIGNED_2_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET, MKV_LENGTH_INDICATOR_2_BYTE | strlen( pVideoTrackInfo->pCodecName ) );
        pIdx += gSegmentTrackEntryCodecHeaderSize;

        memcpy( pIdx, pVideoTrackInfo->pCodecName, strlen( pVideoTrackInfo->pCodecName ) );
        pIdx += strlen( pVideoTrackInfo->pCodecName );

        memcpy( pIdx, gSegmentTrackEntryVideoHeader, gSegmentTrackEntryVideoHeaderSize );
        PUT_UNALIGNED_2_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_VIDEO_WIDTH_OFFSET, pVideoTrackInfo->uWidth );
        PUT_UNALIGNED_2_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_VIDEO_HEIGHT_OFFSET, pVideoTrackInfo->uHeight );
        pIdx += gSegmentTrackEntryVideoHeaderSize;

        if( bHasCodecPrivateData )
        {
            memcpy( pIdx, gSegmentTrackEntryCodecPrivateHeader, gSegmentTrackEntryCodecPrivateHeaderSize );
            PUT_UNALIGNED_4_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | pVideoTrackInfo->uCodecPrivateLen );
            pIdx += gSegmentTrackEntryCodecPrivateHeaderSize;

            memcpy( pIdx, pVideoTrackInfo->pCodecPrivate, pVideoTrackInfo->uCodecPrivateLen );
            pIdx += pVideoTrackInfo->uCodecPrivateLen;
        }

        PUT_UNALIGNED_4_byte_BE( pHeader + MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | ( uHeaderLen - MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE ) );

        *ppBuf = pHeader;
        *pBufLen = uHeaderLen;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

static int32_t createAudioTrackEntry( AudioTrackInfo_t * pAudioTrackInfo,
                                      uint8_t ** ppBuf,
                                      uint32_t * pBufLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uHeaderLen = 0;
    uint8_t *pHeader = NULL;
    uint8_t *pIdx = NULL;
    uint32_t uAudioHeaderLen = 0;
    bool bHasCodecPrivateData = false;
    bool bHasBitsPerSampleField = false;
    double audioFrequency = 0.0;

    if( pAudioTrackInfo == NULL || ppBuf == NULL || pBufLen == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( pAudioTrackInfo->pCodecPrivate != NULL && pAudioTrackInfo->uCodecPrivateLen != 0 )
        {
            bHasCodecPrivateData = true;
        }

        if( pAudioTrackInfo->uBitsPerSample > 0 )
        {
            bHasBitsPerSampleField = true;
        }

        /* Calculate header length */
        uHeaderLen = gSegmentTrackEntryHeaderSize;
        uHeaderLen += gSegmentTrackEntryCodecHeaderSize + strlen( pAudioTrackInfo->pCodecName );
        uHeaderLen += gSegmentTrackEntryAudioHeaderSize;
        if( bHasBitsPerSampleField )
        {
            uHeaderLen += gSegmentTrackEntryAudioHeaderBitsPerSampleSize;
        }
        if( bHasCodecPrivateData )
        {
            uHeaderLen += gSegmentTrackEntryCodecPrivateHeaderSize;
            uHeaderLen += pAudioTrackInfo->uCodecPrivateLen;
        }

        /* Allocate memory for this track entry */
        pHeader = ( uint8_t * )sysMalloc( uHeaderLen );
        if( pHeader == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pIdx = pHeader;

        memcpy( pIdx, gSegmentTrackEntryHeader, gSegmentTrackEntryHeaderSize );
        *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NUMBER_OFFSET) = ( uint8_t )MKV_AUDIO_TRACK_NUMBER;
        PUT_UNALIGNED_8_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_UID_OFFSET, MKV_AUDIO_TRACK_UID );
        *(pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_TYPE_OFFSET) = ( uint8_t )MKV_AUDIO_TRACK_TYPE;
        snprintf(( char * )( pIdx + MKV_SEGMENT_TRACK_ENTRY_TRACK_NAME_OFFSET ), TRACK_NAME_MAX_LEN, "%s", pAudioTrackInfo->pTrackName );
        pIdx += gSegmentTrackEntryHeaderSize;

        memcpy( pIdx, gSegmentTrackEntryCodecHeader, gSegmentTrackEntryCodecHeaderSize );
        PUT_UNALIGNED_2_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_LEN_OFFSET, MKV_LENGTH_INDICATOR_2_BYTE | strlen( pAudioTrackInfo->pCodecName ) );
        pIdx += gSegmentTrackEntryCodecHeaderSize;

        memcpy( pIdx, pAudioTrackInfo->pCodecName, strlen( pAudioTrackInfo->pCodecName ) );
        pIdx += strlen( pAudioTrackInfo->pCodecName );

        memcpy( pIdx, gSegmentTrackEntryAudioHeader, gSegmentTrackEntryAudioHeaderSize );
        audioFrequency = (double) pAudioTrackInfo->uFrequency;
        PUT_UNALIGNED_8_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_FREQUENCY_OFFSET, *( ( uint64_t * )( &audioFrequency ) ) );
        *( pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_CHANNEL_NUMBER_OFFSET ) = pAudioTrackInfo->uChannelNumber;
        if( bHasBitsPerSampleField )
        {
            /* Adjust the length field of Audio header. */
            uAudioHeaderLen = gSegmentTrackEntryAudioHeaderSize + gSegmentTrackEntryAudioHeaderBitsPerSampleSize - MKV_SEGMENT_TRACK_ENTRY_AUDIO_HEADER_SIZE;
            PUT_UNALIGNED_4_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | uAudioHeaderLen );
        }
        pIdx += gSegmentTrackEntryAudioHeaderSize;

        if( bHasBitsPerSampleField )
        {
            memcpy( pIdx, gSegmentTrackEntryAudioHeaderBitsPerSample, gSegmentTrackEntryAudioHeaderBitsPerSampleSize );
            *( pIdx + MKV_SEGMENT_TRACK_ENTRY_AUDIO_BPS_OFFSET ) = pAudioTrackInfo->uBitsPerSample;
            pIdx += gSegmentTrackEntryAudioHeaderBitsPerSampleSize;
        }

        if( bHasCodecPrivateData )
        {
            memcpy( pIdx, gSegmentTrackEntryCodecPrivateHeader, gSegmentTrackEntryCodecPrivateHeaderSize );
            PUT_UNALIGNED_4_byte_BE( pIdx + MKV_SEGMENT_TRACK_ENTRY_CODEC_PRIVATE_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | pAudioTrackInfo->uCodecPrivateLen );
            pIdx += gSegmentTrackEntryCodecPrivateHeaderSize;

            memcpy( pIdx, pAudioTrackInfo->pCodecPrivate, pAudioTrackInfo->uCodecPrivateLen );
            pIdx += pAudioTrackInfo->uCodecPrivateLen;
        }

        PUT_UNALIGNED_4_byte_BE( pHeader + MKV_SEGMENT_TRACK_ENTRY_LEN_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | ( uHeaderLen - MKV_SEGMENT_TRACK_ENTRY_HEADER_SIZE ) );

        *ppBuf = pHeader;
        *pBufLen = uHeaderLen;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_initializeHeaders( MkvHeader_t *pMkvHeader,
                               VideoTrackInfo_t *pVideoTrackInfo,
                               AudioTrackInfo_t *pAudioTrackInfo )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint8_t *pIdx = NULL;
    uint32_t uHeaderLen = 0;
    uint32_t uSegmentTracksLen = 0;

    uint8_t *pSegmentVideo = NULL;
    uint32_t uSegmentVideoLen = 0;

    bool bHasAudioTrack = false;
    uint8_t *pSegmentAudio = NULL;
    uint32_t uSegmentAudioLen = 0;

    uint8_t trackIdx = 0;

    if( pMkvHeader == NULL || pVideoTrackInfo == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        memset( pMkvHeader, 0, sizeof( MkvHeader_t) );

        for( int i = 0; i < UUID_LEN; i++ )
        {
            pMkvHeader->pSegmentUuid[ i ] = getRandomNumber();
        }

        bHasAudioTrack = ( pAudioTrackInfo != NULL ) ? true : false;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        retStatus = createVideoTrackEntry( pVideoTrackInfo, &pSegmentVideo, &uSegmentVideoLen );
    }

    if( retStatus == KVS_STATUS_SUCCEEDED && bHasAudioTrack )
    {
        retStatus = createAudioTrackEntry( pAudioTrackInfo, &pSegmentAudio, &uSegmentAudioLen );
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        /* Calculate size of EBML and Segment header. */
        uHeaderLen = gEbmlHeaderSize;
        uHeaderLen += gSegmentHeaderSize;
        uHeaderLen += gSegmentInfoHeaderSize;
        uHeaderLen += gSegmentTrackHeaderSize;

        uHeaderLen += uSegmentVideoLen;
        uHeaderLen += ( bHasAudioTrack ) ? uSegmentAudioLen : 0;

        /* Allocate memory for EBML and Segment header. */
        pMkvHeader->pHeader = ( uint8_t * )sysMalloc( uHeaderLen );
        if( pMkvHeader->pHeader == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pIdx = pMkvHeader->pHeader;

        memcpy( pIdx, gEbmlHeader, gEbmlHeaderSize );
        pIdx += gEbmlHeaderSize;

        memcpy( pIdx, gSegmentHeader, gSegmentHeaderSize );
        pIdx += gSegmentHeaderSize;

        memcpy( pIdx, gSegmentInfoHeader, gSegmentInfoHeaderSize );
        memcpy( pIdx + MKV_SEGMENT_INFO_UID_OFFSET, pMkvHeader->pSegmentUuid, UUID_LEN );
        snprintf( ( char * )( pIdx + MKV_SEGMENT_INFO_TITLE_OFFSET ), SEGMENT_TITLE_MAX_LEN, "%s", SEGMENT_TITLE );
        snprintf( ( char * )( pIdx + MKV_SEGMENT_INFO_MUXING_APP_OFFSET ), MUXING_APP_MAX_LEN, "%s", MUXING_APP );
        snprintf( ( char * )( pIdx + MKV_SEGMENT_INFO_WRITING_APP_OFFSET ), WRITING_APP_MAX_LEN, "%s", WRITING_APP );
        pIdx += gSegmentInfoHeaderSize;

        memcpy( pIdx, gSegmentTrackHeader, gSegmentTrackHeaderSize );
        uSegmentTracksLen = uSegmentVideoLen;
        uSegmentTracksLen += ( bHasAudioTrack ) ? uSegmentAudioLen : 0;
        PUT_UNALIGNED_4_byte_BE( pIdx + MKV_SEGMENT_TRACK_LENGTH_OFFSET, MKV_LENGTH_INDICATOR_4_BYTE | uSegmentTracksLen );
        pIdx += gSegmentTrackHeaderSize;

        memcpy( pIdx, pSegmentVideo, uSegmentVideoLen );
        pIdx += uSegmentVideoLen;

        if( bHasAudioTrack )
        {
            memcpy( pIdx, pSegmentAudio, uSegmentAudioLen );
            pIdx += uSegmentAudioLen;
        }

        pMkvHeader->uHeaderLen = uHeaderLen;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pMkvHeader->pCluster = ( uint8_t * )sysMalloc( gClusterHeaderSize );
        if( pMkvHeader->pCluster == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else
        {
            memcpy( pMkvHeader->pCluster, gClusterHeader, gClusterHeaderSize );
            pMkvHeader->uClusterLen = gClusterHeaderSize;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        for( trackIdx = 1; trackIdx <= MKV_TRACK_SIZE; trackIdx++ )
        {
            pMkvHeader->pSimpleBlock[ trackIdx - 1 ] = ( uint8_t * )sysMalloc( gClusterSimpleBlockSize );
            if( pMkvHeader->pSimpleBlock == NULL )
            {
                retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
                break;
            }
            else
            {
                memcpy( pMkvHeader->pSimpleBlock[ trackIdx - 1 ], gClusterSimpleBlock, gClusterSimpleBlockSize );
                *( pMkvHeader->pSimpleBlock[ trackIdx - 1 ] + MKV_CLUSTER_SIMPLE_BLOCK_TRACK_NUMBER_OFFSET ) = MKV_LENGTH_INDICATOR_1_BYTE | trackIdx;
                pMkvHeader->uSimpleBlockLen[ trackIdx - 1 ] = gClusterSimpleBlockSize;
            }
        }
    }

    if( retStatus != KVS_STATUS_SUCCEEDED )
    {
        Mkv_terminateHeaders(pMkvHeader);
    }

    if( pSegmentVideo != NULL )
    {
        sysFree( pSegmentVideo );
    }

    if( pSegmentAudio != NULL )
    {
        sysFree( pSegmentAudio );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

void Mkv_terminateHeaders( MkvHeader_t * pMkvHeader )
{
    uint8_t trackIdx = 0;

    if( pMkvHeader != NULL )
    {
        if( pMkvHeader->pHeader != NULL )
        {
            sysFree( pMkvHeader->pHeader );
        }
        pMkvHeader->uHeaderLen = 0;

        if( pMkvHeader->pCluster != NULL )
        {
            sysFree( pMkvHeader->pCluster );
        }
        pMkvHeader->uClusterLen = 0;

        for( trackIdx = 1; trackIdx <= MKV_TRACK_SIZE; trackIdx++ )
        {
            if( pMkvHeader->pSimpleBlock[ trackIdx - 1 ] != NULL )
            {
                sysFree( pMkvHeader->pSimpleBlock[ trackIdx - 1 ] );
            }
            pMkvHeader->uSimpleBlockLen[ trackIdx - 1 ] = 0;
        }
    }
}

/*-----------------------------------------------------------*/

int32_t Mkv_updateTimestamp( MkvHeader_t * pMkvHeader,
                             uint64_t timestamp )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    PUT_UNALIGNED_8_byte_BE( pMkvHeader->pCluster + 7, timestamp );

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_updateDeltaTimestamp( MkvHeader_t * pMkvHeader,
                                  uint8_t trackId,
                                  uint16_t deltaTimestamp )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pMkvHeader == NULL || !( trackId >= 1 && trackId <= MKV_TRACK_SIZE ) || pMkvHeader->pSimpleBlock[ trackId - 1 ] == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        PUT_UNALIGNED_2_byte_BE( pMkvHeader->pSimpleBlock[ trackId - 1 ] + MKV_CLUSTER_SIMPLE_BLOCK_DELTA_TIMESTAMP_OFFSET, deltaTimestamp );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_updateFrameSize( MkvHeader_t * pMkvHeader,
                             uint8_t trackId,
                             uint32_t uFrameSize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pMkvHeader == NULL || !( trackId >= 1 && trackId <= MKV_TRACK_SIZE ) || pMkvHeader->pSimpleBlock[ trackId - 1 ] == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        PUT_UNALIGNED_4_byte_BE( pMkvHeader->pSimpleBlock[ trackId - 1 ] + MKV_CLUSTER_SIMPLE_BLOCK_FRAME_SIZE_OFFSET, SIMPLE_BLOCK_HEADER_SIZE + uFrameSize );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_updateProperty( MkvHeader_t * pMkvHeader,
                            uint8_t trackId,
                            bool isKeyFrame )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint8_t property = 0;

    if( pMkvHeader == NULL || !( trackId >= 1 && trackId <= MKV_TRACK_SIZE ) || pMkvHeader->pSimpleBlock[ trackId - 1 ] == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        property = *( pMkvHeader->pSimpleBlock[ trackId - 1 ] + MKV_CLUSTER_SIMPLE_BLOCK_PROPERTY_OFFSET );
        if( isKeyFrame )
        {
            property |= 0x80;
        }
        else
        {
            property &= ~0x80;
        }
        *( pMkvHeader->pSimpleBlock[ trackId - 1 ] + MKV_CLUSTER_SIMPLE_BLOCK_PROPERTY_OFFSET ) = property;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkvs_getEbmlSegmentHeader( MkvHeader_t * pMkvheader,
                                   uint8_t ** ppHeaderPtr,
                                   uint32_t *pHeaderLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pMkvheader == NULL || pHeaderLen == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        if( ppHeaderPtr != NULL )
        {
            *ppHeaderPtr = pMkvheader->pHeader;
        }
        *pHeaderLen = pMkvheader->uHeaderLen;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkvs_getClusterHeader( MkvHeader_t * pMkvheader,
                               uint8_t ** ppHeaderPtr,
                               uint32_t * pHeaderLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pMkvheader == NULL || pHeaderLen == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        if( ppHeaderPtr != NULL )
        {
            *ppHeaderPtr = pMkvheader->pCluster;
        }
        *pHeaderLen = pMkvheader->uClusterLen;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkvs_getSimpleBlockHeader( MkvHeader_t * pMkvheader,
                                   uint8_t trackId,
                                   uint8_t ** ppHeaderPtr,
                                   uint32_t *pHeaderLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pMkvheader == NULL || pHeaderLen == NULL || !( trackId >= 1 && trackId <= MKV_TRACK_SIZE ) )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        if( ppHeaderPtr != NULL )
        {
            *ppHeaderPtr = pMkvheader->pSimpleBlock[ trackId - 1 ];
        }
        *pHeaderLen = pMkvheader->uSimpleBlockLen[ trackId - 1 ];
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_getMaxHeaderSize( MkvHeader_t * pMkvHeader,
                              uint32_t * pBufferLen )
{
    return Mkv_getMkvHeader( pMkvHeader, true, false, MKV_VIDEO_TRACK_NUMBER, NULL, 0, pBufferLen );
}

/*-----------------------------------------------------------*/

int32_t Mkv_getMkvHeader( MkvHeader_t * pMkvHeader,
                          bool isFirstFrame,
                          bool isKeyFrame,
                          uint8_t trackId,
                          uint8_t * pBuffer,
                          uint32_t uBufferSize,
                          uint32_t * pBufferLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uHeaderSize = 0;
    uint8_t *pIdx = NULL;

    if( pMkvHeader == NULL || pBufferLen == NULL || !( trackId >= 1 && trackId <= MKV_TRACK_SIZE ) )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        /* Calculate header size. */
        if( isFirstFrame )
        {
            /* Video frame has to be the first frame. */
            uHeaderSize = pMkvHeader->uHeaderLen + pMkvHeader->uClusterLen + pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ];
        }
        else if( isKeyFrame )
        {
            /* Video frame has to be the key frame. */
            uHeaderSize = pMkvHeader->uClusterLen + pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ];
        }
        else
        {
            uHeaderSize = pMkvHeader->uSimpleBlockLen[ trackId - 1 ];
        }
        *pBufferLen = uHeaderSize;

        /* If destination buffer is not null, then copy the content. */
        if( pBuffer != NULL )
        {
            if( uBufferSize < uHeaderSize )
            {
                retStatus = KVS_STATUS_BUFFER_SIZE_NOT_ENOUGH;
            }
            else
            {
                pIdx = pBuffer;

                if( isFirstFrame )
                {
                    /* Video frame has to be the first frame. */
                    memcpy( pIdx, pMkvHeader->pHeader, pMkvHeader->uHeaderLen );
                    pIdx += pMkvHeader->uHeaderLen;
                    memcpy( pIdx, pMkvHeader->pCluster, pMkvHeader->uClusterLen );
                    pIdx += pMkvHeader->uClusterLen;
                    memcpy( pIdx, pMkvHeader->pSimpleBlock[ MKV_VIDEO_TRACK_NUMBER - 1 ], pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ] );
                    pIdx += pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ];
                }
                else if( isKeyFrame )
                {
                    /* Video frame has to be the key frame. */
                    memcpy( pIdx, pMkvHeader->pCluster, pMkvHeader->uClusterLen );
                    pIdx += pMkvHeader->uClusterLen;
                    memcpy( pIdx, pMkvHeader->pSimpleBlock[ MKV_VIDEO_TRACK_NUMBER - 1 ], pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ] );
                    pIdx += pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ];
                }
                else
                {
                    memcpy( pIdx, pMkvHeader->pSimpleBlock[ trackId - 1 ], pMkvHeader->uSimpleBlockLen[ trackId - 1 ] );
                    pIdx += pMkvHeader->uSimpleBlockLen[ MKV_VIDEO_TRACK_NUMBER - 1 ];
                }
            }
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_convertAnnexBtoAvccInPlace( uint8_t * pAnnexbBuf,
                                        uint32_t uAnnexbBufLen,
                                        uint32_t uAnnexbBufSize,
                                        uint32_t * pAvccLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t i = 0;
    NalRbsp_t rbsps[ MAX_NALU_COUNT_IN_A_FRAME ];
    uint32_t uNalRbspCount = 0;
    uint32_t uAvccTotalLen = 0;
    uint32_t uAvccIdx = 0;
    uint8_t *pVal = NULL;

    if( pAnnexbBuf == NULL || uAnnexbBufLen == 0 || pAvccLen == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        /* Go through all Annex-B buffer and record all RBSP begin and length first. */
        while( i < uAnnexbBufLen - 4 )
        {
            if( pAnnexbBuf[ i ] == 0x00 )
            {
                if( pAnnexbBuf[ i + 1 ] == 0x00 )
                {
                    if( pAnnexbBuf[ i + 2 ] == 0x00 )
                    {
                        if( pAnnexbBuf[ i + 3 ] == 0x01 )
                        {
                            /* 0x00000001 is start code of NAL. */
                            if( uNalRbspCount > 0 )
                            {
                                rbsps[ uNalRbspCount - 1 ].uRbspLen = i - rbsps[ uNalRbspCount - 1 ].uRbspBeginIdx;
                            }

                            i += 4;
                            rbsps[ uNalRbspCount++ ].uRbspBeginIdx = i;
                        }
                        else if( pAnnexbBuf[ i + 3 ] == 0x00 )
                        {
                            /* 0x00000000 is not allowed. */
                            break;
                        }
                        else
                        {
                            /* 0x000000XX is acceptable. */
                            i += 4;
                        }
                    }
                    else if( pAnnexbBuf[ i + 2 ] == 0x01 )
                    {
                        /* 0x000001 is start code of NAL */
                        if( uNalRbspCount > 0 )
                        {
                            rbsps[ uNalRbspCount - 1 ].uRbspLen = i - rbsps[ uNalRbspCount - 1 ].uRbspBeginIdx;
                        }

                        i += 3;
                        rbsps[ uNalRbspCount++ ].uRbspBeginIdx = i;
                    }
                    else
                    {
                        /* 0x0000XX is acceptable. It includes EPB case and we reserve EPB byte. */
                        i += 3;
                    }
                }
                else
                {
                    /* 0x00XX is acceptable. */
                    i += 2;
                }
            }
            else
            {
                /* 0xXX is acceptable. */
                i++;
            }
        }

        if( uNalRbspCount == 0 )
        {
            retStatus = KVS_STATUS_MKV_INVALID_ANNEXB_CONTENT;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( uNalRbspCount > 0 )
        {
            /* Update the last BSPS. */
            rbsps[ uNalRbspCount - 1 ].uRbspLen = uAnnexbBufLen - rbsps[ uNalRbspCount - 1 ].uRbspBeginIdx;
        }

        /* Calculate needed size if we convert it to Avcc format. */
        uAvccTotalLen = 4 * uNalRbspCount;
        for( i = 0; i < uNalRbspCount; i++ )
        {
            uAvccTotalLen += rbsps[ i ].uRbspLen;
        }

        if( uAvccTotalLen > uAnnexbBufSize )
        {
            /* We don't have enough space to convert Annex-B to Avcc in place. */
            *pAvccLen = 0;
            retStatus = KVS_STATUS_BUFFER_SIZE_NOT_ENOUGH;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        /* move RBSP from back to head */
        i = uNalRbspCount - 1;
        uAvccIdx = uAvccTotalLen;
        do
        {
            /* move RBSP */
            uAvccIdx -= rbsps[ i ].uRbspLen;
            memmove( pAnnexbBuf + uAvccIdx, pAnnexbBuf + rbsps[ i ].uRbspBeginIdx, rbsps[ i ].uRbspLen);

            /* fill length info */
            uAvccIdx -= 4;
            PUT_UNALIGNED_4_byte_BE( pAnnexbBuf + uAvccIdx, rbsps[ i ].uRbspLen );

            if( i == 0 )
            {
                break;
            }
            i--;
        } while ( true );

        *pAvccLen = uAvccTotalLen;
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_generateH264CodecPrivateDataFromAvccNalus( uint8_t * pAvccBuf,
                                                       uint32_t uAvccLen,
                                                       uint8_t * pCodecPrivateData,
                                                       uint32_t * pCodecPrivateLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint8_t *pCpdIdx = NULL;
    uint8_t *pSps = NULL;
    uint32_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    uint32_t uPpsLen = 0;
    uint32_t uCodecPrivateLen = 0;

    if( pAvccBuf == NULL || uAvccLen == 0 )
    {
        return KVS_STATUS_INVALID_ARG;
    }
    else
    {
        Mkv_getH264SpsPpsNalusFromAvccNalus( pAvccBuf, uAvccLen, &pSps, &uSpsLen, &pPps, &uPpsLen );
        if( pSps == NULL || uSpsLen == 0 || pPps == NULL || uPpsLen == 0 )
        {
            retStatus = KVS_STATUS_MKV_FAIL_TO_PARSE_SPS_N_PPS;
        }
        else
        {
            uCodecPrivateLen = MKV_VIDEO_H264_CODEC_PRIVATE_DATA_HEADER_SIZE + uSpsLen + uPpsLen;

            *pCodecPrivateLen = uCodecPrivateLen;

            if( pCodecPrivateData != NULL )
            {
                pCpdIdx = pCodecPrivateData;
                *( pCpdIdx++ ) = 0x01; // Version
                *( pCpdIdx++ ) = pSps[1];
                *( pCpdIdx++ ) = pSps[2];
                *( pCpdIdx++ ) = pSps[3];
                *( pCpdIdx++ ) = 0xFF; // '111111' reserved + '11' lengthSizeMinusOne which is 3 (i.e. AVCC header size = 4)

                *( pCpdIdx++ ) = 0xE1; // '111' reserved + '00001' numOfSequenceParameterSets which is 1
                PUT_UNALIGNED_2_byte_BE( pCpdIdx, uSpsLen );
                pCpdIdx += 2;
                memcpy( pCpdIdx, pSps, uSpsLen );
                pCpdIdx += uSpsLen;

                *( pCpdIdx++ ) = 0x01; // 1 numOfPictureParameterSets
                PUT_UNALIGNED_2_byte_BE( pCpdIdx, uPpsLen );
                pCpdIdx += 2;
                memcpy( pCpdIdx, pPps, uPpsLen );
                pCpdIdx += uPpsLen;

                if( uCodecPrivateLen != pCpdIdx - pCodecPrivateData )
                {
                    retStatus = KVS_STATUS_MKV_FAIL_TO_GENERATE_CODEC_PRIVATE_DATA;
                }
                else
                {
                    *pCodecPrivateLen = uCodecPrivateLen;
                }
            }
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_getH264SpsPpsNalusFromAvccNalus( uint8_t * pAvccBuf,
                                             uint32_t uAvccLen,
                                             uint8_t ** ppSps,
                                             uint32_t * pSpsLen,
                                             uint8_t ** ppPps,
                                             uint32_t * pPpsLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uAvccIdx = 0;
    uint8_t *pSps = NULL;
    uint32_t uSpsLen = 0;
    uint8_t *pPps = NULL;
    uint32_t uPpsLen = 0;
    uint32_t uNaluLen = 0;

    while( uAvccIdx < uAvccLen && ( pSps == NULL || pPps == NULL ) )
    {
        uNaluLen = ( pAvccBuf[ uAvccIdx ] << 24 ) | ( pAvccBuf[ uAvccIdx + 1 ] << 16 ) | ( pAvccBuf[ uAvccIdx + 2 ] << 8 ) | pAvccBuf[ uAvccIdx + 3 ];
        uAvccIdx += 4;

        if( pSps == NULL && ( pAvccBuf[ uAvccIdx ] & 0x80 ) == 0 && ( pAvccBuf[ uAvccIdx ] & 0x60 ) != 0 && (pAvccBuf[ uAvccIdx ] & 0x1F) == 0x07 )
        {
            pSps = pAvccBuf + uAvccIdx;
            uSpsLen = uNaluLen;
        }

        if( pPps == NULL && ( pAvccBuf[ uAvccIdx ] & 0x80 ) == 0 && ( pAvccBuf[ uAvccIdx ] & 0x60 ) != 0 && (pAvccBuf[ uAvccIdx ] & 0x1F) == 0x08 )
        {
            pPps = pAvccBuf + uAvccIdx;
            uPpsLen = uNaluLen;
        }

        uAvccIdx += uNaluLen;
    }

    *ppSps = pSps;
    *pSpsLen = uSpsLen;
    *ppPps = pPps;
    *pPpsLen = uPpsLen;

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_generateAacCodecPrivateData( Mpeg4AudioObjectTypes objectType,
                                         uint32_t frequency,
                                         uint16_t channel,
                                         uint8_t * pBuffer,
                                         uint32_t uBufferSize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint16_t uCodecPrivateData = 0;
    bool bSamplingFreqFound = false;
    uint16_t uSamplingFreqIndex = 0;
    uint16_t i = 0;

    if( pBuffer == NULL || uBufferSize < MKV_AAC_CPD_SIZE_BYTE )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        for( i = 0; i < gMkvAACSamplingFrequenciesCount; i++ )
        {
            if( gMkvAACSamplingFrequencies[ i ] == frequency )
            {
                uSamplingFreqIndex = i;
                bSamplingFreqFound = true;
            }
        }

        if( bSamplingFreqFound == false )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
        }
        else
        {
            memset( pBuffer, 0, MKV_AAC_CPD_SIZE_BYTE );
            uCodecPrivateData = ( ( ( uint16_t )objectType ) << 11 ) | ( uSamplingFreqIndex << 7 ) | ( channel << 3 );
            PUT_UNALIGNED_2_byte_BE( pBuffer, uCodecPrivateData );
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t Mkv_generatePcmCodecPrivateData( PcmFormatCode format, uint32_t uSamplingRate, uint16_t channels, uint8_t *pBuffer, uint32_t uBufferSize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uBitrate = 0;
    uint8_t *pIdx = NULL;

    if( pBuffer == NULL || uBufferSize < MKV_PCM_CPD_SIZE_BYTE )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( format != PCM_FORMAT_CODE_ALAW && format != PCM_FORMAT_CODE_MULAW )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( uSamplingRate < MIN_PCM_SAMPLING_RATE || uSamplingRate > MAX_PCM_SAMPLING_RATE )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( channels != 1 && channels != 2 )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        uint32_t uBitrate = channels * uSamplingRate / 8;

        memset( pBuffer, 0, MKV_PCM_CPD_SIZE_BYTE );

        pIdx = pBuffer;
        PUT_UNALIGNED_2_byte_LE( pIdx, format );
        pIdx += 2;
        PUT_UNALIGNED_2_byte_LE( pIdx, channels );
        pIdx += 2;
        PUT_UNALIGNED_4_byte_LE( pIdx, uSamplingRate );
        pIdx += 4;
        PUT_UNALIGNED_4_byte_LE( pIdx, uBitrate );
        pIdx += 4;
        PUT_UNALIGNED_2_byte_LE( pIdx, channels );
        pIdx += 2;
    }

    return retStatus;
}

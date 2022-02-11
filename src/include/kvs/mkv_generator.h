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

#ifndef KVS_MKV_GENERATOR_H
#define KVS_MKV_GENERATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#ifndef UUID_LEN
    #define UUID_LEN            ( 16 )
#endif

#define SEGMENT_TITLE_MAX_LEN   ( 16 )
#define SEGMENT_TITLE           "KVS"

#define MUXING_APP_MAX_LEN      ( 16 )
#define MUXING_APP              "KVS APP"

#define WRITING_APP_MAX_LEN     ( 16 )
#define WRITING_APP             "KVS APP"

#define TRACK_NAME_MAX_LEN      ( 16 )

#define MKV_TRACK_SIZE          ( 2 )

typedef enum TrackType
{
    TRACK_VIDEO = 1,
    TRACK_AUDIO = 2,
    TRACK_MAX = 2,
} TrackType_t;

typedef enum MkvClusterType
{
    MKV_SIMPLE_BLOCK = 0,
    MKV_CLUSTER = 1,
} MkvClusterType_t;

// 5 bits (Audio Object Type) | 4 bits (frequency index) | 4 bits (channel configuration) | 3 bits (not used)
#define MKV_AAC_CPD_SIZE_BYTE                ( 2 )

/* https://wiki.multimedia.cx/index.php/MPEG-4_Audio */
typedef enum Mpeg4AudioObjectTypes
{
    MPEG4_AAC_MAIN        = 1,
    MPEG4_AAC_LC          = 2,
    MPEG4_AAC_SSR         = 3,
    MPEG4_AAC_LTP         = 4,
    MPEG4_SBR             = 5,
    MPEG4_AAC_SCALABLE    = 6,
} Mpeg4AudioObjectTypes_t;

/*
 * http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
 * cpd structure (little endian):
 * - 2 bytes format code (0x06 0x00 for pcm alaw)
 * - 2 bytes number of channels
 * - 4 bytes sampling rate
 * - 4 bytes average bytes per second
 * - 2 bytes block align
 * - 2 bytes bit depth
 * - 2 bytes extra data (usually 0)
 */
#define MKV_PCM_CPD_SIZE_BYTE            ( 18 )

typedef enum PcmFormatCode
{
    PCM_FORMAT_CODE_ALAW       = (uint16_t)0x0006,
    PCM_FORMAT_CODE_MULAW      = (uint16_t)0x0007,
} PcmFormatCode_t;

// Min/Max sampling rate for PCM alaw and mulaw
#define MIN_PCM_SAMPLING_RATE 8000
#define MAX_PCM_SAMPLING_RATE 192000

typedef struct MkvHeader
{
    uint8_t *pHeader;
    uint32_t uHeaderLen;
} MkvHeader_t;

typedef struct VideoTrackInfo
{
    char *pTrackName;
    char *pCodecName;
    uint16_t uWidth;
    uint16_t uHeight;
    uint8_t *pCodecPrivate;
    uint32_t uCodecPrivateLen;
} VideoTrackInfo_t;

typedef struct AudioTrackInfo
{
    char *pTrackName;
    char *pCodecName;
    uint32_t uFrequency;
    uint8_t uChannelNumber;
    uint8_t uBitsPerSample;
    uint8_t *pCodecPrivate;
    size_t uCodecPrivateLen;
} AudioTrackInfo_t;

/**
 * @brief Initialize MKV EBML and segment header
 *
 * @param[in] pMkvHeader The MKV header to be initialized that is memory allocated
 * @param[in] pVideoTrackInfo The video track info
 * @param[in] pAudioTrackInfo The audio track info if any, or NULL if there is no audio track
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_initializeHeaders(MkvHeader_t *pMkvHeader,  VideoTrackInfo_t *pVideoTrackInfo, AudioTrackInfo_t *pAudioTrackInfo);

/**
 * @brief Terminate MKV header
 *
 * @param[in] pMkvHeader The MKV header to be terminated
 */
void Mkv_terminateHeaders(MkvHeader_t *pMkvHeader);

/**
 * @brief Return the header length of MKV cluster or simple block
 * @param[in] xType MKV cluster type
 * @return the length of MKV header
 */
size_t Mkv_getClusterHdrLen(MkvClusterType_t xType);

/**
 * @brief Initialize a MKV header of either MKV cluster or simple block
 *
 * @param[in] pMkvHeader MKV header buffer
 * @param[in] uMkvHeaderSize MKV header buffer size
 * @param[in] xType the MKV header type, either MKV cluster or MKV simple block
 * @param[in] uFrameSize the size of data frame
 * @param[in] xTrackType the track type (Ex. video or audio track)
 * @param[in] bIsKeyFrame ture if this data frame is a key frame, false otherwise
 * @param[in] uAbsoluteTimestamp absolution timestamp in milliseconds
 * @param[in] uDeltaTimestamp delta timestamp in milliseconds compare to the cluster data frame
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_initializeClusterHdr(uint8_t *pMkvHeader, size_t uMkvHeaderSize, MkvClusterType_t xType, size_t uFrameSize, TrackType_t xTrackType, bool bIsKeyFrame, uint64_t uAbsoluteTimestamp, uint16_t uDeltaTimestamp);

/**
 * @brief Create MKV codec private date for H264 from AVCC NALUs
 *
 * @param[in] pAnnexBBuf The Annex-B NALUs
 * @param[in] uAnnexBLen the length of Annex-B NALUs
 * @param[out] ppCodecPrivateData the generated codec private data that is memory allocated
 * @param[out] puCodecPrivateDataLen the length of generated codec private data
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_generateH264CodecPrivateDataFromAnnexBNalus(uint8_t *pAnnexBBuf, size_t uAnnexBLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen);

/**
 * @brief Create MKV codec private date for H264 from AVCC NALUs
 *
 * @param[in] pAvccBuf The AVCC NALUs
 * @param[in] uAvccLen the length of AVCC NALUs
 * @param[out] ppCodecPrivateData the generated codec private data that is memory allocated
 * @param[out] puCodecPrivateDataLen the length of generated codec private data
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_generateH264CodecPrivateDataFromAvccNalus(uint8_t *pAvccBuf, size_t uAvccLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen);

/**
 * @brief Generate H264 codec private data from SPS and PPS
 *
 * @param[in] pSps The SPS buffer
 * @param[in] uSpsLen The length of SPS
 * @param[in] pPps The PPS buffer
 * @param[in] uPpsLen The length of PPS
 * @param[out] ppCodecPrivateData the generated codec private data that is memory allocated
 * @param[out] puCodecPrivateDataLen the length of generated codec private data
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_generateH264CodecPrivateDataFromSpsPps(uint8_t *pSps, size_t uSpsLen, uint8_t *pPps, size_t uPpsLen, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen);

/**
 * @brief Create MKV codec private data for AAC
 *
 * @param[in] objectType The MPEG4 audio object type
 * @param[in] frequency The audio frequency or sampling rate
 * @param[in] channel The channel number
 * @param[out] ppCodecPrivateData The generated codec private data that is memory allocated
 * @param[out] puCodecPrivateDataLen The length of generated codec private data
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_generateAacCodecPrivateData(Mpeg4AudioObjectTypes_t objectType, uint32_t frequency, uint16_t channel, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen);

/**
 * @brief Create MKV codec private data for PCM
 *
 * @param[in] format The PCM format code
 * @param[in] uSamplingRate The audio frequency or sampling rate
 * @param[in] channels The channel number
 * @param[out] ppCodecPrivateData The generated codec private data that is memory allocated
 * @param[out] puCodecPrivateDataLen The length of generated codec private data
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_generatePcmCodecPrivateData(PcmFormatCode_t format, uint32_t uSamplingRate, uint16_t channels, uint8_t **ppCodecPrivateData, size_t *puCodecPrivateDataLen);

#endif /* KVS_MKV_GENERATOR_H */
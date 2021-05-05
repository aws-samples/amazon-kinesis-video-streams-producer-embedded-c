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

#ifndef _MKV_GENERATOR_H_
#define _MKV_GENERATOR_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef UUID_LEN
    #define UUID_LEN            ( 16 )
#endif

#define SEGMENT_TITLE_MAX_LEN   ( 16 )
#define SEGMENT_TITLE           "KVS RTOS"

#define MUXING_APP_MAX_LEN      ( 16 )
#define MUXING_APP              "KVS RTOS APP"

#define WRITING_APP_MAX_LEN     ( 16 )
#define WRITING_APP             "KVS RTOS APP"

#define TRACK_NAME_MAX_LEN      ( 16 )

#define MAX_NALU_COUNT_IN_A_FRAME ( 32 )

#define MKV_TRACK_SIZE          ( 2 )
#define MKV_VIDEO_TRACK_NUMBER  ( 1 )
#define MKV_AUDIO_TRACK_NUMBER  ( 2 )

// 5 bits (Audio Object Type) | 4 bits (frequency index) | 4 bits (channel configuration) | 3 bits (not used)
#define MKV_AAC_CPD_SIZE_BYTE                ( 2 )

/* https://wiki.multimedia.cx/index.php/MPEG-4_Audio */
typedef enum {
    AAC_MAIN        = 1,
    AAC_LC          = 2,
    AAC_SSR         = 3,
    AAC_LTP         = 4,
    SBR             = 5,
    AAC_SCALABLE    = 6,
} Mpeg4AudioObjectTypes;

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

typedef enum {
    PCM_FORMAT_CODE_ALAW       = ( uint16_t )0x0006,
    PCM_FORMAT_CODE_MULAW      = ( uint16_t )0x0007,
} PcmFormatCode;

// Min/Max sampling rate for PCM alaw and mulaw. Referred pad template of gstreamer alawenc plugin
#define MIN_PCM_SAMPLING_RATE 8000
#define MAX_PCM_SAMPLING_RATE 192000

typedef struct
{
    uint8_t *pHeader;
    uint32_t uHeaderLen;

    uint8_t pSegmentUuid[ UUID_LEN ];

    uint8_t *pCluster;
    uint32_t uClusterLen;

    uint8_t *pSimpleBlock[ MKV_TRACK_SIZE ];
    uint32_t uSimpleBlockLen[ MKV_TRACK_SIZE ];
} MkvHeader_t;

typedef struct
{
    char *pTrackName;
    char *pCodecName;
    uint16_t uWidth;
    uint16_t uHeight;
    uint8_t *pCodecPrivate;
    uint32_t uCodecPrivateLen;
} VideoTrackInfo_t;

typedef struct
{
    char *pTrackName;
    char *pCodecName;
    uint32_t uFrequency;
    uint8_t uChannelNumber;
    uint8_t uBitsPerSample;
    uint8_t *pCodecPrivate;
    uint32_t uCodecPrivateLen;
} AudioTrackInfo_t;

/**
 * @brief Initialize MKV headers
 *
 * @param[in] pMkvHeader The MKV object to be initialized
 * @param[in] pVideoTrackInfo Video track information
 * @param[in] pAudioTrackInfo Audio track information. Pass NULL if there is no audio track.
 *
 * @return KVS error code
 */
int32_t Mkv_initializeHeaders( MkvHeader_t * pMkvHeader,
                               VideoTrackInfo_t * pVideoTrackInfo,
                               AudioTrackInfo_t * pAudioTrackInfo );

/**
 * @brief Terminate MKV headers
 *
 * Free memory allocated in the object.
 *
 * @param[in] pMkvHeader The MKV object to be terminated
 */
void Mkv_terminateHeaders( MkvHeader_t * pMkvHeader );

/**
 * @brief Update absolute timestamp in the cluster header
 *
 * Update absolute timestamp in milliseconds in the Cluster header
 *
 * @param[in] pMkvHeader The MKV object to be updated
 * @param[in] timestamp Absolute timestamp in milliseconds
 *
 * @return KVS error code
 */
int32_t Mkv_updateTimestamp( MkvHeader_t * pMkvHeader,
                             uint64_t timestamp );

/**
 * @brief Update delta timestamp in the simple block header
 *
 * In the simple block header, the timestamp is related to the timestamp in the cluster header.
 *
 * @param[in] pMkvHeader The MKV object to be updated
 * @param[in] trackId The track id. (It could be video track or audio track.)
 * @param[in] deltaTimestamp The delta timestamp in milliseconds
 *
 * @return KVS error code
 */
int32_t Mkv_updateDeltaTimestamp( MkvHeader_t * pMkvHeader,
                                  uint8_t trackId,
                                  uint16_t deltaTimestamp );

/**
 * @brief Update frame size in the simple block header
 *
 * @param[in] pMkvHeader The MKV object to be updated
 * @param[in] trackId The track id. (It could be video track or audio track.)
 * @param[in] uFrameSize frame size in bytes
 *
 * @return MKV error code
 */
int32_t Mkv_updateFrameSize( MkvHeader_t * pMkvHeader,
                             uint8_t trackId,
                             uint32_t uFrameSize );

/**
 * @brief Update property in the simple block header
 *
 * @param[in] pMkvHeader The MKV object to be updated
 * @param[in] trackId  The track id. (It could be video track or audio track.)
 * @param[in] isKeyFrame Keyframe, set when the Block contains only keyframes.
 *
 * @return MKV error code
 */
int32_t Mkv_updateProperty( MkvHeader_t * pMkvHeader,
                            uint8_t trackId,
                            bool isKeyFrame );

/**
 * @breif Get max MKV header size
 *
 * Get max MKV header size, then application can allocate a buffer to get the MKV header via "Mkv_getMkvHeader" API.
 *
 * @param[in] pMkvHeader The MKV object to be calculated
 * @param[out] pBufferLen The max MKV header size
 *
 * @return MKV error code
 */
int32_t Mkv_getMaxHeaderSize( MkvHeader_t * pMkvHeader,
                              uint32_t * pBufferLen );

/**
 * @breif Get MKV header
 *
 * If isFirstFrame is true, then it returns EBML header, Segment header, Cluster header, and Video simple block.
 * If isFirstFrame is false and isKeyFrame is true, then it returns Cluster header and Video simple block.
 * If isFirstFrame is false, and isKeyFrame is false, then it returns simple block according to the track ID
 *
 * @param[in] pMkvHeader MKV object
 * @param[in] isFirstFrame Get header for first frame if it's true
 * @param[in] isKeyFrame Get header for key frame if it's true
 * @param[in] trackId Track ID
 * @param[out] pBuffer The buffer to store the header
 * @param[in] uBufferSize The buffer size. It's used to check if buffer overflow.
 * @param[out] pBufferLen The actual header size.
 *
 * @return MKV error code
 */
int32_t Mkv_getMkvHeader( MkvHeader_t * pMkvHeader,
                          bool isFirstFrame,
                          bool isKeyFrame,
                          uint8_t trackId,
                          uint8_t *pBuffer,
                          uint32_t uBufferSize,
                          uint32_t *pBufferLen );

/**
 * @brief Convert H264 Annex-B frame o AVCC frame in place
 *
 * KVS prefer AVCC format because AVCC provides for the advantage of random access. If we have Annnex-B frame then we
 * can convert it to AVCC frame in place by this API.
 *
 * @param[in,out] pAnnexbBuf The Annex-B frame buffer
 * @param[in] uAnnexbBufLen The frame length
 * @param[in] uAnnexbBufSize The buffer size
 * @param[out] pAvccLen The converted AVCC frame length
 *
 * @return MKV error code
 */
int32_t Mkv_convertAnnexBtoAvccInPlace( uint8_t * pAnnexbBuf,
                                        uint32_t uAnnexbBufLen,
                                        uint32_t uAnnexbBufSize,
                                        uint32_t * pAvccLen );

/**
 * @brief Generate H264 codec private data from AVCC frame
 *
 * In order to be decoded by decoder, the decoder needs "AVC decoder configuration record". This codec private data
 * would be put in the segment header in MKV headers. "AVC decoder configuration record" could be composed from SPS and
 * PPS NALUs from H264 frame.
 *
 * @param[in] pAvccBuf The H264 AVCC frame
 * @param[in] uAvccLen Frame length
 * @param[out] pCodecPrivateData The memory allocated codec private data. It needs to be freed by application
 * @param[out] pCodecPrivateLen The codec private data length
 *
 * @return MKV error code
 */
int32_t Mkv_generateH264CodecPrivateDataFromAvccNalus( uint8_t * pAvccBuf,
                                                       uint32_t uAvccLen,
                                                       uint8_t * pCodecPrivateData,
                                                       uint32_t * pCodecPrivateLen );

/**
 * @brief Get H264 SPS and PPS from AVCC frame
 *
 * @param[in] pAvccBuf The H264 AVCC frame buffer
 * @param[in] uAvccLen Buffer length
 * @param[out] ppSps SPS buffer address in the frame buffer. Set to NULL if it's not available.
 * @param[out] pSpsLen SPS buffer length
 * @param[out] ppPps PPS buffer address in the frame buffer. Set to NULL if it's not available.
 * @param[out] pPpsLen PPS buffer length
 *
 * @return KVS error code
 */
int32_t Mkv_getH264SpsPpsNalusFromAvccNalus( uint8_t * pAvccBuf,
                                             uint32_t uAvccLen,
                                             uint8_t ** ppSps,
                                             uint32_t * pSpsLen,
                                             uint8_t ** ppPps,
                                             uint32_t * pPpsLen );

/**
 * @brief Generate AAC codec private data
 *
 * For AAC audio, in order to be decoded by decoder, the MPEG-4 audio information must be put in the codec private data.
 * We can generate this by providing needed information.
 *
 * @param[in] objectType MPEG-4 audio object type
 * @param[in] frequency Audio frequency
 * @param[in] channel channel number
 * @param[out] pBuffer The buffer of codec private data
 * @param[in] uBufferSize Buffer size
 *
 * @return MKV error code
 */
int32_t Mkv_generateAacCodecPrivateData( Mpeg4AudioObjectTypes objectType,
                                         uint32_t frequency,
                                         uint16_t channel,
                                         uint8_t * pBuffer,
                                         uint32_t uBufferSize );

/**
 * @brief Generate PCM codec private data
 *
 * For AAC audio, in order to be decoded by decoder, the Microsofe WAVE information must be put in the codec private
 * data. We can generate this by providing needed information.
 *
 * @param[in] format PCM codec format
 * @param[in] uSamplingRate Sampling rate
 * @param[in] channels channel number
 * @param[out] pBuffer The buffer of codec private data
 * @param[in] uBufferSize Buffer size
 *
 * @return MKV error code
 */
int32_t Mkv_generatePcmCodecPrivateData( PcmFormatCode format,
                                         uint32_t uSamplingRate,
                                         uint16_t channels,
                                         uint8_t * pBuffer,
                                         uint32_t uBufferSize );

#endif // #ifndef _MKV_GENERATOR_H_
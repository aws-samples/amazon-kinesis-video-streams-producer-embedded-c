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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "example_config.h"
#include "kvs_rest_api.h"
#include "kvs_errors.h"
#include "kvs_port.h"
#include "mkv_generator.h"
#include "iot_credential_provider.h"

#define FRAGMENT_BEGIN_SIZE           ( 10 )
#define FRAGMENT_END_SIZE             ( 2 )

#define STREAM_STATE_INIT                   (0)
#define STREAM_STATE_LOAD_FRAME             (1)
#define STREAM_STATE_SEND_PACKAGE_HEADER    (2)
#define STREAM_STATE_SEND_MKV_HEADER        (3)
#define STREAM_STATE_SEND_FRAME_DATA        (4)
#define STREAM_STATE_SEND_PACKAGE_ENDING    (5)

typedef struct
{
    char pDataEndpoint[KVS_DATA_ENDPOINT_MAX_SIZE];

    MkvHeader_t mkvHeader;

    uint32_t uFragmentLen;

    uint8_t *pMkvHeader;
    uint32_t uMkvHeaderLen;
    uint32_t uMkvHeaderSize;

    uint8_t trackId;
    uint8_t *pFrameData;
    uint32_t uFrameLen;

    uint8_t pFragmentBegin[ FRAGMENT_BEGIN_SIZE ];
    uint8_t pFragmentEnd[ FRAGMENT_END_SIZE ];
    uint8_t streamState;

    bool isFirstFrame;
    bool isKeyFrame;
    uint64_t frameTimestamp;
    uint64_t lastKeyFrameTimestamp;
} StreamData_t;

static int fileIndex = 0;

static uint8_t pFrameBuf[ MAX_VIDEO_FRAME_SIZE ];
static uint32_t uFrameBufSize = MAX_VIDEO_FRAME_SIZE;

static uint32_t getFileSize( char * pFilename )
{
    FILE * fp = NULL;
    size_t uFileSize = 0;

    if( pFilename == NULL )
    {
        printf( "Invalid parameter: pFilename\n" );
        return 0;
    }

    fp = fopen( pFilename, "rb" );
    if( fp == NULL )
    {
        printf("Cannot open file:%s\n", pFilename );
        return 0;
    }

    fseek( fp, 0L, SEEK_END );
    uFileSize = ftell( fp );
    fclose( fp );

    return uFileSize;
}

static uint32_t readFile( char * pFilename,
                          uint8_t * pBuf,
                          uint32_t uBufsize )
{
    FILE * fp = NULL;
    uint32_t uFileSize = 0;
    uint32_t uBytesRead = 0;

    if( pFilename == NULL )
    {
        printf( "Invalid parameter: pFilename\n" );
        return 0;
    }

    fp = fopen( pFilename, "rb" );
    if( fp == NULL )
    {
        printf( "Cannot open file:%s\n", pFilename );
        return 0;
    }

    fseek( fp, 0L, SEEK_END );
    uFileSize = ftell( fp );
    fseek( fp, 0L, SEEK_SET );

    if ( uFileSize > uBufsize )
    {
        printf( "Buffer size is not enough for filesize:%d\n", uFileSize );
        fclose( fp );
        return 0;
    }

    uBytesRead = fread( pBuf, 1, uFileSize, fp ) ;

    fclose( fp );

    return uBytesRead;
}

static int loadFrame( StreamData_t *pStreamData )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint64_t timestamp = 0;
    char * pFilename = NULL;
    int xFilenameSize = 0;
    uint32_t uFrameLen = 0;

    timestamp = getEpochTimestampInMs();
    if( timestamp - pStreamData->frameTimestamp < 1000 / VIDEO_FPS )
    {
        /* Sleep until next FPS timestamp */
        sleepInMs( ( 1000 / VIDEO_FPS ) - ( timestamp - pStreamData->frameTimestamp ) );
    }
    pStreamData->frameTimestamp = getEpochTimestampInMs();

    /* Load frame from file */
    xFilenameSize = snprintf( NULL, 0, "%s/" SAMPLE_VIDEO_FILENAME_FORMAT, SAMPLE_FRAME_ROOT_DIRECTORY, fileIndex + 1 ) + 1;
    pFilename = ( char * )malloc( xFilenameSize );
    if( pFilename == NULL )
    {
        printf("OOM: video frame filename\r\n");
        return KVS_STATUS_NOT_ENOUGH_MEMORY;
    }
    snprintf( pFilename, xFilenameSize , "%s/" SAMPLE_VIDEO_FILENAME_FORMAT, SAMPLE_FRAME_ROOT_DIRECTORY, fileIndex + 1 );
    uFrameLen = readFile( pFilename, pFrameBuf, uFrameBufSize );
    free( pFilename );

#ifdef H264_CONVERT_ANNEX_B_TO_AVCC
    Mkv_convertAnnexBtoAvccInPlace( pFrameBuf, uFrameLen, uFrameBufSize, &uFrameLen );
#endif
    printf("t:%lu\n", pStreamData->frameTimestamp);

    pStreamData->trackId = MKV_VIDEO_TRACK_NUMBER;
    pStreamData->pFrameData = pFrameBuf;
    pStreamData->uFrameLen = uFrameLen;
    pStreamData->isKeyFrame = ( fileIndex % DEFAULT_KEY_FRAME_INTERVAL ) == 0 ? true : false;
    if( pStreamData->isKeyFrame )
    {
        pStreamData->lastKeyFrameTimestamp = pStreamData->frameTimestamp;
    }

    fileIndex = ( fileIndex + 1 ) % H264_SAMPLE_FRAMES_SIZE;

    return retStatus;
}

static int initializeMkvHeader( StreamData_t *pStreamData )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    VideoTrackInfo_t videoTrackInfo = { 0 };
    uint8_t *pVideoCpdData = NULL;
    uint32_t uVideoCpdLen = 0;
    uint32_t uMaxMkvHeaderSize = 0;

    Mkv_generateH264CodecPrivateDataFromAvccNalus( pStreamData->pFrameData, pStreamData->uFrameLen, NULL, &uVideoCpdLen );
    pVideoCpdData = ( uint8_t * ) malloc(uVideoCpdLen );
    Mkv_generateH264CodecPrivateDataFromAvccNalus( pStreamData->pFrameData, pStreamData->uFrameLen, pVideoCpdData, &uVideoCpdLen );

    memset( &videoTrackInfo, 0, sizeof( VideoTrackInfo_t ) );
    videoTrackInfo.pTrackName = VIDEO_NAME;
    videoTrackInfo.pCodecName = VIDEO_CODEC_NAME;
    videoTrackInfo.uWidth = VIDEO_WIDTH;
    videoTrackInfo.uHeight = VIDEO_HEIGHT;
    videoTrackInfo.pCodecPrivate = pVideoCpdData;
    videoTrackInfo.uCodecPrivateLen = uVideoCpdLen;

    Mkv_initializeHeaders( &( pStreamData->mkvHeader ), &videoTrackInfo, NULL );

    Mkv_getMaxHeaderSize( &( pStreamData->mkvHeader ), &uMaxMkvHeaderSize );

    pStreamData->pMkvHeader = (uint8_t *) malloc( uMaxMkvHeaderSize );
    pStreamData->uMkvHeaderSize = uMaxMkvHeaderSize;

    pStreamData->isFirstFrame = true;

    free(pVideoCpdData );

    return retStatus;
}

static int updateFragmentData( StreamData_t *pStreamData )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint64_t timestamp = 0;
    uint16_t deltaTimestamp = 0;

    if( pStreamData->isKeyFrame )
    {
        timestamp = pStreamData->frameTimestamp;
        deltaTimestamp = 0;
    }
    else
    {
        timestamp = 0;
        deltaTimestamp = ( uint16_t )(pStreamData->frameTimestamp - pStreamData->lastKeyFrameTimestamp);
    }

    /* Update MKV headers */
    Mkv_updateTimestamp( &(pStreamData->mkvHeader), timestamp );
    Mkv_updateDeltaTimestamp( &(pStreamData->mkvHeader), MKV_VIDEO_TRACK_NUMBER, deltaTimestamp );
    Mkv_updateProperty( &(pStreamData->mkvHeader), MKV_VIDEO_TRACK_NUMBER, pStreamData->isKeyFrame );
    Mkv_updateFrameSize(&(pStreamData->mkvHeader), MKV_VIDEO_TRACK_NUMBER, pStreamData->uFrameLen);

    /* Get MKV header */
    Mkv_getMkvHeader( &(pStreamData->mkvHeader), pStreamData->isFirstFrame, pStreamData->isKeyFrame, pStreamData->trackId, pStreamData->pMkvHeader, pStreamData->uMkvHeaderSize, &( pStreamData->uMkvHeaderLen ) );

    /* Update fragment length */
    pStreamData->uFragmentLen = pStreamData->uMkvHeaderLen + pStreamData->uFrameLen;

    /* Update fragment begin */
    snprintf( ( char * )( pStreamData->pFragmentBegin ), FRAGMENT_BEGIN_SIZE, "%08x", pStreamData->uFragmentLen );
    pStreamData->pFragmentBegin[8] = '\r';
    pStreamData->pFragmentBegin[9] = '\n';

    return retStatus;
}

static int putMediaWantMoreDataCallback( uint8_t ** ppBuffer,
                                         uint32_t *pBufferLen,
                                         uint8_t * customData )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    StreamData_t *pStreamData = NULL;

    pStreamData = ( StreamData_t * )customData;

    if( pStreamData->streamState == STREAM_STATE_INIT )
    {
        loadFrame( pStreamData );
        initializeMkvHeader( pStreamData );
        pStreamData->streamState = STREAM_STATE_SEND_PACKAGE_HEADER;
    }
    else if( pStreamData->streamState == STREAM_STATE_LOAD_FRAME )
    {
        loadFrame( pStreamData );
        pStreamData->streamState = STREAM_STATE_SEND_PACKAGE_HEADER;
    }

    if ( pStreamData->streamState == STREAM_STATE_SEND_PACKAGE_HEADER )
    {
        updateFragmentData( pStreamData );
    }

    switch( pStreamData->streamState )
    {
        case STREAM_STATE_SEND_PACKAGE_HEADER:
        {
            *ppBuffer = pStreamData->pFragmentBegin;
            *pBufferLen = FRAGMENT_BEGIN_SIZE;
            pStreamData->streamState = STREAM_STATE_SEND_MKV_HEADER;
            break;
        }
        case STREAM_STATE_SEND_MKV_HEADER:
        {
            *ppBuffer = pStreamData->pMkvHeader;
            *pBufferLen = pStreamData->uMkvHeaderLen;

            pStreamData->streamState = STREAM_STATE_SEND_FRAME_DATA;
            break;
        }
        case STREAM_STATE_SEND_FRAME_DATA:
        {
            *ppBuffer = pStreamData->pFrameData;
            *pBufferLen = pStreamData->uFrameLen;
            pStreamData->streamState = STREAM_STATE_SEND_PACKAGE_ENDING;
            break;
        }
        case STREAM_STATE_SEND_PACKAGE_ENDING:
        {
            *ppBuffer = pStreamData->pFragmentEnd;
            *pBufferLen = FRAGMENT_END_SIZE;

            pStreamData->isFirstFrame = false;

            pStreamData->streamState = STREAM_STATE_LOAD_FRAME;
            break;
        }
        default:
        {
            retStatus = KVS_STATUS_REST_UNKNOWN_ERROR;
            break;
        }
    }

    return 0;
}

static int initializeStreamData( StreamData_t *pStreamData )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pStreamData == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pStreamData->uFragmentLen = 0;
        pStreamData->pMkvHeader = NULL;
        pStreamData->uMkvHeaderLen = 0;
        pStreamData->pFrameData = NULL;
        pStreamData->uFrameLen = 0;
        memset(pStreamData->pFragmentBegin, 0, FRAGMENT_BEGIN_SIZE );
        pStreamData->streamState = STREAM_STATE_INIT;
        pStreamData->isFirstFrame = false;
        pStreamData->isKeyFrame = false;
        pStreamData->frameTimestamp = 0;
        pStreamData->lastKeyFrameTimestamp = 0;
        pStreamData->pFragmentEnd[0] = '\r';
        pStreamData->pFragmentEnd[1] = '\n';
    }

    return retStatus;
}

int main()
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    StreamData_t *pStreamData = NULL;
    KvsServiceParameter_t serviceParameter;
#if USE_IOT_CERT
    IotCredentialRequest_t req = {
            .pCredentialHost = CREDENTIALS_HOST,
            .pRoleAlias = ROLE_ALIAS,
            .pThingName = THING_NAME,
            .pRootCA = ROOT_CA,
            .pCertificate = CERTIFICATE,
            .pPrivateKey = PRIVATE_KEY
    };
    IotCredentialToken_t token;
#endif

    srand( time(NULL ) );

#if USE_IOT_CERT
    retStatus = Iot_getCredential( &req, &token );

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        serviceParameter.pAccessKey = token.pAccessKeyId;
        serviceParameter.pSecretKey = token.pSecretAccessKey;
        serviceParameter.pToken = token.pSessionToken;
    }
#else
    serviceParameter.pAccessKey = AWS_ACCESS_KEY;
    serviceParameter.pSecretKey = AWS_SECRET_KEY;
    serviceParameter.pToken = NULL;
#endif

    serviceParameter.pRegion = AWS_KVS_REGION;
    serviceParameter.pService = AWS_KVS_SERVICE;
    serviceParameter.pHost = AWS_KVS_HOST;
    serviceParameter.pUserAgent = HTTP_AGENT;

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        pStreamData = ( StreamData_t * )malloc( sizeof( StreamData_t ) );
        if( pStreamData == NULL )
        {
            printf("failed to allocate kvs stream data\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        retStatus = initializeStreamData( pStreamData );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            printf( "failed to initialize stream data\n" );
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        printf( "try to describe stream\n" );
        retStatus = Kvs_describeStream( &serviceParameter, KVS_STREAM_NAME );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            printf( "failed to describe stream err(%d)\n", retStatus );
#if USE_IOT_CERT
            printf( "If we use IoT certification, then KVS stream has to be created before using it.\n" );
#else
            if( retStatus == KVS_STATUS_REST_RES_NOT_FOUND_ERROR )
            {
                printf( "try to create stream\n" );
                retStatus = Kvs_createStream( &serviceParameter, DEVICE_NAME, KVS_STREAM_NAME, MEDIA_TYPE, DATA_RETENTION_IN_HOURS );
                if( retStatus == KVS_STATUS_SUCCEEDED )
                {
                    printf( "Create stream successfully\n" );
                }
                else
                {
                    printf( "Failed to create stream err(%d)\n", retStatus );
                }
            }
#endif
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        printf( "try to get data endpoint\n" );
        retStatus = Kvs_getDataEndpoint( &serviceParameter, KVS_STREAM_NAME, pStreamData->pDataEndpoint, KVS_DATA_ENDPOINT_MAX_SIZE );
        if( retStatus == KVS_STATUS_SUCCEEDED )
        {
            printf( "Data endpoint: %s\n", pStreamData->pDataEndpoint );
        }
        else
        {
            printf( "Failed to get data endpoint err(%d)\n", retStatus );
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        /* "PUT MEDIA" RESTful API uses different endpoint */
        serviceParameter.pHost = pStreamData->pDataEndpoint;

        printf( "try to put media\n" );
        retStatus = Kvs_putMedia(&serviceParameter, KVS_STREAM_NAME, putMediaWantMoreDataCallback, pStreamData);
        if( retStatus == KVS_STATUS_SUCCEEDED )
        {
            printf( "put media success\n" );
        }
        else
        {
            printf( "Failed to put media %d\n", retStatus );
        }
    }

    if (pStreamData != NULL)
    {
        if (pStreamData->pMkvHeader != NULL)
        {
            free(pStreamData->pMkvHeader);
        }
        free(pStreamData);
    }

    return 0;
}
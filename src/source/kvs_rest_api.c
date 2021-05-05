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
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#include "kvs_errors.h"
#include "kvs_rest_api.h"
#include "aws_signer_v4.h"
#include "network_api.h"
#include "kvs_port.h"
#include "kvs_options.h"
#include "json_helper.h"
#include "http_helper.h"
#include "logger.h"


#include "parson.h"

/*-----------------------------------------------------------*/

#define KVS_ENDPOINT_TCP_PORT   "443"
#define HTTP_METHOD_POST        "POST"

/*-----------------------------------------------------------*/

#define KVS_URI_CREATE_STREAM       "/createStream"
#define KVS_URI_DESCRIBE_STREAM     "/describeStream"
#define KVS_URI_GET_DATA_ENDPOINT   "/getDataEndpoint"
#define KVS_URI_PUT_MEDIA           "/putMedia"

/*-----------------------------------------------------------*/

#define HDR_CONNECTION                  "connection"
#define HDR_HOST                        "host"
#define HDR_TRANSFER_ENCODING           "transfer-encoding"
#define HDR_USER_AGENT                  "user-agent"
#define HDR_X_AMZ_DATE                  "x-amz-date"
#define HDR_X_AMZ_SECURITY_TOKEN        "x-amz-security-token"
#define HDR_X_AMZN_FRAG_ACK_REQUIRED    "x-amzn-fragment-acknowledgment-required"
#define HDR_X_AMZN_FRAG_T_TYPE          "x-amzn-fragment-timecode-type"
#define HDR_X_AMZN_PRODUCER_START_T     "x-amzn-producer-start-timestamp"
#define HDR_X_AMZN_STREAM_NAME          "x-amzn-stream-name"

/*-----------------------------------------------------------*/

#define CREATE_STREAM_HTTP_BODY_TEMPLATE "{\"DeviceName\": \"%s\",\"StreamName\": \"%s\",\"MediaType\": \"%s\",\"DataRetentionInHours\": %d}"

#define DESCRIBE_STREAM_HTTP_BODY_TEMPLATE "{\n\t\"StreamName\": \"%s\"\n}"

#define GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE "{\"StreamName\": \"%s\",\"APIName\":\"PUT_MEDIA\"}"

#define MAX_STRLEN_OF_INT32_t   ( 11 )
#define MAX_STRLEN_OF_UINT32    ( 10 )

#define MAX_X_AMZN_PRODUCER_START_TIMESTAMP_LEN (32)

#define MIN_FRAGMENT_LENGTH     ( 6 )

/*-----------------------------------------------------------*/

#define JSON_KEY_EVENT_TYPE             "EventType"
#define JSON_KEY_FRAGMENT_TIMECODE      "FragmentTimecode"
#define JSON_KEY_ERROR_ID               "ErrorId"

#define EVENT_TYPE_BUFFERING    "\"BUFFERING\""
#define EVENT_TYPE_RECEIVED     "\"RECEIVED\""
#define EVENT_TYPE_PERSISTED    "\"PERSISTED\""
#define EVENT_TYPE_ERROR        "\"ERROR\""
#define EVENT_TYPE_IDLE         "\"IDLE\""

typedef enum {
    EVENT_UNKNOWN = 0,
    EVENT_BUFFERING,
    EVENT_RECEIVED,
    EVENT_PERSISTED,
    EVENT_ERROR,
    EVENT_IDLE
} EVENT_TYPE;

typedef struct
{
    EVENT_TYPE eventType;
    uint64_t uFragmentTimecode;
    int32_t uErrorId;
} FragmentAck_t;

/*-----------------------------------------------------------*/

static const int32_t putMediaErrorIdMap[][2] = {
        { 4000, KVS_STATUS_PUT_MEDIA_STREAM_READ_ERROR },
        { 4001, KVS_STATUS_PUT_MEDIA_MAX_FRAGMENT_SIZE_REACHED },
        { 4002, KVS_STATUS_PUT_MEDIA_MAX_FRAGMENT_DURATION_REACHED },
        { 4003, KVS_STATUS_PUT_MEDIA_MAX_CONNECTION_DURATION_REACHED },
        { 4004, KVS_STATUS_PUT_MEDIA_FRAGMENT_TIMECODE_LESSER_THAN_PREVIOUS },
        { 4005, KVS_STATUS_PUT_MEDIA_MORE_THAN_ALLOWED_TRACKS_FOUND },
        { 4006, KVS_STATUS_PUT_MEDIA_INVALID_MKV_DATA },
        { 4007, KVS_STATUS_PUT_MEDIA_INVALID_PRODUCER_TIMESTAMP },
        { 4008, KVS_STATUS_PUT_MEDIA_STREAM_NOT_ACTIVE },
        { 4009, KVS_STATUS_PUT_MEDIA_FRAGMENT_METADATA_LIMIT_REACHED },
        { 4010, KVS_STATUS_PUT_MEDIA_TRACK_NUMBER_MISMATCH },
        { 4011, KVS_STATUS_PUT_MEDIA_FRAMES_MISSING_FOR_TRACK },
        { 4500, KVS_STATUS_PUT_MEDIA_KMS_KEY_ACCESS_DENIED },
        { 4501, KVS_STATUS_PUT_MEDIA_KMS_KEY_DISABLED },
        { 4502, KVS_STATUS_PUT_MEDIA_KMS_KEY_VALIDATION_ERROR },
        { 4503, KVS_STATUS_PUT_MEDIA_KMS_KEY_UNAVAILABLE },
        { 4504, KVS_STATUS_PUT_MEDIA_KMS_KEY_INVALID_USAGE },
        { 4505, KVS_STATUS_PUT_MEDIA_KMS_KEY_INVALID_STATE },
        { 4506, KVS_STATUS_PUT_MEDIA_KMS_KEY_NOT_FOUND },
        { 5000, KVS_STATUS_PUT_MEDIA_INTERNAL_ERROR },
        { 5001, KVS_STATUS_PUT_MEDIA_ARCHIVAL_ERROR },
        { 0, KVS_STATUS_PUT_MEDIA_UNKNOWN_ERROR}
};

/*-----------------------------------------------------------*/

static int32_t parseDataEndpoint( const char * pJsonSrc,
                                  uint32_t uJsonSrcLen,
                                  char * pBuf,
                                  uint32_t uBufsize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    char *pJson = NULL;
    JSON_Value * rootValue = NULL;
    JSON_Object * rootObject = NULL;
    JSON_Value * jsonDataEndpoint = NULL;
    char * pDataEndpoint = NULL;
    uint32_t uEndpointLen = 0;

    do
    {
        if(pJsonSrc == NULL || pBuf == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        pJson = (char *) sysMalloc( uJsonSrcLen + 1 );
        if(pJson == NULL )
        {
            LOG_ERROR("OOM: pJson\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        memcpy( pJson, pJsonSrc, uJsonSrcLen );
        pJson[ uJsonSrcLen ] = '\0';

        json_set_escape_slashes( 0 );

        rootValue = json_parse_string( pJson );
        if( rootValue == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }

        rootObject = json_value_get_object( rootValue );
        if ( rootObject == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }

        pDataEndpoint = json_object_dotget_serialize_to_string( rootObject, "DataEndpoint", true );
        if( pDataEndpoint == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }
        else
        {
            uEndpointLen = strlen( pDataEndpoint );
            if( uBufsize >= uEndpointLen )
            {
                sprintf( pBuf, "%.*s", uEndpointLen - 8, pDataEndpoint + 8 );
                retStatus = KVS_STATUS_SUCCEEDED;
            }
            sysFree( pDataEndpoint );
        }
    } while ( 0 );

    if( rootValue != NULL )
    {
        json_value_free( rootValue );
    }

    if( pJson != NULL )
    {
        sysFree(pJson );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

/**
 * @brief Parse Fragment and get it's length
 *
 * @param[in] pFragmentSrc Fragment source
 * @param[in] uFragmentSrcLen Fragment length
 * @param[out] puMsgLen message length
 * @param[out] puMsgBytesCnt bytes used to get message
 *
 * @return KVS error code
 */
static int32_t parseFragmentAckLength( char * pFragmentSrc,
                                       uint32_t uFragmentSrcLen,
                                       uint32_t * puMsgLen,
                                       uint32_t * puMsgBytesCnt )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    int i = 0;
    char c = 0;
    uint32_t uMsgLen = 0;
    uint32_t uMsgBytesCnt = 0;

    if( pFragmentSrc == NULL || uFragmentSrcLen < MIN_FRAGMENT_LENGTH )
    {
        retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
    }
    else
    {
        for( i = 0; i < uFragmentSrcLen - 1; i++ )
        {
            c = toupper( pFragmentSrc[ i ] );
            if( isxdigit( c ) )
            {
                /* Nothing to do here. Just fall through. */
            }
            else if( c == '\r' )
            {
                if( pFragmentSrc[ i + 1 ] == '\n' )
                {
                    /* We Found a valid format of length field.*/
                    if( i > 8 )
                    {
                        /* we cannot handle fragment length larger than 2^32 */
                        retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
                        break;
                    }
                    else
                    {
                        pFragmentSrc[ i ] = '\0';
                        uMsgLen = strtoul( pFragmentSrc, NULL, 16 );
                        pFragmentSrc[ i ] = '\r';
                        uMsgBytesCnt = i + 2;
                    }
                    break;
                }
            }
            else
            {
                retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
                break;
            }
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( puMsgLen != NULL && puMsgBytesCnt != NULL )
        {
            *puMsgLen = uMsgLen;
            *puMsgBytesCnt = uMsgBytesCnt;
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

static EVENT_TYPE getEventType( char * pEventType )
{
    EVENT_TYPE ev = EVENT_UNKNOWN;

    if( pEventType != NULL )
    {
        if( strncmp( pEventType, EVENT_TYPE_BUFFERING, sizeof( EVENT_TYPE_BUFFERING ) - 1 ) == 0 )
        {
            ev = EVENT_BUFFERING;
        }
        else if( strncmp( pEventType, EVENT_TYPE_RECEIVED, sizeof( EVENT_TYPE_RECEIVED ) - 1 ) == 0 )
        {
            ev = EVENT_RECEIVED;
        }
        else if ( strncmp( pEventType, EVENT_TYPE_PERSISTED, sizeof( EVENT_TYPE_PERSISTED ) - 1 ) == 0 )
        {
            ev = EVENT_PERSISTED;
        }
        else if( strncmp( pEventType, EVENT_TYPE_ERROR, sizeof( EVENT_TYPE_ERROR ) - 1 ) == 0 )
        {
            ev = EVENT_ERROR;
        }
        else if( strncmp( pEventType, EVENT_TYPE_IDLE, sizeof( EVENT_TYPE_IDLE ) - 1 ) == 0 )
        {
            ev = EVENT_IDLE;
        }
    }

    return ev;
}

/*-----------------------------------------------------------*/

static int32_t getPutMediaStatus( uint32_t uErrorId )
{
    int32_t retStatus = KVS_STATUS_PUT_MEDIA_UNKNOWN_ERROR;
    int i = 0;

    for ( i = 0; putMediaErrorIdMap[ i ][ 0 ] != 0; i++ )
    {
        if( uErrorId == putMediaErrorIdMap[ i ][ 0 ] )
        {
            retStatus = putMediaErrorIdMap[ i ][ 1 ];
            break;
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

/**
 * @brief Parse fragment message of JSON format
 *
 * @param[in] pFragmentMsg JSON format message
 * @param[in] uFragmentMsgLen fragment length
 * @param[out] pFragmentAck
 *
 * @return KVS error code
 */
static int32_t parseFragmentMsg( char * pFragmentMsg,
                                 uint32_t uFragmentMsgLen,
                                 FragmentAck_t * pFragmentAck )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    JSON_Value * rootValue = NULL;
    JSON_Object * rootObject = NULL;
    char * pEventType = NULL;

    json_set_escape_slashes( 0 );

    do
    {
        if( pFragmentMsg == NULL || uFragmentMsgLen == 0 || pFragmentAck == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        rootValue = json_parse_string( pFragmentMsg );
        if( rootValue == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }

        rootObject = json_value_get_object( rootValue );
        if ( rootObject == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }

        pEventType = json_object_dotget_serialize_to_string( rootObject, JSON_KEY_EVENT_TYPE, false );
        if( pEventType != NULL )
        {
            pFragmentAck->eventType = getEventType( pEventType );
            sysFree( pEventType );

            if( pFragmentAck->eventType == EVENT_BUFFERING ||
                    pFragmentAck->eventType == EVENT_RECEIVED ||
                    pFragmentAck->eventType == EVENT_PERSISTED ||
                    pFragmentAck->eventType == EVENT_ERROR )
            {
                pFragmentAck->uFragmentTimecode = json_object_dotget_uint64( rootObject, JSON_KEY_FRAGMENT_TIMECODE, 10);
                if( pFragmentAck->eventType == EVENT_ERROR )
                {
                    pFragmentAck->uErrorId = ( int32_t )json_object_dotget_uint64( rootObject, JSON_KEY_ERROR_ID, 10);
                }
            }
        }
        else
        {
            LOG_WARN("unknown fragment ack:%s\r\n", pFragmentMsg);
            retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
        }
    } while ( false );

    if( rootValue != NULL )
    {
        json_value_free( rootValue );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

static int32_t parseFragmentAck( char * pFragmentSrc,
                                 uint32_t uFragmentSrcLen,
                                 FragmentAck_t * pFragmentAck)
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint32_t uMsgLen = 0;
    uint32_t uMsgBytesCnt = 0;

    do
    {
        if( pFragmentSrc == NULL || uFragmentSrcLen == 0 || pFragmentAck == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        if( ( retStatus = parseFragmentAckLength( pFragmentSrc, uFragmentSrcLen, &uMsgLen, &uMsgBytesCnt ) ) != KVS_STATUS_SUCCEEDED )
        {
            retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
            break;
        }
        else if( uMsgLen + uMsgBytesCnt + 2 != uFragmentSrcLen )
        {
            retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
            break;
        }
        else if( pFragmentSrc[ uMsgBytesCnt + uMsgLen ] != '\r' || pFragmentSrc[ uMsgBytesCnt + uMsgLen + 1 ] != '\n' )
        {
            retStatus = KVS_STATUS_FRAGMENT_INVALID_FORMAT;
            break;
        }
        else
        {
            /* We found a valid format of message here.*/
            pFragmentSrc[ uMsgBytesCnt + uMsgLen ] = '\0';
            retStatus = parseFragmentMsg( pFragmentSrc + uMsgBytesCnt, uMsgLen, pFragmentAck );
            pFragmentSrc[ uMsgBytesCnt + uMsgLen ] = '\r';
        }
    } while( false );

    return retStatus;
}

/*-----------------------------------------------------------*/

static void logFragmentAck( FragmentAck_t * pFragmentAck )
{
    if( pFragmentAck != NULL )
    {
        if( pFragmentAck->eventType == EVENT_BUFFERING )
        {
            LOG_INFO("Fragment buffering, timecode:%" PRIu64 "\r\n", pFragmentAck->uFragmentTimecode);
        }
        else if( pFragmentAck->eventType == EVENT_RECEIVED )
        {
            LOG_INFO("Fragment received, timecode:%" PRIu64 "\r\n", pFragmentAck->uFragmentTimecode);
        }
        else if( pFragmentAck->eventType == EVENT_PERSISTED )
        {
            LOG_INFO("Fragment persisted, timecode:%" PRIu64 "\r\n", pFragmentAck->uFragmentTimecode);
        }
        else if( pFragmentAck->eventType == EVENT_ERROR )
        {
            LOG_ERROR("PutMedia session error id:%d\r\n", pFragmentAck->uErrorId);
        }
        else if( pFragmentAck->eventType == EVENT_IDLE )
        {
            LOG_WARN("PutMedia session Idle\r\n");
        }
        else
        {
            LOG_WARN("Unknown Fragment Ack\r\n");
        }
    }
}

/*-----------------------------------------------------------*/

static int32_t checkServiceParameter( KvsServiceParameter_t * pServiceParameter )
{
    if( pServiceParameter->pAccessKey == NULL ||
        pServiceParameter->pSecretKey == NULL ||
        pServiceParameter->pRegion == NULL ||
        pServiceParameter->pService == NULL ||
        pServiceParameter->pHost == NULL ||
        pServiceParameter->pUserAgent == NULL )
    {
        return KVS_STATUS_INVALID_ARG;
    }
    else
    {
        return KVS_STATUS_SUCCEEDED;
    }
}

/*-----------------------------------------------------------*/

int Kvs_createStream( KvsServiceParameter_t * pServiceParameter,
                      char * pDeviceName,
                      char * pStreamName,
                      char * pMediaType,
                      uint32_t xDataRetentionInHours )
{
    int retStatus = KVS_STATUS_SUCCEEDED;
    char *p = NULL;

    /* Variables for network connection */
    NetworkContext_t *pNetworkContext = NULL;
    size_t uConnectionRetryCnt = 0;
    uint32_t uBytesToSend = 0;

    /* Variables for AWS signer V4 */
    AwsSignerV4Context_t signerContext = { 0 };
    char pXAmzDate[ SIGNATURE_DATE_TIME_STRING_LEN ];

    /* Variables for HTTP request */
    char *pHttpParameter = "";
    char *pHttpBody = NULL;
    size_t uHttpBodyLen = 0;

    uint32_t uHttpStatusCode = 0;

    do
    {
        if( checkServiceParameter( pServiceParameter ) != KVS_STATUS_SUCCEEDED )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }
        else if( pDeviceName == NULL || pStreamName == NULL || pMediaType == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        pHttpBody = ( char * )sysMalloc( sizeof( CREATE_STREAM_HTTP_BODY_TEMPLATE ) + strlen( pDeviceName ) +
                strlen( pStreamName ) + strlen( pMediaType ) + MAX_STRLEN_OF_UINT32 + 1 );
        if( pHttpBody == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        /* generate HTTP request body */
        uHttpBodyLen = sprintf( pHttpBody, CREATE_STREAM_HTTP_BODY_TEMPLATE, pDeviceName, pStreamName, pMediaType, xDataRetentionInHours );

        /* generate UTC time in x-amz-date formate */
        retStatus = getTimeInIso8601( pXAmzDate, sizeof( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Create canonical request and sign the request. */
        retStatus = AwsSignerV4_initContext( &signerContext, AWS_SIGNER_V4_BUFFER_SIZE );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_initCanonicalRequest( &signerContext, HTTP_METHOD_POST, sizeof( HTTP_METHOD_POST ) - 1,
                                                      KVS_URI_CREATE_STREAM, sizeof( KVS_URI_CREATE_STREAM ) - 1,
                                                      pHttpParameter, strlen( pHttpParameter ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_HOST, sizeof( HDR_HOST ) - 1,
                                                    pServiceParameter->pHost, strlen( pServiceParameter->pHost ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_USER_AGENT, sizeof( HDR_USER_AGENT ) - 1,
                                                    pServiceParameter->pUserAgent, strlen( pServiceParameter->pUserAgent ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_DATE, sizeof( HDR_X_AMZ_DATE ) - 1,
                                                    pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalBody( &signerContext, ( uint8_t * )pHttpBody, uHttpBodyLen);
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_sign( &signerContext, pServiceParameter->pSecretKey, strlen( pServiceParameter->pSecretKey ),
                                      pServiceParameter->pRegion, strlen( pServiceParameter->pRegion ),
                                      pServiceParameter->pService, strlen( pServiceParameter->pService ),
                                      pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Initialize and generate HTTP request, then send it. */
        pNetworkContext = ( NetworkContext_t * ) sysMalloc( sizeof( NetworkContext_t ) );
        if( pNetworkContext == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        if( ( retStatus = initNetworkContext( pNetworkContext ) ) != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        for( uConnectionRetryCnt = 0; uConnectionRetryCnt < MAX_CONNECTION_RETRY; uConnectionRetryCnt++ )
        {
            if( ( retStatus = connectToServer( pNetworkContext, pServiceParameter->pHost, KVS_ENDPOINT_TCP_PORT ) ) == KVS_STATUS_SUCCEEDED )
            {
                break;
            }
            sleepInMs( CONNECTION_RETRY_INTERVAL_IN_MS );
        }

        p = ( char * )( pNetworkContext->pHttpSendBuffer );
        p += sprintf( p, "%s %s HTTP/1.1\r\n", HTTP_METHOD_POST, KVS_URI_CREATE_STREAM );
        p += sprintf( p, "Host: %s\r\n", pServiceParameter->pHost );
        p += sprintf( p, "Accept: */*\r\n" );
        p += sprintf( p, "Authorization: %s Credential=%s/%s, SignedHeaders=%s, Signature=%s\r\n",
                      AWS_SIG_V4_ALGORITHM, pServiceParameter->pAccessKey, AwsSignerV4_getScope( &signerContext ),
                      AwsSignerV4_getSignedHeader( &signerContext ), AwsSignerV4_getHmacEncoded( &signerContext ) );
        p += sprintf( p, "content-length: %u\r\n", (unsigned int) uHttpBodyLen );
        p += sprintf( p, "content-type: application/json\r\n" );
        p += sprintf( p, HDR_USER_AGENT ": %s\r\n", pServiceParameter->pUserAgent );
        p += sprintf( p, HDR_X_AMZ_DATE ": %s\r\n", pXAmzDate );
        p += sprintf( p, "\r\n" );
        p += sprintf( p, "%s", pHttpBody );

        AwsSignerV4_terminateContext(&signerContext);

        uBytesToSend = p - ( char * )pNetworkContext->pHttpSendBuffer;
        retStatus = networkSend( pNetworkContext, pNetworkContext->pHttpSendBuffer, uBytesToSend );
        if( retStatus != uBytesToSend )
        {
            retStatus = KVS_STATUS_NETWORK_SEND_ERROR;
            break;
        }

        retStatus = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen );
        if( retStatus < KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = parseHttpResponse( ( char * )pNetworkContext->pHttpRecvBuffer, ( uint32_t )retStatus );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        uHttpStatusCode = getLastHttpStatusCode();

        /* Check HTTP results */
        if( uHttpStatusCode == 200 )
        {
            retStatus = KVS_STATUS_SUCCEEDED;
            /* We got a success response here. */
        }
        else if( uHttpStatusCode == 400 )
        {
            retStatus = KVS_STATUS_REST_EXCEPTION_ERROR;
            break;
        }
        else
        {
            retStatus = KVS_STATUS_REST_UNKNOWN_ERROR;
            break;
        }
    } while ( 0 );

    if( pNetworkContext != NULL )
    {
        disconnectFromServer( pNetworkContext );
        terminateNetworkContext(pNetworkContext);
        sysFree( pNetworkContext );
        AwsSignerV4_terminateContext(&signerContext);
    }

    if( pHttpBody != NULL )
    {
        sysFree( pHttpBody );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int Kvs_describeStream( KvsServiceParameter_t * pServiceParameter,
                        char * pStreamName )
{
    int retStatus = KVS_STATUS_SUCCEEDED;
    char *p = NULL;
    bool bUseIotCert = false;

    /* Variables for network connection */
    NetworkContext_t *pNetworkContext = NULL;
    size_t uConnectionRetryCnt = 0;
    uint32_t uBytesToSend = 0;

    /* Variables for AWS signer V4 */
    AwsSignerV4Context_t signerContext;
    char pXAmzDate[SIGNATURE_DATE_TIME_STRING_LEN];

    /* Variables for HTTP request */
    char *pHttpParameter = "";
    char *pHttpBody = NULL;
    uint32_t uHttpBodyLen = 0;

    uint32_t uHttpStatusCode = 0;

    do
    {
        if( checkServiceParameter( pServiceParameter ) != KVS_STATUS_SUCCEEDED )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }
        else if( pStreamName == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        if( pServiceParameter->pToken != NULL )
        {
            bUseIotCert = true;
        }

        pHttpBody = (char *) sysMalloc( sizeof( DESCRIBE_STREAM_HTTP_BODY_TEMPLATE ) + strlen( pStreamName ) + 1 );
        if( pHttpBody == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        /* generate HTTP request body */
        uHttpBodyLen = sprintf( pHttpBody, DESCRIBE_STREAM_HTTP_BODY_TEMPLATE, pStreamName );

        /* generate UTC time in x-amz-date formate */
        retStatus = getTimeInIso8601( pXAmzDate, sizeof( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Create canonical request and sign the request. */
        retStatus = AwsSignerV4_initContext( &signerContext, AWS_SIGNER_V4_BUFFER_SIZE );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_initCanonicalRequest( &signerContext, HTTP_METHOD_POST, sizeof( HTTP_METHOD_POST ) - 1,
                                                      KVS_URI_DESCRIBE_STREAM, sizeof( KVS_URI_DESCRIBE_STREAM ) - 1,
                                                      pHttpParameter, strlen( pHttpParameter ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_HOST, sizeof( HDR_HOST ) - 1,
                                                    pServiceParameter->pHost, strlen( pServiceParameter->pHost ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_USER_AGENT, sizeof( HDR_USER_AGENT ) - 1,
                                                    pServiceParameter->pUserAgent, strlen( pServiceParameter->pUserAgent ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_DATE, sizeof( HDR_X_AMZ_DATE ) - 1,
                                                    pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        if( bUseIotCert )
        {
            retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_SECURITY_TOKEN, sizeof( HDR_X_AMZ_SECURITY_TOKEN ) - 1,
                                                        pServiceParameter->pToken, strlen( pServiceParameter->pToken ) );
            if( retStatus != KVS_STATUS_SUCCEEDED )
            {
                break;
            }
        }

        retStatus = AwsSignerV4_addCanonicalBody( &signerContext, ( uint8_t * )pHttpBody, uHttpBodyLen );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_sign( &signerContext, pServiceParameter->pSecretKey, strlen( pServiceParameter->pSecretKey ),
                                      pServiceParameter->pRegion, strlen( pServiceParameter->pRegion ),
                                      pServiceParameter->pService, strlen( pServiceParameter->pService ),
                                      pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Initialize and generate HTTP request, then send it. */
        pNetworkContext = ( NetworkContext_t * ) sysMalloc( sizeof( NetworkContext_t ) );
        if( pNetworkContext == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        if( ( retStatus = initNetworkContext( pNetworkContext ) ) != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        for( uConnectionRetryCnt = 0; uConnectionRetryCnt < MAX_CONNECTION_RETRY; uConnectionRetryCnt++ )
        {
            if( ( retStatus = connectToServer( pNetworkContext, pServiceParameter->pHost, KVS_ENDPOINT_TCP_PORT ) ) == KVS_STATUS_SUCCEEDED )
            {
                break;
            }
            sleepInMs( CONNECTION_RETRY_INTERVAL_IN_MS );
        }

        p = (char *)(pNetworkContext->pHttpSendBuffer);
        p += sprintf(p, "%s %s HTTP/1.1\r\n", HTTP_METHOD_POST, KVS_URI_DESCRIBE_STREAM);
        p += sprintf(p, "Host: %s\r\n", pServiceParameter->pHost);
        p += sprintf(p, "Accept: */*\r\n");
        p += sprintf(p, "Authorization: %s Credential=%s/%s, SignedHeaders=%s, Signature=%s\r\n",
                     AWS_SIG_V4_ALGORITHM, pServiceParameter->pAccessKey, AwsSignerV4_getScope( &signerContext ),
                     AwsSignerV4_getSignedHeader( &signerContext ), AwsSignerV4_getHmacEncoded( &signerContext ) );
        p += sprintf(p, "content-length: %u\r\n", (unsigned int) uHttpBodyLen );
        p += sprintf(p, "content-type: application/json\r\n" );
        p += sprintf(p, HDR_USER_AGENT ": %s\r\n", pServiceParameter->pUserAgent );
        p += sprintf(p, HDR_X_AMZ_DATE ": %s\r\n", pXAmzDate );
        if( bUseIotCert )
        {
            p += sprintf(p, HDR_X_AMZ_SECURITY_TOKEN ": %s\r\n", pServiceParameter->pToken );
        }
        p += sprintf(p, "\r\n" );
        p += sprintf(p, "%s", pHttpBody );

        AwsSignerV4_terminateContext(&signerContext);

        uBytesToSend = p - ( char * )pNetworkContext->pHttpSendBuffer;
        retStatus = networkSend( pNetworkContext, pNetworkContext->pHttpSendBuffer, uBytesToSend );
        if( retStatus != uBytesToSend )
        {
            retStatus = KVS_STATUS_NETWORK_SEND_ERROR;
            break;
        }

        retStatus = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen );
        if( retStatus < KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = parseHttpResponse( ( char * )pNetworkContext->pHttpRecvBuffer, ( uint32_t )retStatus );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        uHttpStatusCode = getLastHttpStatusCode();

        /* Check HTTP results */
        if( uHttpStatusCode == 200 )
        {
            retStatus = KVS_STATUS_SUCCEEDED;
            /* We got a success response here. */
        }
        else if( uHttpStatusCode == 404 )
        {
            retStatus = KVS_STATUS_REST_RES_NOT_FOUND_ERROR;
            break;
        }
        else
        {
            LOG_ERROR("Unable to describe stream:\r\n%.*s\r\n", (int)getLastHttpBodyLen(), getLastHttpBodyLoc());
            if( uHttpStatusCode == 400 )
            {
                retStatus = KVS_STATUS_REST_EXCEPTION_ERROR;
                break;
            }
            else if( uHttpStatusCode == 401 )
            {
                retStatus = KVS_STATUS_REST_NOT_AUTHORIZED_ERROR;
                break;
            }
            else
            {
                retStatus = KVS_STATUS_REST_UNKNOWN_ERROR;
                break;
            }
        }
    } while ( 0 );

    if( pNetworkContext != NULL )
    {
        disconnectFromServer( pNetworkContext );
        terminateNetworkContext(pNetworkContext);
        sysFree( pNetworkContext );
        AwsSignerV4_terminateContext(&signerContext);
    }

    if( pHttpBody != NULL )
    {
        sysFree( pHttpBody );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int Kvs_getDataEndpoint( KvsServiceParameter_t * pServiceParameter,
                         char * pStreamName,
                         char * pEndPointBuffer,
                         uint32_t uEndPointBufferLen )
{
    int retStatus = KVS_STATUS_SUCCEEDED;
    char *p = NULL;
    bool bUseIotCert = false;

    /* Variables for network connection */
    NetworkContext_t *pNetworkContext = NULL;
    size_t uConnectionRetryCnt = 0;
    uint32_t uBytesToSend = 0;

    /* Variables for AWS signer V4 */
    AwsSignerV4Context_t signerContext = { 0 };
    char pXAmzDate[ SIGNATURE_DATE_TIME_STRING_LEN ];

    /* Variables for HTTP request */
    char *pHttpParameter = "";
    char *pHttpBody = NULL;
    size_t uHttpBodyLen = 0;

    uint32_t uHttpStatusCode = 0;

    do
    {
        if( checkServiceParameter( pServiceParameter ) != KVS_STATUS_SUCCEEDED )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }
        else if( pStreamName == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        if( pServiceParameter->pToken != NULL )
        {
            bUseIotCert = true;
        }

        pHttpBody = ( char * ) sysMalloc ( sizeof( GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE ) + strlen( pStreamName ) + 1 );
        if( pHttpBody == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }
        uHttpBodyLen = sprintf( pHttpBody, GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE, pStreamName );

        /* generate UTC time in x-amz-date formate */
        retStatus = getTimeInIso8601( pXAmzDate, sizeof( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Create canonical request and sign the request. */
        retStatus = AwsSignerV4_initContext( &signerContext, AWS_SIGNER_V4_BUFFER_SIZE );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_initCanonicalRequest( &signerContext, HTTP_METHOD_POST, sizeof( HTTP_METHOD_POST ) - 1,
                                                      KVS_URI_GET_DATA_ENDPOINT, sizeof( KVS_URI_GET_DATA_ENDPOINT ) - 1,
                                                      pHttpParameter, strlen( pHttpParameter ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_HOST, sizeof( HDR_HOST ) - 1,
                                                    pServiceParameter->pHost, strlen( pServiceParameter->pHost ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_USER_AGENT, sizeof( HDR_USER_AGENT ) - 1,
                                                    pServiceParameter->pUserAgent, strlen( pServiceParameter->pUserAgent ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_DATE, sizeof(HDR_X_AMZ_DATE) - 1,
                                                    pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        if( bUseIotCert )
        {
            retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_SECURITY_TOKEN, sizeof( HDR_X_AMZ_SECURITY_TOKEN ) - 1,
                                                        pServiceParameter->pToken, strlen( pServiceParameter->pToken ) );
            if( retStatus != KVS_STATUS_SUCCEEDED )
            {
                break;
            }
        }

        retStatus = AwsSignerV4_addCanonicalBody( &signerContext, ( uint8_t * )pHttpBody, uHttpBodyLen);
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_sign( &signerContext, pServiceParameter->pSecretKey, strlen( pServiceParameter->pSecretKey ),
                                      pServiceParameter->pRegion, strlen( pServiceParameter->pRegion ),
                                      pServiceParameter->pService, strlen( pServiceParameter->pService ),
                                      pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Initialize and generate HTTP request, then send it. */
        pNetworkContext = ( NetworkContext_t * ) sysMalloc( sizeof( NetworkContext_t ) );
        if( pNetworkContext == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        if( ( retStatus = initNetworkContext( pNetworkContext ) ) != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        for( uConnectionRetryCnt = 0; uConnectionRetryCnt < MAX_CONNECTION_RETRY; uConnectionRetryCnt++ )
        {
            if( ( retStatus = connectToServer( pNetworkContext, pServiceParameter->pHost, KVS_ENDPOINT_TCP_PORT ) ) == KVS_STATUS_SUCCEEDED )
            {
                break;
            }
            sleepInMs( CONNECTION_RETRY_INTERVAL_IN_MS );
        }

        p = ( char * )( pNetworkContext->pHttpSendBuffer );
        p += sprintf( p, "%s %s HTTP/1.1\r\n", HTTP_METHOD_POST, KVS_URI_GET_DATA_ENDPOINT );
        p += sprintf( p, "Host: %s\r\n", pServiceParameter->pHost);
        p += sprintf( p, "Accept: */*\r\n");
        p += sprintf( p, "Authorization: %s Credential=%s/%s, SignedHeaders=%s, Signature=%s\r\n",
                      AWS_SIG_V4_ALGORITHM, pServiceParameter->pAccessKey, AwsSignerV4_getScope( &signerContext ),
                      AwsSignerV4_getSignedHeader( &signerContext ), AwsSignerV4_getHmacEncoded( &signerContext ) );
        p += sprintf(p, "content-length: %u\r\n", ( unsigned int )uHttpBodyLen );
        p += sprintf(p, "content-type: application/json\r\n");
        p += sprintf(p, HDR_USER_AGENT ": %s\r\n", pServiceParameter->pUserAgent);
        p += sprintf(p, HDR_X_AMZ_DATE ": %s\r\n", pXAmzDate);
        if( bUseIotCert )
        {
            p += sprintf(p, HDR_X_AMZ_SECURITY_TOKEN ": %s\r\n", pServiceParameter->pToken );
        }
        p += sprintf(p, "\r\n");
        p += sprintf(p, "%s", pHttpBody);

        AwsSignerV4_terminateContext(&signerContext);

        uBytesToSend = p - ( char * )pNetworkContext->pHttpSendBuffer;
        retStatus = networkSend( pNetworkContext, pNetworkContext->pHttpSendBuffer, uBytesToSend );
        if( retStatus != uBytesToSend )
        {
            retStatus = KVS_STATUS_NETWORK_SEND_ERROR;
            break;
        }

        retStatus = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen );
        if( retStatus < KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = parseHttpResponse( ( char * )pNetworkContext->pHttpRecvBuffer, ( uint32_t )retStatus );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        uHttpStatusCode = getLastHttpStatusCode();

        /* Check HTTP results */
        if( uHttpStatusCode == 200 )
        {
            /* propagate the parse result on either success or fail */
            retStatus = parseDataEndpoint( ( const char * )getLastHttpBodyLoc(), getLastHttpBodyLen(), pEndPointBuffer, uEndPointBufferLen );
            if( retStatus != KVS_STATUS_SUCCEEDED )
            {
                LOG_ERROR("Unable to get data endpoint\r\n");
            }
        }
        else
        {
            LOG_ERROR("Unable to create stream:\r\n%.*s\r\n", ( int )getLastHttpBodyLen(), getLastHttpBodyLoc());
            if( uHttpStatusCode == 400 )
            {
                retStatus = KVS_STATUS_REST_EXCEPTION_ERROR;
            }
            else
            {
                retStatus = KVS_STATUS_REST_UNKNOWN_ERROR;
            }
        }
    } while ( 0 );

    if( pNetworkContext != NULL )
    {
        disconnectFromServer( pNetworkContext );
        terminateNetworkContext(pNetworkContext);
        sysFree( pNetworkContext );
        AwsSignerV4_terminateContext(&signerContext);
    }

    if( pHttpBody != NULL )
    {
        sysFree( pHttpBody );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int Kvs_putMedia( KvsServiceParameter_t * pServiceParameter,
                  char * pStreamName,
                  PutMediaWantMoreDateCallbackFunc wantMoreCb,
                  void * pCustomData )
{
    int retStatus = KVS_STATUS_SUCCEEDED;
    char *p = NULL;
    int n = 0;
    bool bUseIotCert = false;

    /* Variables for network pConnection */
    NetworkContext_t *pNetworkContext = NULL;
    size_t uConnectionRetryCnt = 0;
    uint32_t uBytesToSend = 0;

    /* Variables for AWS signer V4 */
    AwsSignerV4Context_t signerContext = { 0 };
    char pXAmzDate[ SIGNATURE_DATE_TIME_STRING_LEN ];

    /* Variables for HTTP request */
    char *pHttpParameter = "";
    char pXAmznProducerStartTimestamp[ MAX_X_AMZN_PRODUCER_START_TIMESTAMP_LEN ];
    char *pConnection = "keep-alive";
    char *pTransferEncoding = "chunked";
    char *pXAmznFragmentAcknowledgmentRequired = "1";
    char *pXAmznFragmentTimecodeType = "ABSOLUTE";
    char *pHttpBody = "";

    uint32_t uHttpStatusCode = 0;

    /* Variables for PUT MEDIA RESTful call */
    uint8_t *pDataBuf = NULL;
    uint32_t uDataLen = 0;
    int32_t xDataReceived = 0;
    int32_t xDataSent = 0;
    FragmentAck_t fragmentAck;

    do
    {
        if( checkServiceParameter( pServiceParameter ) != KVS_STATUS_SUCCEEDED )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }
        else if( pStreamName == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        if( pServiceParameter->pToken != NULL )
        {
            bUseIotCert = true;
        }

        /* generate UTC time in x-amz-date formate */
        retStatus = getTimeInIso8601( pXAmzDate, sizeof( pXAmzDate ) );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Get the epoch time as the value of x-amzn-producer-start-timestamp */
        snprintf( pXAmznProducerStartTimestamp, MAX_X_AMZN_PRODUCER_START_TIMESTAMP_LEN, "%" PRIu64, getEpochTimestampInMs() / 1000 );

        /* Create canonical request and sign the request. */
        retStatus = AwsSignerV4_initContext( &signerContext, AWS_SIGNER_V4_BUFFER_SIZE );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Sign */
        retStatus = AwsSignerV4_initCanonicalRequest( &signerContext, HTTP_METHOD_POST, sizeof( HTTP_METHOD_POST ) - 1,
                                                      KVS_URI_PUT_MEDIA, sizeof( KVS_URI_PUT_MEDIA ) - 1,
                                                      pHttpParameter, strlen( pHttpParameter ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_CONNECTION, sizeof( HDR_CONNECTION ) - 1,
                                                    pConnection, strlen(pConnection ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_HOST, sizeof( HDR_HOST ) - 1,
                                                    pServiceParameter->pHost, strlen(pServiceParameter->pHost));
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_TRANSFER_ENCODING, sizeof( HDR_TRANSFER_ENCODING ) - 1,
                                                    pTransferEncoding, strlen(pTransferEncoding));
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_USER_AGENT, sizeof( HDR_USER_AGENT ) - 1,
                                                    pServiceParameter->pUserAgent, strlen( pServiceParameter->pUserAgent ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_DATE, sizeof( HDR_X_AMZ_DATE ) - 1,
                                                    pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        if( bUseIotCert )
        {
            retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZ_SECURITY_TOKEN, sizeof( HDR_X_AMZ_SECURITY_TOKEN ) - 1,
                                                        pServiceParameter->pToken, strlen( pServiceParameter->pToken ) );
            if( retStatus != KVS_STATUS_SUCCEEDED )
            {
                break;
            }
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZN_FRAG_ACK_REQUIRED, sizeof( HDR_X_AMZN_FRAG_ACK_REQUIRED ) - 1,
                                                    pXAmznFragmentAcknowledgmentRequired, strlen(pXAmznFragmentAcknowledgmentRequired ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZN_FRAG_T_TYPE, sizeof( HDR_X_AMZN_FRAG_T_TYPE ),
                                                    pXAmznFragmentTimecodeType, strlen( pXAmznFragmentTimecodeType ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZN_PRODUCER_START_T, sizeof( HDR_X_AMZN_PRODUCER_START_T ) - 1,
                                                    pXAmznProducerStartTimestamp, strlen( pXAmznProducerStartTimestamp ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalHeader( &signerContext, HDR_X_AMZN_STREAM_NAME, sizeof( HDR_X_AMZN_STREAM_NAME ) - 1,
                                                    pStreamName, strlen( pStreamName ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_addCanonicalBody( &signerContext, ( uint8_t * )pHttpBody, strlen( pHttpBody ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = AwsSignerV4_sign( &signerContext, pServiceParameter->pSecretKey, strlen( pServiceParameter->pSecretKey ),
                                      pServiceParameter->pRegion, strlen( pServiceParameter->pRegion ),
                                      pServiceParameter->pService, strlen( pServiceParameter->pService ),
                                      pXAmzDate, strlen( pXAmzDate ) );
        if( retStatus  != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        /* Initialize and generate HTTP request, then send it. */
        pNetworkContext = ( NetworkContext_t * ) sysMalloc( sizeof( NetworkContext_t ) );
        if( pNetworkContext == NULL )
        {
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
            break;
        }

        if( ( retStatus = initNetworkContext( pNetworkContext ) ) != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        for( uConnectionRetryCnt = 0; uConnectionRetryCnt < MAX_CONNECTION_RETRY; uConnectionRetryCnt++ )
        {
            if( ( retStatus = connectToServer( pNetworkContext, pServiceParameter->pHost, KVS_ENDPOINT_TCP_PORT ) ) == KVS_STATUS_SUCCEEDED )
            {
                break;
            }
            sleepInMs( CONNECTION_RETRY_INTERVAL_IN_MS );
        }

        p = (char *)pNetworkContext->pHttpSendBuffer;
        p += sprintf(p, "%s %s HTTP/1.1\r\n", HTTP_METHOD_POST, KVS_URI_PUT_MEDIA);
        p += sprintf(p, HDR_HOST ": %s\r\n", pServiceParameter->pHost);
        p += sprintf(p, "Accept: */*\r\n");
        p += sprintf(p, "Authorization: %s Credential=%s/%s, SignedHeaders=%s, Signature=%s\r\n",
                     AWS_SIG_V4_ALGORITHM,
                     pServiceParameter->pAccessKey,
                     AwsSignerV4_getScope( &signerContext ),
                     AwsSignerV4_getSignedHeader( &signerContext ),
                     AwsSignerV4_getHmacEncoded( &signerContext ) );
        p += sprintf(p, HDR_CONNECTION ": %s\r\n", pConnection);
        p += sprintf(p, "content-type: application/json\r\n");
        p += sprintf(p, HDR_TRANSFER_ENCODING ": %s\r\n", pTransferEncoding);
        p += sprintf(p, HDR_USER_AGENT ": %s\r\n", pServiceParameter->pUserAgent);
        p += sprintf(p, HDR_X_AMZ_DATE ": %s\r\n", pXAmzDate);
        if( bUseIotCert )
        {
            p += sprintf(p, HDR_X_AMZ_SECURITY_TOKEN ": %s\r\n", pServiceParameter->pToken );
        }
        p += sprintf(p, HDR_X_AMZN_FRAG_ACK_REQUIRED ": %s\r\n", pXAmznFragmentAcknowledgmentRequired);
        p += sprintf(p, HDR_X_AMZN_FRAG_T_TYPE ": %s\r\n", pXAmznFragmentTimecodeType);
        p += sprintf(p, HDR_X_AMZN_PRODUCER_START_T ": %s\r\n", pXAmznProducerStartTimestamp);
        p += sprintf(p, HDR_X_AMZN_STREAM_NAME ": %s\r\n", pStreamName);
        p += sprintf(p, "Expect: 100-continue\r\n");
        p += sprintf(p, "\r\n");
        p += sprintf(p, "%s", pHttpBody);

        AwsSignerV4_terminateContext(&signerContext);

        uBytesToSend = p - ( char * )pNetworkContext->pHttpSendBuffer;
        retStatus = networkSend( pNetworkContext, pNetworkContext->pHttpSendBuffer, uBytesToSend );
        if( retStatus != uBytesToSend )
        {
            retStatus = KVS_STATUS_NETWORK_SEND_ERROR;
            break;
        }

        retStatus = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen );
        if( retStatus < KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        retStatus = parseHttpResponse( ( char * )pNetworkContext->pHttpRecvBuffer, ( uint32_t )retStatus );
        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            break;
        }

        uHttpStatusCode = getLastHttpStatusCode();

        if ( uHttpStatusCode / 100 == 1 )
        {
            /* It's a HTTP 100 continue, we should try to receive next one. */
            retStatus = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen );
            if( retStatus < KVS_STATUS_SUCCEEDED )
            {
                break;
            }

            retStatus = parseHttpResponse( ( char * )pNetworkContext->pHttpRecvBuffer, ( uint32_t )retStatus );
            if( retStatus != KVS_STATUS_SUCCEEDED )
            {
                break;
            }

            uHttpStatusCode = getLastHttpStatusCode();
        }

        /* Check HTTP results */
        if( uHttpStatusCode == 200 )
        {
            /* We got a success response here. */

            /* Here is a streaming loop. For upstream, it calls a user callback to get more media data, then sends it out.
             * For downstream, it checks if there is any message from server side and then processes it. */
            while( true )
            {
                if ( isRecvDataAvailable( pNetworkContext ) == 0 )
                {
                    /* We reserve 1 byte for adding end of string for parsing and debugging. */
                    xDataReceived = networkRecv( pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen - 1 );
                    if ( xDataReceived > 0 )
                    {
                        *( ( char * )pNetworkContext->pHttpRecvBuffer + xDataReceived ) = '\0';

                        retStatus = parseFragmentAck( ( char * )( pNetworkContext->pHttpRecvBuffer ), xDataReceived, &fragmentAck );
                        if( retStatus == KVS_STATUS_SUCCEEDED )
                        {
                            logFragmentAck( &fragmentAck );
                            if( fragmentAck.eventType == EVENT_ERROR )
                            {
                                retStatus = getPutMediaStatus( fragmentAck.uErrorId );
                                break;
                            }
                        }
                        else
                        {
                            LOG_WARN("Unknown fragment ack: %s\r\n", pNetworkContext->pHttpRecvBuffer);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                n = wantMoreCb( &pDataBuf, &uDataLen, pCustomData );
                if ( uDataLen > 0 && pDataBuf != NULL )
                {
                    xDataSent = networkSend( pNetworkContext, pDataBuf, uDataLen );
                    if( xDataSent > 0 )
                    {

                    }
                    else
                    {
                        break;
                    }
                }
                else if ( uDataLen == 0 )
                {
                    // do some sleep;
                    sleepInMs( PUT_MEDIA_IDLE_INTERVAL_IN_MS );
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            LOG_ERROR("Unable to put media:\r\n%.*s\r\n", ( int )getLastHttpBodyLen(), getLastHttpBodyLoc());
            if( uHttpStatusCode == 400 )
            {
                retStatus = KVS_STATUS_REST_EXCEPTION_ERROR;
                break;
            }
            else if( uHttpStatusCode == 401 )
            {
                retStatus = KVS_STATUS_REST_NOT_AUTHORIZED_ERROR;
                break;
            }
            else if( uHttpStatusCode == 404 )
            {
                retStatus = KVS_STATUS_REST_RES_NOT_FOUND_ERROR;
                break;
            }
            else
            {
                retStatus = KVS_STATUS_REST_UNKNOWN_ERROR;
                break;
            }
        }
    } while ( 0 );

    if( pNetworkContext != NULL )
    {
        disconnectFromServer( pNetworkContext );
        terminateNetworkContext(pNetworkContext);
        sysFree( pNetworkContext );
        AwsSignerV4_terminateContext(&signerContext);
    }

    return retStatus;
}
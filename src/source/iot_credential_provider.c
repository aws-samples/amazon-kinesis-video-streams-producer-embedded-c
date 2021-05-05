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

#include "kvs_errors.h"
#include "network_api.h"
#include "iot_credential_provider.h"
#include "kvs_port.h"
#include "kvs_options.h"
#include "json_helper.h"
#include "http_helper.h"

#include "parson.h"

#define IOT_CREDENTIAL_PROVIDER_ENDPOINT_TCP_PORT   "443"
#define HTTP_METHOD_GET        "GET"

#define IOT_URI_ROLE_ALIASES_BEGIN  "/role-aliases"
#define IOT_URI_ROLE_ALIASES_END    "/credentials"

static int32_t parseIoTCredential( const char * pJsonSrc,
                                   uint32_t uJsonSrcLen,
                                   IotCredentialToken_t * pToken )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    char *pJson = NULL;
    JSON_Value * rootValue = NULL;
    JSON_Object * rootObject = NULL;

    do
    {
        if( pJsonSrc == NULL )
        {
            retStatus = KVS_STATUS_INVALID_ARG;
            break;
        }

        pJson = ( char * )sysMalloc( uJsonSrcLen + 1 );
        if( pJson == NULL )
        {
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

        pToken->pAccessKeyId = json_object_dotget_serialize_to_string( rootObject, "credentials.accessKeyId", true );
        pToken->pSecretAccessKey = json_object_dotget_serialize_to_string( rootObject, "credentials.secretAccessKey", true );
        pToken->pSessionToken = json_object_dotget_serialize_to_string( rootObject, "credentials.sessionToken", true );

        if( pToken->pAccessKeyId == NULL || pToken->pSecretAccessKey == NULL || pToken->pSessionToken == NULL )
        {
            retStatus = KVS_STATUS_JSON_PARSE_ERROR;
            break;
        }

    } while ( 0 );

    if( rootValue != NULL )
    {
        json_value_free( rootValue );
    }

    if( pJson != NULL )
    {
        sysFree( pJson );
    }

    return retStatus;
}

int32_t Iot_getCredential( IotCredentialRequest_t * pReq,
                           IotCredentialToken_t * pToken )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    char * p = NULL;

    NetworkContext_t * pNetworkContext = NULL;
    size_t uConnectionRetryCnt = 0;
    uint32_t uBytesToSend = 0;

    uint32_t uHttpStatusCode = 0;

    do
    {
        pNetworkContext = ( NetworkContext_t * )sysMalloc( sizeof( NetworkContext_t ) );
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
            if( ( retStatus = connectToServerWithX509Cert( pNetworkContext, pReq->pCredentialHost, IOT_CREDENTIAL_PROVIDER_ENDPOINT_TCP_PORT, pReq->pRootCA, pReq->pCertificate, pReq->pPrivateKey ) ) == KVS_STATUS_SUCCEEDED )
            {
                break;
            }
            sleepInMs( CONNECTION_RETRY_INTERVAL_IN_MS );
        }

        p = ( char * )( pNetworkContext->pHttpSendBuffer );
        p += sprintf( p, "%s %s/%s%s HTTP/1.1\r\n", HTTP_METHOD_GET, IOT_URI_ROLE_ALIASES_BEGIN, pReq->pRoleAlias, IOT_URI_ROLE_ALIASES_END );
        p += sprintf( p, "Host: %s\r\n", pReq->pCredentialHost );
        p += sprintf( p, "Accept: */*\r\n" );
        p += sprintf( p, "x-amzn-iot-thingname: %s\r\n", pReq->pThingName );
        p += sprintf( p, "\r\n" );

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
            /* We got a success response here. Parse the token. */
            retStatus = parseIoTCredential( ( const char * )getLastHttpBodyLoc(), getLastHttpBodyLen(), pToken );
        }
        else
        {
            retStatus = KVS_STATUS_IOT_CERTIFICATE_ERROR;
        }
    } while( 0 );

    if( retStatus != KVS_STATUS_SUCCEEDED )
    {
        Iot_terminateCredential( pToken );
    }

    if( pNetworkContext != NULL )
    {
        disconnectFromServer( pNetworkContext );
        terminateNetworkContext(pNetworkContext);
        sysFree( pNetworkContext );
    }

    return retStatus;
}

void Iot_terminateCredential( IotCredentialToken_t * pToken )
{
    if( pToken != NULL )
    {
        if( pToken->pAccessKeyId != NULL )
        {
            sysFree( pToken->pAccessKeyId );
            pToken->pAccessKeyId = NULL;
        }

        if( pToken->pSecretAccessKey != NULL )
        {
            sysFree( pToken->pSecretAccessKey );
            pToken->pSecretAccessKey = NULL;
        }

        if( pToken->pSessionToken != NULL )
        {
            sysFree( pToken->pSessionToken );
            pToken->pSessionToken = NULL;
        }
    }
}
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
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <mbedtls/net.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <sys/socket.h>

#include "network_api.h"
#include "kvs_errors.h"
#include "kvs_port.h"
#include "logger.h"

/*-----------------------------------------------------------*/

int32_t initNetworkContext( NetworkContext_t * pNetworkContext )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pNetworkContext == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        memset( pNetworkContext, 0, sizeof( NetworkContext_t ) );

        mbedtls_net_init( &( pNetworkContext->server_fd ) );

        mbedtls_ssl_init( &( pNetworkContext->ssl ) );

        mbedtls_ssl_config_init( &( pNetworkContext->conf ) );

        mbedtls_ctr_drbg_init( &( pNetworkContext->ctr_drbg ) );

        mbedtls_entropy_init( &( pNetworkContext->entropy ) );

        if( mbedtls_ctr_drbg_seed( &( pNetworkContext->ctr_drbg ), mbedtls_entropy_func, &( pNetworkContext->entropy ), NULL, 0 ) != 0 )
        {
            retStatus = KVS_STATUS_FAIL_TO_INIT_NETWORK;
        }
        else if( ( pNetworkContext->pHttpSendBuffer = ( uint8_t * ) sysMalloc ( MAX_HTTP_SEND_BUFFER_LEN ) ) == NULL )
        {
            LOG_ERROR("OOM: pHttpSendBuffer\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pNetworkContext->pHttpRecvBuffer = ( uint8_t * ) sysMalloc ( MAX_HTTP_RECV_BUFFER_LEN ) ) == NULL )
        {
            LOG_ERROR("OOM: pHttpRecvBuffer\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else
        {
            pNetworkContext->uHttpSendBufferLen = MAX_HTTP_SEND_BUFFER_LEN;
            pNetworkContext->uHttpRecvBufferLen = MAX_HTTP_RECV_BUFFER_LEN;
        }

        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            terminateNetworkContext( pNetworkContext);
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

void terminateNetworkContext( NetworkContext_t * pNetworkContext )
{
    if( pNetworkContext != NULL )
    {
        LOG_DEBUG("Terminate network context\r\n");

        mbedtls_net_free( &( pNetworkContext->server_fd ) );
        mbedtls_ssl_free( &( pNetworkContext->ssl ) );
        mbedtls_ssl_config_free( &( pNetworkContext->conf ) );

        if( pNetworkContext->pRootCA != NULL )
        {
            mbedtls_x509_crt_free( pNetworkContext->pRootCA );
            sysFree( pNetworkContext->pRootCA );
            pNetworkContext->pRootCA = NULL;
        }

        if( pNetworkContext->pClientCert != NULL )
        {
            mbedtls_x509_crt_free( pNetworkContext->pClientCert );
            sysFree( pNetworkContext->pClientCert );
            pNetworkContext->pClientCert = NULL;
        }

        if( pNetworkContext->pPrivateKey != NULL )
        {
            mbedtls_pk_free( pNetworkContext->pPrivateKey );
            sysFree( pNetworkContext->pPrivateKey );
            pNetworkContext->pPrivateKey = NULL;
        }

        sysFree( pNetworkContext->pHttpSendBuffer );
        pNetworkContext->pHttpSendBuffer = NULL;
        pNetworkContext->uHttpSendBufferLen = 0;

        sysFree( pNetworkContext->pHttpRecvBuffer );
        pNetworkContext->pHttpRecvBuffer = NULL;
        pNetworkContext->uHttpRecvBufferLen = 0;
    }
}

/*-----------------------------------------------------------*/

static int32_t _connectToServer( NetworkContext_t * pNetworkContext,
                                 const char * pServerHost,
                                 const char * pServerPort,
                                 const char * pRootCA,
                                 const char * pCertificate,
                                 const char * pPrivateKey )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    int ret = 0;
    bool bHasX509Certificate = false;

    if( pNetworkContext == NULL )
    {
        LOG_ERROR("Invalid Arg: network context\r\n");
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( pRootCA != NULL && pCertificate != NULL && pPrivateKey != NULL )
    {
        bHasX509Certificate = true;
        if( ( pNetworkContext->pRootCA = ( mbedtls_x509_crt * )sysMalloc( sizeof( mbedtls_x509_crt ) ) ) == NULL )
        {
            LOG_ERROR("OOM: pRootCA\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pNetworkContext->pClientCert = ( mbedtls_x509_crt * )sysMalloc( sizeof( mbedtls_x509_crt ) ) ) == NULL )
        {
            LOG_ERROR("OOM: pClientCert\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pNetworkContext->pPrivateKey = ( mbedtls_pk_context * )sysMalloc( sizeof( mbedtls_pk_context ) ) ) == NULL )
        {
            LOG_ERROR("OOM: pPrivateKey\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else
        {
            mbedtls_x509_crt_init( pNetworkContext->pRootCA );
            mbedtls_x509_crt_init( pNetworkContext->pClientCert );
            mbedtls_pk_init( pNetworkContext->pPrivateKey );
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( ( ret = mbedtls_net_connect( &( pNetworkContext->server_fd ), pServerHost, pServerPort, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
        {
            LOG_ERROR("net connect err (%d)\r\n", ret);
            retStatus = KVS_STATUS_NETWORK_CONNECT_ERROR;
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        mbedtls_ssl_set_bio( &( pNetworkContext->ssl ), &( pNetworkContext->server_fd ), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout );

        if( ( ret = mbedtls_ssl_config_defaults( &( pNetworkContext->conf ), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
        {
            LOG_ERROR("ssl config err (%d)\r\n", ret);
            retStatus = KVS_STATUS_NETWORK_CONFIG_ERROR;
        }
        else
        {
            mbedtls_ssl_conf_rng( &( pNetworkContext->conf ), mbedtls_ctr_drbg_random, &( pNetworkContext->ctr_drbg ) );

            if( bHasX509Certificate )
            {
                if( ( ret = mbedtls_x509_crt_parse( pNetworkContext->pRootCA, ( void * )pRootCA, strlen( pRootCA ) + 1 ) ) != 0 )
                {
                    LOG_ERROR("x509 Root CA parse err (%d)\r\n", ret);
                    retStatus = KVS_STATUS_NETWORK_SETUP_ERROR;
                }
                else if( ( ret = mbedtls_x509_crt_parse( pNetworkContext->pClientCert, ( void * )pCertificate, strlen( pCertificate ) + 1 ) ) != 0 )
                {
                    LOG_ERROR("x509 client cert parse err (%d)\r\n", ret);
                    retStatus = KVS_STATUS_NETWORK_SETUP_ERROR;
                }
                else if( ( ret = mbedtls_pk_parse_key( pNetworkContext->pPrivateKey, ( void * )pPrivateKey, strlen( pPrivateKey ) + 1, NULL, 0 ) ) != 0 )
                {
                    LOG_ERROR("x509 priv key parse err (%d)\r\n", ret);
                    retStatus = KVS_STATUS_NETWORK_SETUP_ERROR;
                }
                else
                {
                    mbedtls_ssl_conf_authmode( &( pNetworkContext->conf ), MBEDTLS_SSL_VERIFY_REQUIRED );

                    mbedtls_ssl_conf_ca_chain( &( pNetworkContext->conf ), pNetworkContext->pRootCA, NULL );

                    if( ( ret = mbedtls_ssl_conf_own_cert( &( pNetworkContext->conf ), pNetworkContext->pClientCert, pNetworkContext->pPrivateKey ) ) != 0 )
                    {
                        LOG_ERROR("ssl conf cert err (%d)\r\n", ret);
                        retStatus = KVS_STATUS_NETWORK_SETUP_ERROR;
                    }
                }
            }
            else
            {
                mbedtls_ssl_conf_authmode( &( pNetworkContext->conf ), MBEDTLS_SSL_VERIFY_OPTIONAL );
            }
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        if( ( ret = mbedtls_ssl_setup( &( pNetworkContext->ssl ), &( pNetworkContext->conf ) ) ) != 0 )
        {
            LOG_ERROR("ssl setup err (%d)\r\n", ret);
            retStatus = KVS_STATUS_NETWORK_SETUP_ERROR;
        }
        else if( ( ret = mbedtls_ssl_handshake( &( pNetworkContext->ssl ) ) ) != 0 )
        {
            LOG_ERROR("ssl handshake err (%d)\r\n", ret);
            retStatus = KVS_STATUS_NETWORK_SSL_HANDSHAKE_ERROR;
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t connectToServer( NetworkContext_t * pNetworkContext,
                         const char * pServerHost,
                         const char * pServerPort )
{
    return _connectToServer( pNetworkContext, pServerHost, pServerPort, NULL, NULL, NULL );
}

/*-----------------------------------------------------------*/

int32_t connectToServerWithX509Cert( NetworkContext_t * pNetworkContext,
                                     const char * pServerHost,
                                     const char * pServerPort,
                                     const char * pRootCA,
                                     const char * pCertificate,
                                     const char * pPrivateKey )
{
    return _connectToServer( pNetworkContext, pServerHost, pServerPort, pRootCA, pCertificate, pPrivateKey );
}

/*-----------------------------------------------------------*/

int32_t disconnectFromServer( NetworkContext_t * pNetworkContext )
{
    mbedtls_ssl_close_notify( &( pNetworkContext->ssl ) );

    return 0;
}

/*-----------------------------------------------------------*/

int32_t networkSend( NetworkContext_t * pNetworkContext,
                     const void * pBuffer,
                     size_t uBytesToSend )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    uint8_t * pIndex = ( uint8_t * )pBuffer;
    int32_t uBytesRemaining = ( int32_t )uBytesToSend; // It should be safe because we won't send data larger than 2^31-1 bytes
    int32_t n = 0;

    if( pNetworkContext == NULL || pBuffer == NULL  )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        while( uBytesRemaining > 0UL )
        {
            LOG_DEBUG("try to send %d bytes\r\n", uBytesRemaining);
            n = mbedtls_ssl_write( &( pNetworkContext->ssl ), pIndex, uBytesRemaining );

            if( n < 0 || n > uBytesRemaining )
            {
                LOG_WARN("ssl send err (%d)\r\n", n);
                retStatus = KVS_STATUS_NETWORK_SEND_ERROR;
                break;
            }
            else
            {
                LOG_DEBUG("sent %d bytes\r\n", n);
                uBytesRemaining -= n;
                pIndex += n;
            }
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        return uBytesToSend;
    }
    else
    {
        return retStatus;
    }
}

/*-----------------------------------------------------------*/

int32_t isRecvDataAvailable( NetworkContext_t * pNetworkContext )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    struct timeval tv = { 0 };
    fd_set read_fds = { 0 };
    int fd = 0;

    if( pNetworkContext == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        fd = pNetworkContext->server_fd.fd;
        if( fd < 0 )
        {
            retStatus = KVS_STATUS_NETWORK_SOCKET_NOT_CONNECTED;
        }
        else
        {
            FD_ZERO( &read_fds );
            FD_SET( fd, &read_fds );

            tv.tv_sec  = 0;
            tv.tv_usec = 0;

            if( select( fd + 1, &read_fds, NULL, NULL, &tv ) >= 0 )
            {
                if( FD_ISSET( fd, &read_fds) )
                {
                    /* We have available receiving data. */
                }
                else
                {
                    retStatus = KVS_STATUS_NETWORK_NO_AVAILABLE_RECV_DATA;
                }
            }
            else
            {
                retStatus = KVS_STATUS_NETWORK_SELECT_ERROR;
            }
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t networkRecv( NetworkContext_t * pNetworkContext,
                     void * pBuffer,
                     size_t uBytesToRecv )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    int32_t n = 0;

    if( pNetworkContext == NULL || pBuffer == NULL )
    {
        LOG_ERROR("Invalid Arg in networkRecv\r\n");
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        n = mbedtls_ssl_read( &( pNetworkContext->ssl ), pBuffer, uBytesToRecv );

        if( n < 0 || n > uBytesToRecv )
        {
            LOG_WARN("ssl read err (%d)\r\n", n);
            retStatus = KVS_STATUS_NETWORK_RECV_ERROR;
        }
        else
        {
            LOG_DEBUG("ssl read %d bytes\r\n", n);
        }
    }

    if( retStatus == KVS_STATUS_SUCCEEDED )
    {
        return n;
    }
    else
    {
        return retStatus;
    }
}
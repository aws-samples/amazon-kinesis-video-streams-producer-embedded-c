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
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <mbedtls/sha256.h>
#include <mbedtls/md.h>

#include "aws_signer_v4.h"
#include "kvs_errors.h"
#include "kvs_port.h"
#include "logger.h"

#define CANONICAL_HEADER_TEMPLATE   "%.*s:%.*s\n"
#define CANONICAL_BODY_TEMPLATE     "\n%s\n"
#define CANONICAL_SCOPE_TEMPLATE    "%.*s/%.*s/%.*s/%s"
#define CANONICAL_SIGNED_TEMPLATE   "%s\n%s\n%s\n%s"
#define SIGNATURE_START_TEMPLATE    "%s%.*s"

/*-----------------------------------------------------------*/

/**
 * @brief Encode a message into SHA256 text format
 *
 * It's a util function that encode pMsg into SHA256 HEX text format and put it in buffer pEncodeHash.
 *
 * @param[in] pMsg The message to be encoded
 * @param[in] uMsgLen The length of the message
 * @param[out] pEncodedHash The buffer to be stored the results
 *
 * @return KVS error code if error happened; Or the length of encoded length if it succeeded.
 */
static int32_t hexEncodedSha256( uint8_t * pMsg,
                                 uint32_t uMsgLen,
                                 char * pEncodedHash )
{
    size_t i = 0;
    char *p = NULL;
    uint8_t hashBuf[ SHA256_DIGEST_LENGTH ];

    if( pMsg == NULL || pEncodedHash == NULL )
    {
        return KVS_STATUS_INVALID_ARG;
    }

    mbedtls_sha256( pMsg, uMsgLen, hashBuf, 0 );

    /* encode hash result into hex text format */
    p = pEncodedHash;
    for( i = 0; i < SHA256_DIGEST_LENGTH; i++ )
    {
        p += sprintf( p, "%02x", hashBuf[i] );
    }

    return p - pEncodedHash;
}

/*-----------------------------------------------------------*/

int32_t AwsSignerV4_initContext( AwsSignerV4Context_t * pCtx,
                                 uint32_t uBufsize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;

    if( pCtx == NULL )
    {
        LOG_ERROR("Invalid Arg: AwsSignerV4Context\r\n");
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        memset( pCtx, 0, sizeof( AwsSignerV4Context_t ) );

        if( ( pCtx->pBuf = ( char * )sysMalloc( uBufsize ) ) == NULL )
        {
            LOG_ERROR("OOM: AwsSignerV4 pBuf\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pCtx->pSignedHeader = ( char * )sysMalloc( AWS_SIG_V4_MAX_HEADERS_LEN ) ) == NULL )
        {
            LOG_ERROR("OOM: pSignedHeader\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pCtx->pScope = ( char * )sysMalloc( MAX_SCOPE_LEN ) ) == NULL )
        {
            LOG_ERROR("OOM: pScope\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else if( ( pCtx->pHmacEncoded = ( char * )sysMalloc( AWS_SIG_V4_MAX_HMAC_SIZE ) ) == NULL )
        {
            LOG_ERROR("OOM: pHmacEncoded\r\n");
            retStatus = KVS_STATUS_NOT_ENOUGH_MEMORY;
        }
        else
        {
            pCtx->uBufSize = uBufsize;
            pCtx->pBufIndex = pCtx->pBuf;
            memset( pCtx->pSignedHeader, 0, AWS_SIG_V4_MAX_HEADERS_LEN );
            memset( pCtx->pScope, 0, MAX_SCOPE_LEN );
            memset( pCtx->pHmacEncoded, 0, AWS_SIG_V4_MAX_HMAC_SIZE );
        }

        if( retStatus != KVS_STATUS_SUCCEEDED )
        {
            AwsSignerV4_terminateContext(pCtx);
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

void AwsSignerV4_terminateContext( AwsSignerV4Context_t * pCtx )
{
    if( pCtx != NULL )
    {
        if( pCtx->pBuf != NULL )
        {
            sysFree( pCtx->pBuf );
            pCtx->pBuf = NULL;
            pCtx->uBufSize = 0;
        }

        if( pCtx->pSignedHeader != NULL )
        {
            sysFree( pCtx->pSignedHeader );
            pCtx->pSignedHeader = NULL;
        }

        if( pCtx->pScope != NULL )
        {
            sysFree( pCtx->pScope );
            pCtx->pScope = NULL;
        }

        if( pCtx->pHmacEncoded != NULL )
        {
            sysFree( pCtx->pHmacEncoded );
            pCtx->pHmacEncoded = NULL;
        }
    }
}

/*-----------------------------------------------------------*/

int32_t AwsSignerV4_initCanonicalRequest( AwsSignerV4Context_t * pCtx,
                                          char * pMethod,
                                          uint32_t uMethodLen,
                                          char * pUri,
                                          uint32_t uUriLen,
                                          char * pParameter,
                                          uint32_t pParameterLen )
{
    int retStatus = KVS_STATUS_SUCCEEDED;

    if( pCtx == NULL || pCtx->pBuf == NULL || pCtx->pBufIndex == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( pMethod == NULL || uMethodLen == 0 || pUri == NULL || uUriLen == 0 )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        pCtx->pBufIndex += sprintf( pCtx->pBufIndex, "%.*s\n", ( int ) uMethodLen, pMethod );
        pCtx->pBufIndex += sprintf( pCtx->pBufIndex, "%.*s\n", ( int ) uUriLen, pUri );

        if( pParameter == NULL || pParameterLen == 0 )
        {
            pCtx->pBufIndex += sprintf( pCtx->pBufIndex, "\n");
        }
        else
        {
            pCtx->pBufIndex += sprintf( pCtx->pBufIndex, "%.*s\n", ( int )pParameterLen, pParameter );
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t AwsSignerV4_addCanonicalHeader( AwsSignerV4Context_t * pCtx,
                                        char * pHeader,
                                        uint32_t uHeaderLen,
                                        char * pValue,
                                        uint32_t uValueLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    size_t uSignedHeaderLen = 0;

    if( pCtx == NULL || pCtx->pBuf == NULL || pCtx->pBufIndex == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( pHeader == NULL || uHeaderLen == 0 || pValue == NULL || uValueLen == 0 )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        /* Add this canonical header into headers */
        pCtx->pBufIndex += sprintf( pCtx->pBufIndex, CANONICAL_HEADER_TEMPLATE,
                                    ( int ) uHeaderLen, pHeader,
                                    ( int ) uValueLen, pValue );

        /* Append seperator ';' if this is not the first header */
        uSignedHeaderLen = strlen( pCtx->pSignedHeader );
        if( uSignedHeaderLen != 0 )
        {
            pCtx->pSignedHeader[ uSignedHeaderLen++ ] = ';';
        }

        /* append this header into signed header list */
        snprintf( pCtx->pSignedHeader + uSignedHeaderLen, uHeaderLen + 1, "%.*s", ( int )uHeaderLen, pHeader );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t AwsSignerV4_addCanonicalBody( AwsSignerV4Context_t * pCtx,
                                      uint8_t * pBody,
                                      uint32_t uBodyLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    int32_t xEncodedLen = 0;
    char pBodyHexSha256[ AWS_SIG_V4_MAX_HMAC_SIZE ];

    if ( pCtx == NULL || pCtx->pBuf == NULL || pCtx->pBufIndex == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        /* Append signed header list into canonical request */
        pCtx->pBufIndex += sprintf( pCtx->pBufIndex, CANONICAL_BODY_TEMPLATE, pCtx->pSignedHeader );

        /* Append encoded sha results of message body into canonical request */
        xEncodedLen = hexEncodedSha256( pBody, uBodyLen, pBodyHexSha256 );
        if (xEncodedLen < 0 ) {
            /* Propagate the error */
            retStatus = xEncodedLen;
        }
        else
        {
            memcpy( pCtx->pBufIndex, pBodyHexSha256, xEncodedLen );
            pCtx->pBufIndex += xEncodedLen;
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

int32_t AwsSignerV4_sign( AwsSignerV4Context_t * pCtx,
                          char * pSecretKey,
                          uint32_t uSecretKeyLen,
                          char * pRegion,
                          uint32_t uRegionLen,
                          char * pService,
                          uint32_t uServiceLen,
                          char * pXAmzDate,
                          uint32_t uXAmzDateLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    char *p = NULL;
    size_t i = 0;
    char pRequestHexSha256[ AWS_SIG_V4_MAX_HMAC_SIZE ];
    char pSignedStr[ MAX_SIGNED_STRING_LEN ];
    int xSignedStrLen = 0;
    const mbedtls_md_info_t * md_info = NULL;
    uint8_t pHmac[ AWS_SIG_V4_MAX_HMAC_SIZE ];
    uint32_t uHmacSize = 0;

    if( pCtx == NULL || pCtx->pBuf == NULL || pCtx->pBufIndex == NULL )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else if( pSecretKey == NULL || uSecretKeyLen == 0 || pRegion == NULL || uRegionLen == 0 || pService == NULL || uServiceLen == 0 || pXAmzDate == NULL || uXAmzDateLen == 0 )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        /* Encoded the canonical request into HEX SHA text format. */
        retStatus = hexEncodedSha256( ( uint8_t * )( pCtx->pBuf ), pCtx->pBufIndex - pCtx->pBuf, pRequestHexSha256 );

        if( retStatus >= 0 )
        {
            retStatus = KVS_STATUS_SUCCEEDED;

            md_info = mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 );

            /* Generate the scope string */
            snprintf( pCtx->pScope, MAX_SCOPE_LEN, CANONICAL_SCOPE_TEMPLATE, SIGNATURE_DATE_STRING_LEN, pXAmzDate, uRegionLen, pRegion, uServiceLen, pService, AWS_SIG_V4_SIGNATURE_END );

            /* Generate signed string */
            xSignedStrLen = sprintf( pSignedStr, CANONICAL_SIGNED_TEMPLATE, AWS_SIG_V4_ALGORITHM, pXAmzDate, pCtx->pScope, pRequestHexSha256 );

            /* Generate the beginning of the signature */
            snprintf( ( char * )pHmac, AWS_SIG_V4_MAX_HMAC_SIZE, SIGNATURE_START_TEMPLATE, AWS_SIG_V4_SIGNATURE_START, ( int )uSecretKeyLen, pSecretKey );

            uHmacSize = mbedtls_md_get_size( mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ) );

            if( mbedtls_md_hmac( md_info, pHmac, sizeof( AWS_SIG_V4_SIGNATURE_START ) - 1 + uSecretKeyLen, ( uint8_t * )pXAmzDate, SIGNATURE_DATE_STRING_LEN, pHmac ) != 0 )
            {
                retStatus = KVS_STATUS_FAIL_TO_CALCULATE_HASH;
            }
            else if( mbedtls_md_hmac( md_info, pHmac, uHmacSize, ( uint8_t * )pRegion, uRegionLen, pHmac ) != 0 )
            {
                retStatus = KVS_STATUS_FAIL_TO_CALCULATE_HASH;
            }
            else if( mbedtls_md_hmac( md_info, pHmac, uHmacSize, ( uint8_t * )pService, uServiceLen, pHmac ) != 0 )
            {
                retStatus = KVS_STATUS_FAIL_TO_CALCULATE_HASH;
            }
            else if( mbedtls_md_hmac( md_info, pHmac, uHmacSize, ( uint8_t * )AWS_SIG_V4_SIGNATURE_END, sizeof( AWS_SIG_V4_SIGNATURE_END ) - 1, pHmac ) != 0 )
            {
                retStatus = KVS_STATUS_FAIL_TO_CALCULATE_HASH;
            }
            else if( mbedtls_md_hmac( md_info, pHmac, uHmacSize, ( uint8_t * )pSignedStr, xSignedStrLen, pHmac ) != 0 )
            {
                retStatus = KVS_STATUS_FAIL_TO_CALCULATE_HASH;
            }
            else
            {
                /* encode the hash result into HEX text */
                p = pCtx->pHmacEncoded;
                for( i = 0; i < uHmacSize; i++ )
                {
                    p += sprintf( p, "%02x", pHmac[ i ] & 0xFF );
                }
            }
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

char * AwsSignerV4_getSignedHeader( AwsSignerV4Context_t * pCtx )
{
    if( pCtx == NULL )
    {
        return NULL;
    }
    else
    {
        return pCtx->pSignedHeader;
    }
}

/*-----------------------------------------------------------*/

char * AwsSignerV4_getScope( AwsSignerV4Context_t * pCtx )
{
    if( pCtx == NULL )
    {
        return NULL;
    }
    else
    {
        return pCtx->pScope;
    }
}

/*-----------------------------------------------------------*/

char * AwsSignerV4_getHmacEncoded( AwsSignerV4Context_t * pCtx )
{
    if( pCtx == NULL )
    {
        return NULL;
    }
    else
    {
        return pCtx->pHmacEncoded;
    }
}
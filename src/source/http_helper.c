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

#include "kvs_errors.h"
#include "http_helper.h"

#include "llhttp.h"

/*-----------------------------------------------------------*/

static uint32_t uLastHttpStatusCode = 0;
static char *pLastHttpBodyLoc = NULL;
static uint32_t uLastHttpBodyLen = 0;

/*-----------------------------------------------------------*/

static int handleHttpOnBodyComplete( llhttp_t *httpParser, const char *at, size_t length )
{
    /* FIXME: It's neither a thread safe design, nor a memory safe design. */
    pLastHttpBodyLoc = ( char * )at;
    uLastHttpBodyLen = ( uint32_t )length;
    return 0;
}

/*-----------------------------------------------------------*/

int32_t parseHttpResponse( char *pBuf, uint32_t uLen )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    llhttp_t httpParser = { 0 };
    llhttp_settings_t httpSettings = { 0 };
    enum llhttp_errno httpErrno = HPE_OK;

    pLastHttpBodyLoc = NULL;
    uLastHttpBodyLen = 0;

    llhttp_settings_init( &httpSettings );
    httpSettings.on_body = handleHttpOnBodyComplete;
    llhttp_init( &httpParser, HTTP_RESPONSE, &httpSettings);

    httpErrno = llhttp_execute( &httpParser, pBuf, ( size_t )uLen );
    if ( httpErrno != HPE_OK )
    {
        retStatus = KVS_STATUS_NETWORK_RECV_ERROR;
    }
    else
    {
        uLastHttpStatusCode = ( uint32_t )(httpParser.status_code);
        retStatus = KVS_STATUS_SUCCEEDED;
    }
    return retStatus;
}

uint32_t getLastHttpStatusCode( void )
{
    return uLastHttpStatusCode;
}

char * getLastHttpBodyLoc( void )
{
    return pLastHttpBodyLoc;
}

uint32_t getLastHttpBodyLen( void )
{
    return uLastHttpBodyLen;
}
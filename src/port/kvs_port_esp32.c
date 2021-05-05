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

#include <time.h>
#include <sys/time.h>

#include "kvs_port.h"
#include "kvs_errors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void sleepInMs( uint32_t ms )
{
    vTaskDelay( ms / portTICK_PERIOD_MS  );
}

int32_t getTimeInIso8601( char *pBuf, uint32_t uBufSize )
{
    int32_t retStatus = KVS_STATUS_SUCCEEDED;
    time_t timeUtcNow = { 0 };

    if( pBuf == NULL || uBufSize < DATE_TIME_ISO_8601_FORMAT_STRING_SIZE )
    {
        retStatus = KVS_STATUS_INVALID_ARG;
    }
    else
    {
        timeUtcNow = time( NULL );
        strftime( pBuf, DATE_TIME_ISO_8601_FORMAT_STRING_SIZE, "%Y%m%dT%H%M%SZ", gmtime( &timeUtcNow ) );
    }

    return retStatus;
}

uint64_t getEpochTimestampInMs( void )
{
    uint64_t timestamp = 0;

    struct timeval tv;
    gettimeofday( &tv, NULL );
    timestamp = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

    return timestamp;
}

uint8_t getRandomNumber( void )
{
    return ( uint8_t )rand();
}

void * sysMalloc( size_t size )
{
    return malloc( size );
}

void sysFree( void * ptr )
{
    free( ptr );
}
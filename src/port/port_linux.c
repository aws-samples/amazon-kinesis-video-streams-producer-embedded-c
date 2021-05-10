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

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "kvs/errors.h"
#include "kvs/port.h"

int platformInit(void)
{
    int xRes = KVS_ERRNO_NONE;

    srand(time(NULL));

    return xRes;
}

int getTimeInIso8601( char *pBuf, size_t uBufSize )
{
    int xRes = KVS_ERRNO_NONE;
    time_t xTimeUtcNow = { 0 };

    if (pBuf == NULL || uBufSize < DATE_TIME_ISO_8601_FORMAT_STRING_SIZE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        xTimeUtcNow = time( NULL );
        strftime(pBuf, DATE_TIME_ISO_8601_FORMAT_STRING_SIZE, "%Y%m%dT%H%M%SZ", gmtime(&xTimeUtcNow));
    }

    return xRes;
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
    return (uint8_t)rand();
}
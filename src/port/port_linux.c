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
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "kvs/errors.h"
#include "kvs/port.h"

#define PAST_OLD_TIME_IN_EPOCH 1600000000

int platformInit(void)
{
    int res = KVS_ERRNO_NONE;

    srand(time(NULL));

    return res;
}

int getTimeInIso8601(char *pBuf, size_t uBufSize)
{
    int res = KVS_ERRNO_NONE;
    time_t xTimeUtcNow = {0};

    if (pBuf == NULL || uBufSize < DATE_TIME_ISO_8601_FORMAT_STRING_SIZE)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        xTimeUtcNow = time(NULL);
        /* Current time should not less than a specific old time. If it does, then it means system time is incorrect. */
        if ((long)xTimeUtcNow < (long)PAST_OLD_TIME_IN_EPOCH)
        {
            res = KVS_ERROR_PAST_OLD_TIME;
        }
        else
        {
            strftime(pBuf, DATE_TIME_ISO_8601_FORMAT_STRING_SIZE, "%Y%m%dT%H%M%SZ", gmtime(&xTimeUtcNow));
        }
    }

    return res;
}

uint64_t getEpochTimestampInMs(void)
{
    uint64_t timestamp = 0;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;

    return timestamp;
}

uint8_t getRandomNumber(void)
{
    return (uint8_t)rand();
}

void sleepInMs(uint32_t ms)
{
    usleep(ms * 1000);
}
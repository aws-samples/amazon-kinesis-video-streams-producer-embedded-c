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

#ifndef KVS_PORT_H
#define KVS_PORT_H

#include <inttypes.h>

/* The string length of "date + time" format of ISO 8601 required by AWS Signature V4. */
#define DATE_TIME_ISO_8601_FORMAT_STRING_SIZE           ( 17 )

/**
 * @brief Platform init for KVS application
 *
 * @return 0 on success, non-zero value otherwise
 */
int platformInit(void);

/**
 * @brief Return time in ISO 8601 format: YYYYMMDD'T'HHMMSS'Z'
 *
 * AWS Signature V4 requires HTTP header x-amz-date which is in ISO 8601 format. ISO 8601 format is YYYYMMDD'T'HHMMSS'Z'.
 * For example, "20150830T123600Z" is a valid timestamp.
 *
 * @param[out] buf The buffer to store ths ISO 8601 string (including end of string character)
 * @param[out] uBufSize The buffer size
 * @return 0 on success, non-zero value otherwise
 */
int getTimeInIso8601(char *buf, size_t uBufSize);

/**
 * @brief return epoch time in unit of millisecond
 *
 * @return epoch time in millisecond
 */
uint64_t getEpochTimestampInMs(void);

/**
 * @brief return a random number
 *
 * @return a random number in uint8_t range
 */
uint8_t getRandomNumber(void);

/**
 * @brief sleep in milliseconds.
 *
 * @param[in] ms milliseconds to sleep
 */
void sleepInMs(uint32_t ms);

#endif /* KVS_PORT_H */
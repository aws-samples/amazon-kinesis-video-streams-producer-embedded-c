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

#ifndef _KVS_PORT_H_
#define _KVS_PORT_H_

#include <stdint.h>

/* The string length of "date + time" format of ISO 8601 required by AWS Signature V4. */
#define DATE_TIME_ISO_8601_FORMAT_STRING_SIZE           ( 17 )

/*-----------------------------------------------------------*/

/* Un-aligned big endian functions. Different arch and compiler may have better assembly instructions.
 * But these functions are not bottle neck of performance. So it should be fine to keep this implementation. */
#define PUT_UNALIGNED_2_byte_BE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint16_t v = ( uint16_t )val;                                           \
        pDst[1] = ( uint8_t )v;                                                 \
        pDst[0] = ( uint8_t )( v >> 8 );                                        \
    } while( 0 );

#define PUT_UNALIGNED_4_byte_BE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint32_t v = ( uint32_t )val;                                           \
        pDst[3] = ( uint8_t )v;                                                 \
        pDst[2] = ( uint8_t )( v >> 8 );                                        \
        pDst[1] = ( uint8_t )( v >> 16 );                                       \
        pDst[0] = ( uint8_t )( v >> 24 );                                       \
    } while( 0 );

#define PUT_UNALIGNED_8_byte_BE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint64_t v = ( uint64_t )val;                                           \
        pDst[7] = ( uint8_t )v;                                                 \
        pDst[6] = ( uint8_t )( v >>  8 );                                       \
        pDst[5] = ( uint8_t )( v >> 16 );                                       \
        pDst[4] = ( uint8_t )( v >> 24 );                                       \
        pDst[3] = ( uint8_t )( v >> 32 );                                       \
        pDst[2] = ( uint8_t )( v >> 40 );                                       \
        pDst[1] = ( uint8_t )( v >> 48 );                                       \
        pDst[0] = ( uint8_t )( v >> 56 );                                       \
    } while( 0 );

#define PUT_UNALIGNED_2_byte_LE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint16_t v = ( uint16_t )val;                                           \
        pDst[0] = ( uint8_t )v;                                                 \
        pDst[1] = ( uint8_t )( v >> 8 );                                        \
    } while( 0 );

/*-----------------------------------------------------------*/

/* Un-aligned little endian functions. Different arch and compiler may have better assembly instructions.
 * But these functions are not bottle neck of performance. So it should be fine to keep this implementation. */
#define PUT_UNALIGNED_4_byte_LE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint32_t v = ( uint32_t )val;                                           \
        pDst[0] = ( uint8_t )v;                                                 \
        pDst[1] = ( uint8_t )( v >> 8 );                                        \
        pDst[2] = ( uint8_t )( v >> 16 );                                       \
        pDst[3] = ( uint8_t )( v >> 24 );                                       \
    } while( 0 );

#define PUT_UNALIGNED_8_byte_LE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint64_t v = ( uint64_t )val;                                           \
        pDst[0] = ( uint8_t )v;                                                 \
        pDst[1] = ( uint8_t )( v >>  8 );                                       \
        pDst[2] = ( uint8_t )( v >> 16 );                                       \
        pDst[3] = ( uint8_t )( v >> 24 );                                       \
        pDst[4] = ( uint8_t )( v >> 32 );                                       \
        pDst[5] = ( uint8_t )( v >> 40 );                                       \
        pDst[6] = ( uint8_t )( v >> 48 );                                       \
        pDst[7] = ( uint8_t )( v >> 56 );                                       \
    } while( 0 );

/*-----------------------------------------------------------*/

/**
 * @brief sleep in milliseconds
 *
 * @param[in] ms The desired milliseconds to sleep
 */
void sleepInMs( uint32_t ms );

/**
 * @brief Return time in ISO 8601 format: YYYYMMDD'T'HHMMSS'Z'
 *
 * AWS Signature V4 requires HTTP header x-amz-date which is in ISO 8601 format. ISO 8601 format is YYYYMMDD'T'HHMMSS'Z'.
 * For example, "20150830T123600Z" is a valid timestamp.
 *
 * @param[out] buf The buffer to store ths ISO 8601 string (including end of string character)
 * @param[out] uBufSize The buffer size
 * @return
 */
int32_t getTimeInIso8601( char *buf, uint32_t uBufSize );

/**
 * @brief return epoch time in unit of millisecond
 *
 * @return epoch time in millisecond
 */
uint64_t getEpochTimestampInMs( void );

/**
 * @brief return a random number
 *
 * @return a random number in uint8_t range
 */
uint8_t getRandomNumber( void );

/**
 * @brief System malloc function
 *
 * The implementation can be c "malloc" function or a wrapper function for debug and diagnosis purpose.
 *
 * @param[in] size Memory size
 *
 * @return data pointer
 */
void * sysMalloc( size_t size );

/**
 * @brief System free function
 *
 * The implementation can be c "free" function or a wrapper function for debug and diagnosis purpose.
 *
 * @param[in] ptr data pointer
 */
void sysFree( void * ptr );

#endif // #ifndef _KVS_PORT_H_
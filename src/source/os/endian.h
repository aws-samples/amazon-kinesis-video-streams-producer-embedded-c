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

#ifndef ENDIAN_H
#define ENDIAN_H

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

/* Un-aligned little endian functions. Different arch and compiler may have better assembly instructions.
 * But these functions are not bottle neck of performance. So it should be fine to keep this implementation. */
#define PUT_UNALIGNED_2_byte_LE(ptr,val)                                        \
    do {                                                                        \
        uint8_t *pDst = ptr;                                                    \
        uint16_t v = ( uint16_t )val;                                           \
        pDst[0] = ( uint8_t )v;                                                 \
        pDst[1] = ( uint8_t )( v >> 8 );                                        \
    } while( 0 );

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

#endif /* ENDIAN_H */
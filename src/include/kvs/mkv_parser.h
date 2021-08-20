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

#ifndef MKV_PARSER_H
#define MKV_PARSER_H

#include <stdint.h>

#define MKV_ELEMENT_MAX_ID_LEN                      (4)
#define MKV_ELEMENT_MAX_SIZE_LEN                    (8)
#define MKV_ELEMENT_HDR_MAX_SIZE                    (MKV_ELEMENT_MAX_ID_LEN + MKV_ELEMENT_MAX_SIZE_LEN)

#define MKV_ELEMENT_SIZE_UNKNOWN                    0xFF

#define MKV_ELEMENT_ID_EBML                         0x1A45DFA3

#define MKV_ELEMENT_ID_SEGMENT                      0x18538067
#define MKV_ELEMENT_ID_INFO                         0x1549A966
#define MKV_ELEMENT_ID_TIMESTAMP_SCALE              0x2AD7B1
#define MKV_ELEMENT_ID_CLUSTER                      0x1F43B675
#define MKV_ELEMENT_ID_TIMESTAMP                    0xE7

typedef struct ElementHdr
{
    size_t uIdLen;
    uint32_t uId;
    size_t uSizeLen;
    uint64_t uSize;
} ElementHdr_t;

/**
 * @brief Get the length of element ID of MKV element
 *
 * @param[in] uByte The first byte of element ID
 * @return length of element ID
 */
size_t Mkv_getElementIdLen(uint8_t uByte);

/**
 * @brief Get the element id from a given buffer
 *
 * @param[in] pBuf buffer of element id
 * @param[in] uBufSize buffer size
 * @param[out] puElementId element id
 * @param[out] puElementIdLen length of element id
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_getElementId(uint8_t *pBuf, size_t uBufSize, uint32_t *puElementId, size_t *puElementIdLen);

/**
 * @brief Get the length of element size of MKV element
 *
 * @param[in] uByte The first byte of element size
 * @return length of element size
 */
size_t Mkv_getElementSizeLen(uint8_t uByte);

/**
 * @brief Get the element size from a given buffer
 *
 * @param pBuf buffer of element size
 * @param uBufSize buffer size
 * @param puElementSize element size
 * @param puElementSizeLen length of element size
 * @return 0 on success, non-zero value otherwise
 */
int Mkv_getElementSize(uint8_t *pBuf, size_t uBufSize, uint64_t *puElementSize, size_t *puElementSizeLen);

#endif /* MKV_PARSER_H */
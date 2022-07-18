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

#ifndef KVS_NALU_H
#define KVS_NALU_H

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#define NALU_TYPE_UNKNOWN   (0)

/* VCL */
#define NALU_TYPE_NON_IDR_PICTURE   (1)
#define NALU_TYPE_PFRAME_PA         (2)
#define NALU_TYPE_PFRAME_PB         (3)
#define NALU_TYPE_PFRAME_PC         (4)
#define NALU_TYPE_IFRAME            (5)

/* non-VCL */
#define NALU_TYPE_SEI               (6)
#define NALU_TYPE_SPS               (7)
#define NALU_TYPE_PPS               (8)

/**
 * @brief Check if the frame is key frame
 *
 * @param[in] pBuf The AVCC or Annex-B buffer
 * @param[in] uLen The length of buffer
 * @return true if it's key-frame, or false otherwise
 */
bool isKeyFrame(uint8_t *pBuf, size_t uLen);

/**
 * @brief Get NALU type of the first NALU in the buffer
 *
 * @param[in] pBuf The NALU buffer
 * @param[in] uLen The size of buffer
 * @return NALU type
 */
int NALU_getNaluType(uint8_t *pBuf, size_t uLen);

/**
 * @brief Get specific NALU type from AVCC NALUs
 *
 * @param[in] pAvccBuf The buffer of AVCC NALUs
 * @param[in] uAvccLen The length of buffer
 * @param[in] uNaluType The NALU type to be query
 * @param[out] ppNalu The address of queried NALU type that is not memory allocated. It's NULL if it's not found or error happened
 * @param[out] puNaluLen The length of queried NALU type
 * @return 0 on success, non-zero value otherwise
 */
int NALU_getNaluFromAvccNalus(uint8_t *pAvccBuf, size_t uAvccLen, uint8_t uNaluType, uint8_t **ppNalu, size_t *puNaluLen);

/**
 * @brief Get specific NALU type from Annex-B NALUs
 *
 * @param[in] pAnnexBBuf The buffer of Annex-B NALUs
 * @param[in] uAnnexBLen The length of buffer
 * @param[in] uNaluType The NALU type to be query
 * @param[out] ppNalu The address of queried NALU type that is not memory allocated. It's NULL if it's not found or error happened
 * @param[out] puNaluLen The length of queried NALU type
 * @return 0 on success, non-zero value otherwise
 */
int NALU_getNaluFromAnnexBNalus(uint8_t *pAnnexBBuf, size_t uAnnexBLen, uint8_t uNaluType, uint8_t **ppNalu, size_t *puNaluLen);

/**
 * @brief Check if a NALU is Annex-B NALU
 *
 * @param[in] pAnnexbBuf The buffer of NALU
 * @param[in] uAnnexbBufLen The length of buffer
 * @return true if it's a Annex-B NALU, or false otherwise
 */
bool NALU_isAnnexBFrame(uint8_t *pAnnexbBuf, uint32_t uAnnexbBufLen);

/**
 * @brief Convert a Annex-B NALU into AVCC NALU in place
 *
 * An AVCC NALU may has larger length than Annex-B NALU, so a larger Annex-B buffer size may needed.
 *
 * @param[in,out] pAnnexbBuf The Annex-B NALU buffer
 * @param[in] uAnnexbBufLen The length Annex-B NALU
 * @param[in] uAnnexbBufSize The size of the Annex-B buffer
 * @param[out] pAvccLen The converted AVCC NALU length.
 * @return 0 on success, non-zero value otherwise
 */
int NALU_convertAnnexBToAvccInPlace(uint8_t *pAnnexbBuf, uint32_t uAnnexbBufLen, uint32_t uAnnexbBufSize, uint32_t *pAvccLen);

/**
 * @brief Parse the video resolution from a SPS NALU
 *
 * @param[in] pSps The SPS NALU
 * @param[in] uSpsLen The length of SPS NALU
 * @param[out] puWidth The width of video
 * @param[out] puHeight The height of video
 * @return 0 on success, non-zero value otherwise
 */
int NALU_getH264VideoResolutionFromSps(uint8_t *pSps, size_t uSpsLen, uint16_t *puWidth, uint16_t *puHeight);

#endif /* KVS_NALU_H */
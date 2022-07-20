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
#include <stdbool.h>

/* Third party headers */
#include "azure_c_shared_utility/xlogging.h"

/* Public headers */
#include "kvs/errors.h"
#include "kvs/nalu.h"

/* Internal headers */
#include "codec/sps_decode.h"
#include "os/endian.h"

#define MAX_NALU_COUNT_IN_A_FRAME ( 16 )

typedef struct Nal
{
    uint32_t uNalBeginIdx;
    uint32_t uNalLen;
} Nal_t;

bool isKeyFrame(uint8_t *pBuf, size_t uLen)
{
    bool bIsKeyFrame = false;
    uint8_t *pIFrameNalu = NULL;
    size_t uIFrameNalueLen = 0;

    if (pBuf != NULL)
    {
        if (NALU_isAnnexBFrame(pBuf, uLen))
        {
            if (NALU_getNaluFromAnnexBNalus(pBuf, uLen, NALU_TYPE_IFRAME, &pIFrameNalu, &uIFrameNalueLen) == KVS_ERRNO_NONE)
            {
                bIsKeyFrame = true;
            }
        }
        else
        {
            if (NALU_getNaluFromAvccNalus(pBuf, uLen, NALU_TYPE_IFRAME, &pIFrameNalu, &uIFrameNalueLen) == KVS_ERRNO_NONE)
            {
                bIsKeyFrame = true;
            }
        }
    }

    return bIsKeyFrame;
}

int NALU_getNaluType(uint8_t *pBuf, size_t uLen)
{
    int xNaluType = NALU_TYPE_UNKNOWN;

    if (pBuf != NULL && uLen > 0)
    {
        if (uLen >=4 && pBuf[0] == 0x00 && pBuf[1] == 0x00 && pBuf[2] == 0x01)
        {
            /* It's AnnexB frame with 3 bytes header, get type from 4th byte. */
            xNaluType = pBuf[3] & 0x1F;
        }
        else if (uLen >= 5 && pBuf[0] == 0x00 && pBuf[1] == 0x00 && pBuf[2] == 0x00 && pBuf[3] == 0x01)
        {
            /* It's AnnexB frame with 4 bytes header, get type from 5th byte. */
            xNaluType = pBuf[4] & 0x1F;
        }
        else if (uLen >= 5)
        {
            /* It's AVCC frame, get type from 5th byte. */
            xNaluType = pBuf[4] & 0x1F;
        }
    }
    return xNaluType;
}

int NALU_getNaluFromAvccNalus(uint8_t *pAvccBuf, size_t uAvccLen, uint8_t uNaluType, uint8_t **ppNalu, size_t *puNaluLen)
{
    int res = KVS_ERRNO_NONE;
    uint32_t uAvccIdx = 0;
    uint32_t uNaluLen = 0;

    if (pAvccBuf == NULL || uAvccLen < 5 || uNaluType == 0 || uNaluType >= 32 || ppNalu == NULL || puNaluLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        while (uAvccIdx < uAvccLen - 4)
        {
            uNaluLen = (pAvccBuf[uAvccIdx] << 24 ) | ( pAvccBuf[uAvccIdx+1] << 16 ) | (pAvccBuf[uAvccIdx+2] << 8 ) | pAvccBuf[uAvccIdx+3];
            uAvccIdx += 4;

            if ((pAvccBuf[uAvccIdx] & 0x80) == 0 && (pAvccBuf[uAvccIdx] & 0x1F) == uNaluType)
            {
                *ppNalu = pAvccBuf + uAvccIdx;
                *puNaluLen = uNaluLen;
                break;
            }
            uAvccIdx += uNaluLen;
        }

        if (uAvccIdx >= uAvccLen)
        {
            res = KVS_ERROR_AVCC_NALU_IS_BROKEN;
        }
    }

    return res;
}

int NALU_getNaluFromAnnexBNalus(uint8_t *pAnnexBBuf, size_t uAnnexBLen, uint8_t uNaluType, uint8_t **ppNalu, size_t *puNaluLen)
{
    int res = KVS_ERRNO_NONE;
    uint8_t *pIdx = pAnnexBBuf;
    uint8_t *pNalu = NULL;
    size_t uNaluLen = 0;

    if (pAnnexBBuf == NULL || uAnnexBLen < 5 || uNaluType >=32 || ppNalu == NULL || puNaluLen == NULL)
    {
        LogError("Invalid argument");
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        while (pIdx - pAnnexBBuf < uAnnexBLen - 4)
        {
            if (pIdx[0] == 0x00)
            {
                if (pIdx[1] == 0x00)
                {
                    if (pIdx[2] == 0x00)
                    {
                        if (pIdx[3] == 0x01)
                        {
                            /* It's a valid NALU here. */
                            if (pNalu != NULL)
                            {
                                uNaluLen = pIdx - pNalu;
                                break;
                            }
                            else if ((pIdx[4] & 0x80) == 0 && (pIdx[4] & 0x1F) == uNaluType)
                            {
                                pNalu = pIdx + 4;
                            }
                            pIdx += 4;
                        }
                        else
                        {
                            pIdx += 4;
                        }
                    }
                    else if (pIdx[2] == 0x01)
                    {
                        /* It's a valid NALU here. */
                        if (pNalu != NULL)
                        {
                            uNaluLen = pIdx - pNalu;
                            break;
                        }
                        else if ((pIdx[3] & 0x80) == 0 && (pIdx[3] & 0x1F) == uNaluType)
                        {
                            pNalu = pIdx + 3;
                        }
                        pIdx += 3;
                    }
                    else
                    {
                        pIdx += 3;
                    }
                }
                else
                {
                    pIdx += 2;
                }
            }
            else
            {
                pIdx++;
            }
        }

        if (pNalu != NULL)
        {
            if (uNaluLen == 0)
            {
                uNaluLen = uAnnexBLen - (pNalu - pAnnexBBuf);
            }
            *ppNalu = pNalu;
            *puNaluLen = uNaluLen;
        }
        else
        {
            res = KVS_ERROR_NALU_TYPE_NOT_FOUND;
        }
    }

    return res;
}

bool NALU_isAnnexBFrame(uint8_t *pAnnexbBuf, uint32_t uAnnexbBufLen)
{
    bool bRes = false;

    if (pAnnexbBuf == NULL || uAnnexbBufLen < 3)
    {
        LogError("Invalid argument");
    }
    else
    {
        if (uAnnexbBufLen >=3 && pAnnexbBuf[0] == 0x00 && pAnnexbBuf[1] == 0x00 && pAnnexbBuf[2] == 0x01)
        {
            bRes = true;
        }
        else if (uAnnexbBufLen >= 4 && pAnnexbBuf[0] == 0x00 && pAnnexbBuf[1] == 0x00 && pAnnexbBuf[2] == 0x00 && pAnnexbBuf[3] == 0x01)
        {
            bRes = true;
        }
    }

    return bRes;
}

int NALU_convertAnnexBToAvccInPlace(uint8_t *pAnnexbBuf, uint32_t uAnnexbBufLen, uint32_t uAnnexbBufSize, uint32_t *pAvccLen)
{
    int res = KVS_ERRNO_NONE;
    uint32_t i = 0;
    Nal_t xNals[ MAX_NALU_COUNT_IN_A_FRAME ];
    uint32_t uNalRbspCount = 0;
    uint32_t uAvccTotalLen = 0;
    uint32_t uAvccIdx = 0;

    if (pAnnexbBuf == NULL || uAnnexbBufLen <= 4 || uAnnexbBufSize < uAnnexbBufLen || pAvccLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if (!NALU_isAnnexBFrame(pAnnexbBuf, uAnnexbBufLen))
    {
        LogInfo("It's not a Annex-B frame, skip convert");
    }
    else
    {
        /* Go through all Annex-B buffer and record all RBSP begin and length first. */
        while (i < uAnnexbBufLen - 4)
        {
            if (uNalRbspCount > MAX_NALU_COUNT_IN_A_FRAME)
            {
                break;
            }

            if (pAnnexbBuf[i] == 0x00)
            {
                if (pAnnexbBuf[i+1] == 0x00)
                {
                    if (pAnnexbBuf[i+2] == 0x00)
                    {
                        if (pAnnexbBuf[i+3] == 0x01)
                        {
                            /* 0x00000001 is start code of NAL. */
                            if (uNalRbspCount > 0)
                            {
                                xNals[uNalRbspCount-1].uNalLen = i - xNals[uNalRbspCount-1].uNalBeginIdx;
                            }

                            i += 4;
                            xNals[uNalRbspCount++].uNalBeginIdx = i;
                        }
                        else if (pAnnexbBuf[i + 3] == 0x00)
                        {
                            /* 0x00000000 is not allowed. */
                            LogInfo("Invalid NALU format");
                            res = KVS_ERROR_INVALID_NALU_FORMAT;
                            break;
                        }
                        else
                        {
                            /* 0x000000XX is acceptable. */
                            i += 4;
                        }
                    }
                    else if (pAnnexbBuf[i+2] == 0x01)
                    {
                        /* 0x000001 is start code of NAL */
                        if (uNalRbspCount > 0)
                        {
                            xNals[uNalRbspCount-1].uNalLen = i - xNals[uNalRbspCount-1].uNalBeginIdx;
                        }

                        i += 3;
                        xNals[uNalRbspCount++].uNalBeginIdx = i;
                    }
                    else
                    {
                        /* 0x0000XX is acceptable. It includes EPB case and we reserve EPB byte. */
                        i += 3;
                    }
                }
                else
                {
                    /* 0x00XX is acceptable. */
                    i += 2;
                }
            }
            else
            {
                /* 0xXX is acceptable. */
                i++;
            }
        }

        if (uNalRbspCount == 0)
        {
            res = KVS_ERROR_MISSING_NALU;
            LogInfo("No NALU is found in Annex-B frame");
        }
        else if (uNalRbspCount > MAX_NALU_COUNT_IN_A_FRAME)
        {
            res = KVS_ERROR_EXCEED_MAX_NALU_COUNT_LIMIT;
            LogError("NAL RBSP count exceeds max count");
        }
        else
        {
            /* Update the last BSPS. */
            xNals[ uNalRbspCount - 1 ].uNalLen = uAnnexbBufLen - xNals[ uNalRbspCount - 1 ].uNalBeginIdx;

            /* Calculate needed size if we convert it to Avcc format. */
            uAvccTotalLen = 4 * uNalRbspCount;
            for (i=0; i<uNalRbspCount; i++)
            {
                uAvccTotalLen += xNals[i].uNalLen;
            }

            if (uAvccTotalLen > uAnnexbBufSize)
            {
                /* We don't have enough space to convert Annex-B to Avcc in place. */
                LogInfo("No available space to convert Annex-B inplace");
                *pAvccLen = 0;
                res = KVS_ERROR_NO_ENOUGH_SPACE_FOR_NALU_CONVERSION;
            }
            else
            {
               /* move RBSP from back to head */
                i = uNalRbspCount - 1;
                uAvccIdx = uAvccTotalLen;
                do
                {
                    /* move RBSP */
                    uAvccIdx -= xNals[i].uNalLen;
                    memmove(pAnnexbBuf + uAvccIdx, pAnnexbBuf + xNals[i].uNalBeginIdx, xNals[i].uNalLen);

                    /* fill length info */
                    uAvccIdx -= 4;
                    PUT_UNALIGNED_4_byte_BE(pAnnexbBuf + uAvccIdx, xNals[i].uNalLen);

                    if (i == 0)
                    {
                        break;
                    }
                    i--;
                } while (true);

                *pAvccLen = uAvccTotalLen;
            }
        }
    }

    return res;
}

int NALU_getH264VideoResolutionFromSps(uint8_t *pSps, size_t uSpsLen, uint16_t *puWidth, uint16_t *puHeight)
{
    int res = KVS_ERRNO_NONE;

    if (pSps == NULL || uSpsLen < 2 || puWidth == NULL || puHeight == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((pSps[0] & 0x1F) != 7)
    {
        res = KVS_ERROR_INVALID_NALU_FORMAT;
        LogError("Not a SPS NALU");
    }
    else
    {
        getH264VideoResolution((char *)(pSps + 1), uSpsLen - 1, puWidth, puHeight);
    }

    return res;
}
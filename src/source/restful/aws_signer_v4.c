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

#include <stddef.h>

/* Third party headers */
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/xlogging.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"

/* Public headers */
#include "kvs/errors.h"

/* Internal headers */
#include "os/allocator.h"
#include "restful/aws_signer_v4.h"

#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_POST "POST"

/* The buffer length used for doing SHA256 hash check. */
#define SHA256_DIGEST_LENGTH 32

/* The buffer length used for ASCII Hex encoded SHA256 result. */
#define HEX_ENCODED_SHA_256_STRING_SIZE 65

/* The string length of "date" format defined by AWS Signature V4. */
#define SIGNATURE_DATE_STRING_LEN 8

/* The signature start described by AWS Signature V4. */
#define AWS_SIG_V4_SIGNATURE_START "AWS4"

/* The signature end described by AWS Signature V4. */
#define AWS_SIG_V4_SIGNATURE_END "aws4_request"

/* The signed algorithm. */
#define AWS_SIG_V4_ALGORITHM "AWS4-HMAC-SHA256"

/* The length of oct string for SHA256 hash buffer. */
#define AWS_SIG_V4_MAX_HMAC_SIZE (SHA256_DIGEST_LENGTH * 2 + 1)

/* Template of canonical scope: <DATE>/<REGION>/<SERVICE>/<SIGNATURE_END>*/
#define TEMPLATE_CANONICAL_SCOPE "%.*s/%s/%s/%s"

/* Template of canonical signed string: <ALGO>\n<DATE_TIME>\n<SCOPE>\n<HEX_SHA_CANONICAL_REQ> */
#define TEMPLATE_CANONICAL_SIGNED_STRING "%s\n%s\n%s\n%s"

#define TEMPLATE_SIGNATURE_START "%s%s"

typedef struct AwsSigV4
{
    STRING_HANDLE xStCanonicalRequest;
    STRING_HANDLE xStSignedHeaders;
    STRING_HANDLE xStScope;
    STRING_HANDLE xStHmacHexEncoded;
    STRING_HANDLE xStAuthorization;
} AwsSigV4_t;

static int prvValidateHttpMethod(const char *pcHttpMethod)
{
    if (pcHttpMethod == NULL)
    {
        return KVS_ERROR_INVALID_ARGUMENT;
    }
    else if (!strcmp(pcHttpMethod, HTTP_METHOD_POST) && !strcmp(pcHttpMethod, HTTP_METHOD_GET) && !strcmp(pcHttpMethod, HTTP_METHOD_PUT))
    {
        return KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidateUri(const char *pcUri)
{
    /* TODO: Add lexical verification. */

    if (pcUri == NULL)
    {
        return KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidateHttpHeader(const char *pcHeader, const char *pcValue)
{
    /* TODO: Add lexical verification. */

    if (pcHeader == NULL || pcValue == NULL)
    {
        return KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvHexEncodedSha256(const unsigned char *pMsg, size_t uMsgLen, char pcHexEncodedHash[HEX_ENCODED_SHA_256_STRING_SIZE])
{
    int res = KVS_ERRNO_NONE;
    int retVal = 0;
    int i = 0;
    char *p = NULL;
    unsigned char pHashBuf[SHA256_DIGEST_LENGTH] = {0};

    if (pMsg == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((retVal = mbedtls_sha256_ret(pMsg, uMsgLen, pHashBuf, 0)) != 0)
    {
        res = KVS_GENERATE_MBEDTLS_ERROR(retVal);
    }
    else
    {
        p = pcHexEncodedHash;
        for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            p += snprintf(p, 3, "%02x", pHashBuf[i]);
        }
    }

    return res;
}

AwsSigV4Handle AwsSigV4_Create(char *pcHttpMethod, char *pcUri, char *pcQuery)
{
    int res = KVS_ERRNO_NONE;
    AwsSigV4_t *pxAwsSigV4 = NULL;

    if ((res = prvValidateHttpMethod(pcHttpMethod) == KVS_ERRNO_NONE && prvValidateUri(pcUri)) == KVS_ERRNO_NONE)
    {
        do
        {
            pxAwsSigV4 = (AwsSigV4_t *)kvsMalloc(sizeof(AwsSigV4_t));
            if (pxAwsSigV4 == NULL)
            {
                res = KVS_ERROR_OUT_OF_MEMORY;
                break;
            }
            memset(pxAwsSigV4, 0, sizeof(AwsSigV4_t));

            pxAwsSigV4->xStCanonicalRequest = STRING_construct_sprintf("%s\n%s\n%s\n", pcHttpMethod, pcUri, (pcQuery == NULL) ? "" : pcQuery);
            pxAwsSigV4->xStSignedHeaders = STRING_new();
            pxAwsSigV4->xStScope = STRING_new();
            pxAwsSigV4->xStHmacHexEncoded = STRING_new();
            pxAwsSigV4->xStAuthorization = STRING_new();

            if (pxAwsSigV4->xStCanonicalRequest == NULL || pxAwsSigV4->xStSignedHeaders == NULL || pxAwsSigV4->xStHmacHexEncoded == NULL)
            {
                res = KVS_ERROR_INVALID_ARGUMENT;
                break;
            }
        } while (0);

        if (res == KVS_ERRNO_FAIL)
        {
            LogError("Failed to init canonical request");
            AwsSigV4_Terminate(pxAwsSigV4);
            pxAwsSigV4 = NULL;
        }
    }

    return (AwsSigV4Handle)pxAwsSigV4;
}

void AwsSigV4_Terminate(AwsSigV4Handle xSigV4Handle)
{
    AwsSigV4_t *pxAwsSigV4 = (AwsSigV4_t *)xSigV4Handle;

    if (pxAwsSigV4 != NULL)
    {
        STRING_delete(pxAwsSigV4->xStCanonicalRequest);
        STRING_delete(pxAwsSigV4->xStSignedHeaders);
        STRING_delete(pxAwsSigV4->xStScope);
        STRING_delete(pxAwsSigV4->xStHmacHexEncoded);
        STRING_delete(pxAwsSigV4->xStAuthorization);
        kvsFree(pxAwsSigV4);
    }
}

int AwsSigV4_AddCanonicalHeader(AwsSigV4Handle xSigV4Handle, const char *pcHeader, const char *pcValue)
{
    int res = KVS_ERRNO_NONE;
    AwsSigV4_t *pxAwsSigV4 = (AwsSigV4_t *)xSigV4Handle;

    if (pxAwsSigV4 == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((res = prvValidateHttpHeader(pcHeader, pcValue)) != KVS_ERRNO_NONE)
    {
        /* Propagate the res error */
    }
    else if (STRING_sprintf(pxAwsSigV4->xStCanonicalRequest, "%s:%s\n", pcHeader, pcValue) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else if (STRING_length(pxAwsSigV4->xStSignedHeaders) > 0 && STRING_concat(pxAwsSigV4->xStSignedHeaders, ";") != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else if (STRING_concat(pxAwsSigV4->xStSignedHeaders, pcHeader) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else
    {
        /* nop */
    }

    return res;
}

int AwsSigV4_AddCanonicalBody(AwsSigV4Handle xSigV4Handle, const char *pBody, size_t uBodyLen)
{
    int res = KVS_ERRNO_NONE;
    AwsSigV4_t *pxAwsSigV4 = (AwsSigV4_t *)xSigV4Handle;
    char pcBodyHexEncodedSha256[HEX_ENCODED_SHA_256_STRING_SIZE] = {0};

    if (pxAwsSigV4 == NULL || pBody == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if (STRING_sprintf(pxAwsSigV4->xStCanonicalRequest, "\n%s\n", STRING_c_str(pxAwsSigV4->xStSignedHeaders)) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else if (prvHexEncodedSha256((const unsigned char *)pBody, uBodyLen, pcBodyHexEncodedSha256) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else if (STRING_concat(pxAwsSigV4->xStCanonicalRequest, pcBodyHexEncodedSha256) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else
    {
        /* nop */
    }

    return res;
}

int AwsSigV4_Sign(AwsSigV4Handle xSigV4Handle, char *pcAccessKey, char *pcSecretKey, char *pcRegion, char *pcService, const char *pcXAmzDate)
{
    int res = KVS_ERRNO_NONE;
    int retVal = 0;
    AwsSigV4_t *pxAwsSigV4 = (AwsSigV4_t *)xSigV4Handle;
    char pcCanonicalReqHexEncSha256[HEX_ENCODED_SHA_256_STRING_SIZE] = {0};
    const mbedtls_md_info_t *pxMdInfo = NULL;
    STRING_HANDLE xStSignedStr = NULL;
    size_t uHmacSize = 0;
    char pHmac[AWS_SIG_V4_MAX_HMAC_SIZE] = {0};
    int i = 0;

    if (pxAwsSigV4 == NULL || pcSecretKey == NULL || pcRegion == NULL || pcService == NULL || pcXAmzDate == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    /* Do SHA256 on canonical request and then hex encode it. */
    else if (
        (res = prvHexEncodedSha256((const unsigned char *)STRING_c_str(pxAwsSigV4->xStCanonicalRequest), STRING_length(pxAwsSigV4->xStCanonicalRequest), pcCanonicalReqHexEncSha256)) !=
        KVS_ERRNO_NONE)
    {
        /* Propagate the res error */
    }
    else if ((pxMdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256)) == NULL)
    {
        res = KVS_ERROR_UNKNOWN_MBEDTLS_MESSAGE_DIGEST;
    }
    /* HMAC size of SHA256 should be 32. */
    else if ((uHmacSize = mbedtls_md_get_size(pxMdInfo)) == 0)
    {
        res = KVS_ERROR_INVALID_MBEDTLS_MESSAGE_DIGEST_SIZE;
    }
    /* Generate the scope string. */
    else if (STRING_sprintf(pxAwsSigV4->xStScope, TEMPLATE_CANONICAL_SCOPE, SIGNATURE_DATE_STRING_LEN, pcXAmzDate, pcRegion, pcService, AWS_SIG_V4_SIGNATURE_END) != 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    /* Generate the signed string. */
    else if (
        (xStSignedStr =
             STRING_construct_sprintf(TEMPLATE_CANONICAL_SIGNED_STRING, AWS_SIG_V4_ALGORITHM, pcXAmzDate, STRING_c_str(pxAwsSigV4->xStScope), pcCanonicalReqHexEncSha256)) == NULL)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    /* Generate the beginning of the signature. */
    else if (snprintf(pHmac, AWS_SIG_V4_MAX_HMAC_SIZE, TEMPLATE_SIGNATURE_START, AWS_SIG_V4_SIGNATURE_START, pcSecretKey) == 0)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    /* Calculate the HMAC of date, region, service, signature end, and signed string*/
    else if (
        (retVal = mbedtls_md_hmac(pxMdInfo, (const unsigned char *)pHmac, strlen(pHmac), (const unsigned char *)pcXAmzDate, SIGNATURE_DATE_STRING_LEN, (unsigned char *)pHmac)) != 0 ||
        (retVal = mbedtls_md_hmac(pxMdInfo, (const unsigned char *)pHmac, uHmacSize, (const unsigned char *)pcRegion, strlen(pcRegion), (unsigned char *)pHmac)) != 0 ||
        (retVal = mbedtls_md_hmac(pxMdInfo, (const unsigned char *)pHmac, uHmacSize, (const unsigned char *)pcService, strlen(pcService), (unsigned char *)pHmac)) != 0 ||
        (retVal = mbedtls_md_hmac(pxMdInfo, (const unsigned char *)pHmac, uHmacSize, (const unsigned char *)AWS_SIG_V4_SIGNATURE_END, sizeof(AWS_SIG_V4_SIGNATURE_END) - 1, (unsigned char *)pHmac)) != 0 ||
        (retVal = mbedtls_md_hmac(pxMdInfo, (const unsigned char *)pHmac, uHmacSize, (const unsigned char *)STRING_c_str(xStSignedStr), STRING_length(xStSignedStr), (unsigned char *)pHmac)) != 0)
    {
        res = KVS_GENERATE_MBEDTLS_ERROR(retVal);
    }
    else
    {
        for (i = 0; i < uHmacSize; i++)
        {
            if (STRING_sprintf(pxAwsSigV4->xStHmacHexEncoded, "%02x", pHmac[i] & 0xFF) != 0)
            {
                res = KVS_ERROR_C_UTIL_STRING_ERROR;
                break;
            }
        }

        if (res == 0)
        {
            if (STRING_sprintf(
                    pxAwsSigV4->xStAuthorization,
                    "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s",
                    pcAccessKey,
                    STRING_c_str(pxAwsSigV4->xStScope),
                    STRING_c_str(pxAwsSigV4->xStSignedHeaders),
                    STRING_c_str(pxAwsSigV4->xStHmacHexEncoded)) != 0)
            {
                res = KVS_ERROR_C_UTIL_STRING_ERROR;
            }
        }
    }

    STRING_delete(xStSignedStr);

    return res;
}

const char *AwsSigV4_GetAuthorization(AwsSigV4Handle xSigV4Handle)
{
    AwsSigV4_t *pxAwsSigV4 = (AwsSigV4_t *)xSigV4Handle;

    if (pxAwsSigV4 != NULL)
    {
        return STRING_c_str(pxAwsSigV4->xStAuthorization);
    }
    else
    {
        return NULL;
    }
}
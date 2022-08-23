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

/* Public headers */
#include "kvs/errors.h"

/* Internal headers */
#include "net/http_parser_adapter.h"

#define HTTP_RSP_STATUS_HDR         "HTTP/1.1"
#define HTTP_HDR_CONTENT_LENGTH     "Content-Length"

#define TOLOWERCASE(c)              (c | 0xA0)

/**
 * Get the length of the first line in a string. Assume each line is separated by the EOL ('\n') character.
 *
 * @param[in] pStr string to be parsed
 * @param[in] uStrLen string length
 * @param[out] puLineLen the length of the first line
 * @return 0 on success, non-zero value otherwise
 */
static int prvGetLine(const char *pStr, size_t uStrLen, size_t *puLineLen)
{
    int res = KVS_ERRNO_FAIL;
    const char *p = NULL;

    for (p = pStr; p != NULL && (p - pStr) < uStrLen; p++) {
        if (*p == '\n')
        {
            res = KVS_ERRNO_NONE;
            *puLineLen = p - pStr + 1;
            break;
        }
    }

    return res;
}

/**
 * Convert C-string to unsigned integer. It converts from the first digital character until the first non-digital character.
 *
 * @param[in] pStr string to be converted
 * @param[in] uStrLen string length
 * @return converted unsigned integer
 */
static unsigned int prvStrToUInt(const char *pStr, size_t uStrLen)
{
    const char *p = pStr;
    unsigned int n = 0;

    /* Step to the first digital character. */
    while ((p - pStr) < uStrLen && !(*p >= '0' && *p <= '9'))
    {
        p++;
    }

    /* Convert until the first non-digital character. */
    while ((p - pStr) < uStrLen && (*p >= '0' && *p <= '9'))
    {
        n = n * 10 + (*p - '0');
        p++;
    }

    return n;
}

/**
 * Compare at most n case-insensitive characters on two strings.
 *
 * @param[in] pStr1 pointer of first string
 * @param[in] pStr2 pointer of second string
 * @param[in] uLen maximum number of characters to compare
 * @return 0 on the contents of both strings are the same, none zero value which is the difference of the first character that does not match.
 */
static int prvStrNCmpCi(const char *pStr1, const char *pStr2, size_t uLen)
{
    int res = 0;
    int n = 0;

    while (n < uLen)
    {
        if (pStr1 == NULL)
        {
            if (pStr2 == NULL)
            {
                res = -1;
                break;
            }
            else
            {
                res = 1;
            }
        }
        else
        {
            if (pStr2 == NULL)
            {
                res = -1;
            }
            else
            {
                res = TOLOWERCASE(*pStr1) - TOLOWERCASE(*pStr2);
                if (res != 0)
                {
                    break;
                }
                n++;
                pStr1++;
                pStr2++;
            }
        }
    }

    return res;
}

int HttpParser_parseHttpResponse(const char *pBuf, size_t uLen, unsigned int *puStatusCode, const char **ppBodyLoc, size_t *puBodyLen)
{
    int res = KVS_ERRNO_NONE;
    const char *p = pBuf;
    size_t uLineLen = 0;
    const char *pBodyLoc = NULL;
    size_t uBodyLen = 0;

    if (pBuf == NULL || uLen == 0)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        while (prvGetLine(p, uLen, &uLineLen) == KVS_ERRNO_NONE)
        {
            if (prvStrNCmpCi(p, HTTP_RSP_STATUS_HDR, sizeof(HTTP_RSP_STATUS_HDR) - 1) == 0)
            {
                if (puStatusCode != NULL)
                {
                    *puStatusCode = prvStrToUInt(p + sizeof(HTTP_RSP_STATUS_HDR), uLineLen - sizeof(HTTP_RSP_STATUS_HDR));
                }
            }
            else if (prvStrNCmpCi(p, HTTP_HDR_CONTENT_LENGTH, sizeof(HTTP_HDR_CONTENT_LENGTH) - 1) == 0)
            {
                uBodyLen = prvStrToUInt(p + sizeof(HTTP_HDR_CONTENT_LENGTH), uLineLen - sizeof(HTTP_HDR_CONTENT_LENGTH));
            }
            else if (uLineLen == 2 && *p == '\r')
            {
                p += 2;
                uLen -= 2;
                break;
            }

            p += uLineLen;
            uLen -= uLineLen;
        }

        if (uBodyLen > 0)
        {
            if (uLen == uBodyLen)
            {
                pBodyLoc = p;
            }
            else
            {
                /* The body length does not match the remaining length of the buffer. Reset the result. */
                pBodyLoc = NULL;
                uBodyLen = 0;
                res = KVS_ERROR_HTTP_PARSE_EXECUTE_FAIL;
            }
        }

        if (ppBodyLoc != NULL && puBodyLen != NULL)
        {
            *ppBodyLoc = pBodyLoc;
            *puBodyLen = uBodyLen;
        }
    }
    return res;
}
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

/**
 * Parse the HTTP response.
 *
 * @param pBuf buffer that stores HTTP headers and body
 * @param uLen length of the buffer
 * @param puStatusCode HTTP status code
 * @param ppBodyLoc pointer of the HTTP body in the buffer
 * @param puBodyLen length of the body
 * @return 0 on success, non-zero value otherwise
 */
int HttpParser_parseHttpResponse(const char *pBuf, size_t uLen, unsigned int *puStatusCode, const char **ppBodyLoc, size_t *puBodyLen);
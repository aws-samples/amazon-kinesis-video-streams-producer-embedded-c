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

#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

#include "azure_c_shared_utility/httpheaders.h"

#include "netio.h"

#define HTTP_METHOD_GET                 "GET"
#define HTTP_METHOD_POST                "POST"

#define URI_QUERY_EMPTY                 ""

/* Headers that needs to be signed in AWS signer V4 */
#define HDR_CONNECTION                  "connection"
#define HDR_HOST                        "host"
#define HDR_TRANSFER_ENCODING           "transfer-encoding"
#define HDR_USER_AGENT                  "user-agent"
#define HDR_X_AMZ_DATE                  "x-amz-date"
#define HDR_X_AMZ_SECURITY_TOKEN        "x-amz-security-token"
#define HDR_X_AMZN_FRAG_ACK_REQUIRED    "x-amzn-fragment-acknowledgment-required"
#define HDR_X_AMZN_FRAG_T_TYPE          "x-amzn-fragment-timecode-type"
#define HDR_X_AMZN_IOT_THINGNAME        "x-amzn-iot-thingname"
#define HDR_X_AMZN_PRODUCER_START_T     "x-amzn-producer-start-timestamp"
#define HDR_X_AMZN_STREAM_NAME          "x-amzn-stream-name"

/* Headers that doesn't need sign */
#define HDR_ACCEPT                      "accept"
#define HDR_AUTHORIZATION               "authorization"
#define HDR_CONTENT_LENGTH              "content-length"
#define HDR_CONTENT_TYPE                "content-type"

#define VAL_ACCEPT_ANY                  "*/*"
#define VAL_CONTENT_TYPE_APPLICATION_jSON   "application/json"
#define VAL_USER_AGENT                  "myagent"
#define VAL_KEEP_ALIVE                  "keep-alive"
#define VAL_TRANSFER_ENCODING_CHUNKED   "chunked"
#define VAL_FRAGMENT_ACK_REQUIRED_TRUE  "1"

#define HTTP_BODY_EMPTY                 ""

/**
 * @brief Execute HTTP request
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[in] pcHttpMethod The HTTP method. (Ex. GET, PUT, POST)
 * @param[in] pcUri The relative path of URI
 * @param[in] xHttpReqHeaders The HTTP headers
 * @param[in] pcBody The HTTP body
 * @return 0 on success, non-zero value otherwise
 */
int Http_executeHttpReq(NetIoHandle xNetIoHandle, const char *pcHttpMethod, const char *pcUri, HTTP_HEADERS_HANDLE xHttpReqHeaders, const char *pcBody);

/**
 * @brief Receive HTTP response
 *
 * @param[in] xNetIoHandle The network I/O handle
 * @param[out] puHttpStatus The HTTP status code
 * @param[out] ppRspBody The HTTP response body that is memory allocated and the callee has the responsible to free it.
 * @param[out] puRspBodyLen The length of HTTP response body.
 * @return 0 on success, non-zero value otherwise
 */
int Http_recvHttpRsp(NetIoHandle xNetIoHandle, unsigned int *puHttpStatus, char **ppRspBody, size_t *puRspBodyLen);

#endif /* HTTP_HELPER_H */
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

#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

/* Thirdparty headers */
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/httpheaders.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/xlogging.h"
#include "llhttp.h"
#include "parson.h"

/* Public headers */
#include "kvs/errors.h"
#include "kvs/restapi.h"

/* Platform dependent headers */
#include "kvs/port.h"

/* Internal headers */
#include "allocator.h"
#include "aws_signer_v4.h"
#include "http_helper.h"
#include "json_helper.h"
#include "netio.h"

#ifndef SAFE_FREE
#define SAFE_FREE(a)                                                                                                                                                               \
    do                                                                                                                                                                             \
    {                                                                                                                                                                              \
        kvsFree(a);                                                                                                                                                                \
        a = NULL;                                                                                                                                                                  \
    } while (0)
#endif /* SAFE_FREE */

#define DEFAULT_RECV_BUFSIZE (1024)

#define PORT_HTTPS "443"

/*-----------------------------------------------------------*/

#define KVS_URI_CREATE_STREAM "/createStream"
#define KVS_URI_DESCRIBE_STREAM "/describeStream"
#define KVS_URI_GET_DATA_ENDPOINT "/getDataEndpoint"
#define KVS_URI_PUT_MEDIA "/putMedia"

/*-----------------------------------------------------------*/

#define DESCRIBE_STREAM_HTTP_BODY_TEMPLATE "{\"StreamName\": \"%s\"}"

#define CREATE_STREAM_HTTP_BODY_TEMPLATE "{\"StreamName\": \"%s\",\"DataRetentionInHours\": %d}"

#define GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE "{\"StreamName\": \"%s\",\"APIName\":\"PUT_MEDIA\"}"

/*-----------------------------------------------------------*/

typedef struct PutMedia
{
    NetIoHandle xNetIoHandle;
    unsigned int uLastErrorId;
} PutMedia_t;

#define JSON_KEY_EVENT_TYPE "EventType"
#define JSON_KEY_FRAGMENT_TIMECODE "FragmentTimecode"
#define JSON_KEY_ERROR_ID "ErrorId"

#define EVENT_TYPE_BUFFERING "\"BUFFERING\""
#define EVENT_TYPE_RECEIVED "\"RECEIVED\""
#define EVENT_TYPE_PERSISTED "\"PERSISTED\""
#define EVENT_TYPE_ERROR "\"ERROR\""
#define EVENT_TYPE_IDLE "\"IDLE\""

typedef enum
{
    EVENT_UNKNOWN = 0,
    EVENT_BUFFERING,
    EVENT_RECEIVED,
    EVENT_PERSISTED,
    EVENT_ERROR,
    EVENT_IDLE
} EVENT_TYPE;

typedef struct
{
    EVENT_TYPE eventType;
    uint64_t uFragmentTimecode;
    unsigned int uErrorId;
} FragmentAck_t;

/*-----------------------------------------------------------*/

static int prvValidateServiceParameter(KvsServiceParameter_t *pServPara)
{
    if (pServPara == NULL || pServPara->pcAccessKey == NULL || pServPara->pcSecretKey == NULL || pServPara->pcRegion == NULL || pServPara->pcService == NULL ||
        pServPara->pcHost == NULL)
    {
        return KVS_ERRNO_FAIL;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidateDescribeStreamParameter(KvsDescribeStreamParameter_t *pDescPara)
{
    if (pDescPara == NULL || pDescPara->pcStreamName == NULL)
    {
        return KVS_ERRNO_FAIL;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidateCreateStreamParameter(KvsCreateStreamParameter_t *pCreatePara)
{
    if (pCreatePara == NULL || pCreatePara->pcStreamName == NULL)
    {
        return KVS_ERRNO_FAIL;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidateGetDataEndpointParameter(KvsGetDataEndpointParameter_t *pGetDataEpPara)
{
    if (pGetDataEpPara == NULL || pGetDataEpPara->pcStreamName == NULL)
    {
        return KVS_ERRNO_FAIL;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static int prvValidatePutMediaParameter(KvsPutMediaParameter_t *pPutMediaPara)
{
    if (pPutMediaPara == NULL || pPutMediaPara->pcStreamName == NULL)
    {
        return KVS_ERRNO_FAIL;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static AwsSigV4Handle prvSign(KvsServiceParameter_t *pServPara, char *pcUri, char *pcQuery, HTTP_HEADERS_HANDLE xHeadersToSign, const char *pcHttpBody)
{
    int xRes = KVS_ERRNO_NONE;

    AwsSigV4Handle xAwsSigV4Handle = NULL;
    const char *pcVal;
    const char *pcXAmzDate;

    if ((xAwsSigV4Handle = AwsSigV4_Create(HTTP_METHOD_POST, pcUri, pcQuery)) == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_CONNECTION)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_CONNECTION, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_HOST)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_HOST, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_TRANSFER_ENCODING)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_TRANSFER_ENCODING, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_USER_AGENT)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_USER_AGENT, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcXAmzDate = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZ_DATE)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZ_DATE, pcXAmzDate) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZ_SECURITY_TOKEN)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZ_SECURITY_TOKEN, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_FRAG_ACK_REQUIRED)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_FRAG_ACK_REQUIRED, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_FRAG_T_TYPE)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_FRAG_T_TYPE, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_PRODUCER_START_T)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_PRODUCER_START_T, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_STREAM_NAME)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_STREAM_NAME, pcVal) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (AwsSigV4_AddCanonicalBody(xAwsSigV4Handle, pcHttpBody, strlen(pcHttpBody)) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (AwsSigV4_Sign(xAwsSigV4Handle, pServPara->pcAccessKey, pServPara->pcSecretKey, pServPara->pcRegion, pServPara->pcService, pcXAmzDate) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        /* nop */
    }

    if (xRes != KVS_ERRNO_NONE)
    {
        AwsSigV4_Terminate(xAwsSigV4Handle);
        xAwsSigV4Handle = NULL;
    }

    return xAwsSigV4Handle;
}

static int prvParseDataEndpoint(const char *pcJsonSrc, size_t uJsonSrcLen, char **ppcEndpoint)
{
    int xRes = KVS_ERRNO_NONE;
    STRING_HANDLE xStJson = NULL;
    JSON_Value *pxRootValue = NULL;
    JSON_Object *pxRootObject = NULL;
    char *pcDataEndpoint = NULL;
    size_t uEndpointLen = 0;

    json_set_escape_slashes(0);

    if (pcJsonSrc == NULL || uJsonSrcLen == 0 || ppcEndpoint == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((xStJson = STRING_construct_n(pcJsonSrc, uJsonSrcLen)) == NULL)
    {
        LogError("OOM: parse data endpoint");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (pxRootValue = json_parse_string(STRING_c_str(xStJson))) == NULL || (pxRootObject = json_value_get_object(pxRootValue)) == NULL ||
        (pcDataEndpoint = json_object_dotget_serialize_to_string(pxRootObject, "DataEndpoint", true)) == NULL)
    {
        LogError("Failed to parse data endpoint");
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        /* Please note that the memory of pcDataEndpoint is from malloc and we tranfer the ownership to caller here. */
        uEndpointLen = strlen(pcDataEndpoint);
        if (uEndpointLen > 8)
        {
            uEndpointLen -= 8;
            *ppcEndpoint = (char *)kvsMalloc(uEndpointLen + 1);
            if (*ppcEndpoint != NULL)
            {
                memcpy(*ppcEndpoint, pcDataEndpoint + 8, uEndpointLen);
                (*ppcEndpoint)[uEndpointLen] = '\0';
            }
        }
        kvsFree(pcDataEndpoint);
    }

    if (pxRootValue != NULL)
    {
        json_value_free(pxRootValue);
    }

    STRING_delete(xStJson);

    return xRes;
}

static char *prvGetTimecodeValue(FragmentTimecodeType_t xTimecodeType)
{
    if (xTimecodeType == TIMECODE_TYPE_ABSOLUTE)
    {
        return "ABSOLUTE";
    }
    else if (xTimecodeType == TIMECODE_TYPE_RELATIVE)
    {
        return "RELATIVE";
    }
    else
    {
        LogError("Invalid timecode type:%d\r\n", xTimecodeType);
        return "";
    }
}

static int prvParseFragmentAckLength(char *pcSrc, size_t uLen, size_t *puMsgLen, size_t *puBytesRead)
{
    int xRes = KVS_ERRNO_NONE;
    size_t uMsgLen = 0;
    size_t uBytesRead = 0;
    size_t i = 0;
    char c = 0;

    if (pcSrc == NULL || uLen == 0 || puMsgLen == NULL || puBytesRead == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        for (i = 0; i < uLen - 1; i++)
        {
            c = toupper(pcSrc[i]);
            if (isxdigit(c))
            {
                if (c >= '0' && c <= '9')
                {
                    uMsgLen = uMsgLen * 16 + (c - '0');
                }
                else
                {
                    uMsgLen = uMsgLen * 16 + (c - 'A') + 10;
                }
            }
            else if (c == '\r')
            {
                if (pcSrc[i + 1] == '\n')
                {
                    uBytesRead = i + 2;
                    break;
                }
            }
            else
            {
                xRes = KVS_ERRNO_FAIL;
            }
        }
    }

    if (xRes == KVS_ERRNO_NONE)
    {
        if (uBytesRead < 3 || (uBytesRead + uMsgLen + 2) > uLen || pcSrc[uBytesRead + uMsgLen] != '\r' || pcSrc[uBytesRead + uMsgLen + 1] != '\n')
        {
            xRes = KVS_ERRNO_FAIL;
        }
        else
        {
            *puMsgLen = uMsgLen;
            *puBytesRead = uBytesRead;
        }
    }

    return xRes;
}

static EVENT_TYPE prvGetEventType(char *pcEventType)
{
    EVENT_TYPE ev = EVENT_UNKNOWN;

    if (pcEventType != NULL)
    {
        if (strncmp(pcEventType, EVENT_TYPE_BUFFERING, sizeof(EVENT_TYPE_BUFFERING) - 1) == 0)
        {
            ev = EVENT_BUFFERING;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_RECEIVED, sizeof(EVENT_TYPE_RECEIVED) - 1) == 0)
        {
            ev = EVENT_RECEIVED;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_PERSISTED, sizeof(EVENT_TYPE_PERSISTED) - 1) == 0)
        {
            ev = EVENT_PERSISTED;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_ERROR, sizeof(EVENT_TYPE_ERROR) - 1) == 0)
        {
            ev = EVENT_ERROR;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_IDLE, sizeof(EVENT_TYPE_IDLE) - 1) == 0)
        {
            ev = EVENT_IDLE;
        }
    }

    return ev;
}

static int32_t parseFragmentMsg(const char *pcFragmentMsg, FragmentAck_t *pxFragmentAck)
{
    int xRes = KVS_ERRNO_NONE;
    JSON_Value *pxRootValue = NULL;
    JSON_Object *pxRootObject = NULL;
    char *pcEventType = NULL;

    json_set_escape_slashes(0);

    if (pcFragmentMsg == NULL || pxFragmentAck == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pxRootValue = json_parse_string(pcFragmentMsg)) == NULL || (pxRootObject = json_value_get_object(pxRootValue)) == NULL)
    {
        LogInfo("Failed to parse fragment msg:%s\r\n", pcFragmentMsg);
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((pcEventType = json_object_dotget_serialize_to_string(pxRootObject, JSON_KEY_EVENT_TYPE, false)) == NULL)
    {
        LogInfo("Unknown fragment ack:%s\r\n", pcFragmentMsg);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        pxFragmentAck->eventType = prvGetEventType(pcEventType);
        kvsFree(pcEventType);

        if (pxFragmentAck->eventType == EVENT_BUFFERING || pxFragmentAck->eventType == EVENT_RECEIVED || pxFragmentAck->eventType == EVENT_PERSISTED ||
            pxFragmentAck->eventType == EVENT_ERROR)
        {
            pxFragmentAck->uFragmentTimecode = json_object_dotget_uint64(pxRootObject, JSON_KEY_FRAGMENT_TIMECODE, 10);
            if (pxFragmentAck->eventType == EVENT_ERROR)
            {
                pxFragmentAck->uErrorId = (unsigned int)json_object_dotget_uint64(pxRootObject, JSON_KEY_ERROR_ID, 10);
            }
        }
    }

    if (pxRootValue != NULL)
    {
        json_value_free(pxRootValue);
    }

    return xRes;
}

static int prvParseFragmentAck(char *pcSrc, size_t uLen, FragmentAck_t *pxFragAck, size_t *puFragAckLen)
{
    int xRes = KVS_ERRNO_NONE;
    size_t uMsgLen = 0;
    size_t uBytesRead = 0;
    STRING_HANDLE xStFragMsg = NULL;

    if (pcSrc == NULL || uLen == 0 || pxFragAck == NULL || puFragAckLen == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (prvParseFragmentAckLength(pcSrc, uLen, &uMsgLen, &uBytesRead) != KVS_ERRNO_NONE)
    {
        LogInfo("Unknown fragment ack:%.*s", (int)uLen, pcSrc);
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((xStFragMsg = STRING_construct_n(pcSrc + uBytesRead, uMsgLen)) == NULL || parseFragmentMsg(STRING_c_str(xStFragMsg), pxFragAck) != KVS_ERRNO_NONE)
    {
        LogInfo("Failed to parse fragment ack");
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        *puFragAckLen = uBytesRead + uMsgLen + 2;
    }

    STRING_delete(xStFragMsg);

    return xRes;
}

static void prvLogFragmentAck(FragmentAck_t *pFragmentAck)
{
    if (pFragmentAck != NULL)
    {
        if (pFragmentAck->eventType == EVENT_BUFFERING)
        {
            LogInfo("Fragment buffering, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == EVENT_RECEIVED)
        {
            LogInfo("Fragment received, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == EVENT_PERSISTED)
        {
            LogInfo("Fragment persisted, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == EVENT_ERROR)
        {
            LogError("PutMedia session error id:%d", pFragmentAck->uErrorId);
        }
        else if (pFragmentAck->eventType == EVENT_IDLE)
        {
            LogInfo("PutMedia session Idle");
        }
        else
        {
            LogInfo("Unknown Fragment Ack");
        }
    }
}

int Kvs_describeStream(KvsServiceParameter_t *pServPara, KvsDescribeStreamParameter_t *pDescPara, unsigned int *puHttpStatusCode)
{
    int xRes = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (prvValidateServiceParameter(pServPara) != KVS_ERRNO_NONE || prvValidateDescribeStreamParameter(pDescPara) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(DESCRIBE_STREAM_HTTP_BODY_TEMPLATE, pDescPara->pcStreamName)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        LogError("Failed to allocate HTTP body");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL || HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        LogError("Failed to generate HTTP headers");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_DESCRIBE_STREAM, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        LogError("Failed to sign");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xNetIoHandle = NetIo_create()) == NULL || NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs) != KVS_ERRNO_NONE ||
        NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs) != KVS_ERRNO_NONE || NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s\r\n", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_DESCRIBE_STREAM, xHttpReqHeaders, STRING_c_str(xStHttpBody)) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen))
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        *puHttpStatusCode = uHttpStatusCode;
    }

    NetIo_disconnect(xNetIoHandle);
    NetIo_terminate(xNetIoHandle);
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStContentLength);
    STRING_delete(xStHttpBody);

    return xRes;
}

int Kvs_createStream(KvsServiceParameter_t *pServPara, KvsCreateStreamParameter_t *pCreatePara, unsigned int *puHttpStatusCode)
{
    int xRes = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (prvValidateServiceParameter(pServPara) != KVS_ERRNO_NONE || prvValidateCreateStreamParameter(pCreatePara) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(CREATE_STREAM_HTTP_BODY_TEMPLATE, pCreatePara->pcStreamName, pCreatePara->uDataRetentionInHours)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        LogError("Failed to allocate HTTP body");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL || HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        LogError("Failed to generate HTTP headers");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_CREATE_STREAM, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        LogError("Failed to sign");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xNetIoHandle = NetIo_create()) == NULL || NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs) != KVS_ERRNO_NONE ||
        NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs) != KVS_ERRNO_NONE || NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s\r\n", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_CREATE_STREAM, xHttpReqHeaders, STRING_c_str(xStHttpBody)) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen))
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        *puHttpStatusCode = uHttpStatusCode;
    }

    NetIo_disconnect(xNetIoHandle);
    NetIo_terminate(xNetIoHandle);
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStContentLength);
    STRING_delete(xStHttpBody);

    return xRes;
}

int Kvs_getDataEndpoint(KvsServiceParameter_t *pServPara, KvsGetDataEndpointParameter_t *pGetDataEpPara, unsigned int *puHttpStatusCode, char **ppcDataEndpoint)
{
    int xRes = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (prvValidateServiceParameter(pServPara) != KVS_ERRNO_NONE || prvValidateGetDataEndpointParameter(pGetDataEpPara) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE, pGetDataEpPara->pcStreamName)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        LogError("Failed to allocate HTTP body");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL || HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        LogError("Failed to generate HTTP headers");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_GET_DATA_ENDPOINT, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        LogError("Failed to sign");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xNetIoHandle = NetIo_create()) == NULL || NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs) != KVS_ERRNO_NONE ||
        NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs) != KVS_ERRNO_NONE || NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s\r\n", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_GET_DATA_ENDPOINT, xHttpReqHeaders, STRING_c_str(xStHttpBody)) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen))
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        if (puHttpStatusCode != NULL)
        {
            *puHttpStatusCode = uHttpStatusCode;
        }

        if (uHttpStatusCode != 200)
        {
            LogInfo("Get Data Endpoint failed, HTTP status code: %u", uHttpStatusCode);
            LogInfo("HTTP response message:%.*s", (int)uRspBodyLen, pRspBody);
        }
        else
        {
            if (prvParseDataEndpoint(pRspBody, uRspBodyLen, ppcDataEndpoint) != KVS_ERRNO_NONE)
            {
                LogError("Failed to parse data endpoint");
                xRes = KVS_ERRNO_FAIL;
            }
        }
    }

    NetIo_disconnect(xNetIoHandle);
    NetIo_terminate(xNetIoHandle);
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStContentLength);
    STRING_delete(xStHttpBody);

    return xRes;
}

int Kvs_putMediaStart(KvsServiceParameter_t *pServPara, KvsPutMediaParameter_t *pPutMediaPara, unsigned int *puHttpStatusCode, PutMediaHandle *pPutMediaHandle)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = NULL;

    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};
    STRING_HANDLE xStProducerStartTimestamp = NULL;
    uint64_t uProducerStartTimestamp = 0;

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;
    bool bKeepNetIo = false;

    if (prvValidateServiceParameter(pServPara) != KVS_ERRNO_NONE || prvValidatePutMediaParameter(pPutMediaPara) != KVS_ERRNO_NONE || pPutMediaHandle == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        ((uProducerStartTimestamp = (pPutMediaPara->uProducerStartTimestampMs == 0) ? getEpochTimestampInMs() : pPutMediaPara->uProducerStartTimestampMs) == 0) ||
        (xStProducerStartTimestamp = STRING_construct_sprintf("%." PRIu64 ".%03d", uProducerStartTimestamp / 1000, uProducerStartTimestamp % 1000)) == NULL)
    {
        LogError("Failed to get epoch time");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL || HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcPutMediaEndpoint) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONNECTION, VAL_KEEP_ALIVE) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_TRANSFER_ENCODING, VAL_TRANSFER_ENCODING_CHUNKED) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)) ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZN_FRAG_ACK_REQUIRED, VAL_FRAGMENT_ACK_REQUIRED_TRUE) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZN_FRAG_T_TYPE, prvGetTimecodeValue(pPutMediaPara->xTimecodeType)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZN_PRODUCER_START_T, STRING_c_str(xStProducerStartTimestamp)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZN_STREAM_NAME, pPutMediaPara->pcStreamName) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, "expect", "100-continue") != HTTP_HEADERS_OK)
    {
        LogError("Failed to generate HTTP headers");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_PUT_MEDIA, URI_QUERY_EMPTY, xHttpReqHeaders, HTTP_BODY_EMPTY)) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        LogError("Failed to sign");
        xRes = KVS_ERRNO_FAIL;
    }
    else if (
        (xNetIoHandle = NetIo_create()) == NULL || NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs) != KVS_ERRNO_NONE ||
        NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs) != KVS_ERRNO_NONE || NetIo_connect(xNetIoHandle, pServPara->pcPutMediaEndpoint, PORT_HTTPS) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s\r\n", pServPara->pcPutMediaEndpoint);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_PUT_MEDIA, xHttpReqHeaders, HTTP_BODY_EMPTY) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else if (Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen))
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        *puHttpStatusCode = uHttpStatusCode;

        if (uHttpStatusCode == 200)
        {
            if ((pPutMedia = (PutMedia_t *)kvsMalloc(sizeof(PutMedia_t))) == NULL)
            {
                LogError("OOM: pPutMedia");
                xRes = KVS_ERRNO_FAIL;
            }
            else
            {
                /* Change network I/O receiving timeout for streaming purpose. */
                NetIo_setRecvTimeout(xNetIoHandle, pPutMediaPara->uRecvTimeoutMs);
                NetIo_setSendTimeout(xNetIoHandle, pPutMediaPara->uSendTimeoutMs);

                memset(pPutMedia, 0, sizeof(PutMedia_t));
                pPutMedia->xNetIoHandle = xNetIoHandle;
                *pPutMediaHandle = pPutMedia;
                bKeepNetIo = true;
            }
        }
    }

    if (!bKeepNetIo)
    {
        NetIo_disconnect(xNetIoHandle);
        NetIo_terminate(xNetIoHandle);
    }
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStProducerStartTimestamp);

    return xRes;
}

int Kvs_putMediaUpdate(PutMediaHandle xPutMediaHandle, uint8_t *pMkvHeader, size_t uMkvHeaderLen, uint8_t *pData, size_t uDataLen)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    int xChunkedHeaderLen = 0;
    char pcChunkedHeader[sizeof(size_t) * 2 + 3];
    const char *pcChunkedEnd = "\r\n";

    if (pData == NULL)
    {
        uDataLen = 0;
    }

    if (pPutMedia == NULL || pMkvHeader == NULL || uMkvHeaderLen == 0)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        xChunkedHeaderLen = snprintf(pcChunkedHeader, sizeof(pcChunkedHeader), "%lx\r\n", (unsigned long)(uMkvHeaderLen + uDataLen));
        if (xChunkedHeaderLen <= 0)
        {
            LogError("Failed to init chunk size");
            xRes = KVS_ERRNO_FAIL;
        }
        else
        {
            if (NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedHeader, (size_t)xChunkedHeaderLen) != 0 ||
                NetIo_send(pPutMedia->xNetIoHandle, pMkvHeader, uMkvHeaderLen) != 0 ||
                (pData != NULL && uDataLen > 0 && NetIo_send(pPutMedia->xNetIoHandle, pData, uDataLen) != 0) ||
                NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedEnd, strlen(pcChunkedEnd)) != 0)
            {
                LogError("Failed to send data frame");
                xRes = KVS_ERRNO_FAIL;
            }
            else
            {
                /* nop */
            }
        }
    }

    return xRes;
}

int Kvs_putMediaUpdateRaw(PutMediaHandle xPutMediaHandle, uint8_t *pBuf, size_t uLen)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    int xChunkedHeaderLen = 0;
    char pcChunkedHeader[sizeof(size_t) * 2 + 3];
    const char *pcChunkedEnd = "\r\n";

    if (pPutMedia == NULL || pBuf == NULL || uLen == 0)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        xChunkedHeaderLen = snprintf(pcChunkedHeader, sizeof(pcChunkedHeader), "%lx\r\n", (unsigned long)uLen);
        if (xChunkedHeaderLen <= 0)
        {
            LogError("Failed to init chunk size");
            xRes = KVS_ERRNO_FAIL;
        }
        else
        {
            if (NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedHeader, (size_t)xChunkedHeaderLen) != 0 ||
                NetIo_send(pPutMedia->xNetIoHandle, pBuf, uLen) != 0 || NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedEnd, strlen(pcChunkedEnd)) != 0)
            {
                LogError("Failed to send data frame");
                xRes = KVS_ERRNO_FAIL;
            }
            else
            {
                /* nop */
            }
        }
    }

    return xRes;
}

int Kvs_putMediaDoWork(PutMediaHandle xPutMediaHandle)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    BUFFER_HANDLE xBufRecv = NULL;
    size_t uBytesTotalReceived = 0;
    size_t uBytesReceived = 0;
    FragmentAck_t xFragmentAck = {0};
    size_t uFragAckLen = 0;

    if (pPutMedia == NULL)
    {
        LogError("Invalid argument");
        xRes = KVS_ERRNO_FAIL;
    }
    else if ((xBufRecv = BUFFER_create_with_size(DEFAULT_RECV_BUFSIZE)) == NULL)
    {
        LogError("OOM: xBufRecv");
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        while (NetIo_isDataAvailable(pPutMedia->xNetIoHandle))
        {
            if (BUFFER_length(xBufRecv) == uBytesTotalReceived && BUFFER_enlarge(xBufRecv, BUFFER_length(xBufRecv) * 2) != 0)
            {
                LogError("OOM: xBufRecv");
                xRes = KVS_ERRNO_FAIL;
                break;
            }

            if (NetIo_recv(pPutMedia->xNetIoHandle, BUFFER_u_char(xBufRecv) + uBytesTotalReceived, BUFFER_length(xBufRecv) - uBytesTotalReceived, &uBytesReceived) !=
                KVS_ERRNO_NONE)
            {
                LogError("Failed to receive");
                xRes = KVS_ERRNO_FAIL;
                break;
            }

            uBytesTotalReceived += uBytesReceived;
        }

        if (xRes == KVS_ERRNO_NONE && uBytesTotalReceived > 0)
        {
            uBytesReceived = 0;
            while (uBytesReceived < uBytesTotalReceived)
            {
                memset(&xFragmentAck, 0, sizeof(FragmentAck_t));
                if (prvParseFragmentAck((char *)BUFFER_u_char(xBufRecv) + uBytesReceived, uBytesTotalReceived - uBytesReceived, &xFragmentAck, &uFragAckLen) != KVS_ERRNO_NONE ||
                    uFragAckLen == 0)
                {
                    break;
                }
                else
                {
                    prvLogFragmentAck(&xFragmentAck);
                    if (xFragmentAck.eventType == EVENT_ERROR)
                    {
                        pPutMedia->uLastErrorId = xFragmentAck.uErrorId;
                        xRes = KVS_ERRNO_FAIL;
                        break;
                    }
                }
                uBytesReceived += uFragAckLen;
            }
        }
    }

    BUFFER_delete(xBufRecv);

    return xRes;
}

void Kvs_putMediaFinish(PutMediaHandle xPutMediaHandle)
{
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia != NULL)
    {
        if (pPutMedia->xNetIoHandle != NULL)
        {
            NetIo_disconnect(pPutMedia->xNetIoHandle);
            NetIo_terminate(pPutMedia->xNetIoHandle);
        }
        kvsFree(pPutMedia);
    }
}

int Kvs_putMediaUpdateRecvTimeout(PutMediaHandle xPutMediaHandle, unsigned int uRecvTimeoutMs)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia == NULL || pPutMedia->xNetIoHandle == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (NetIo_setRecvTimeout(pPutMedia->xNetIoHandle, uRecvTimeoutMs) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        /* nop */
    }

    return xRes;
}

int Kvs_putMediaUpdateSendTimeout(PutMediaHandle xPutMediaHandle, unsigned int uSendTimeoutMs)
{
    int xRes = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia == NULL || pPutMedia->xNetIoHandle == NULL)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else if (NetIo_setSendTimeout(pPutMedia->xNetIoHandle, uSendTimeoutMs) != KVS_ERRNO_NONE)
    {
        xRes = KVS_ERRNO_FAIL;
    }
    else
    {
        /* nop */
    }

    return xRes;
}

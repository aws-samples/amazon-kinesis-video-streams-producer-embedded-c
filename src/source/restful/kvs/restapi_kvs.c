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
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/httpheaders.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/xlogging.h"
#include "parson.h"

/* Public headers */
#include "kvs/errors.h"
#include "kvs/restapi.h"

/* Platform dependent headers */
#include "kvs/port.h"

/* Internal headers */
#include "os/allocator.h"
#include "restful/aws_signer_v4.h"
#include "misc/json_helper.h"
#include "net/http_helper.h"
#include "net/netio.h"

#ifndef SAFE_FREE
#define SAFE_FREE(a) \
    do               \
    {                \
        kvsFree(a);  \
        a = NULL;    \
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

typedef struct
{
    ePutMediaFragmentAckEventType eventType;
    uint64_t uFragmentTimecode;
    unsigned int uErrorId;

    DLIST_ENTRY xAckEntry;
} FragmentAck_t;

typedef struct PutMedia
{
    LOCK_HANDLE xLock;

    NetIoHandle xNetIoHandle;
    DLIST_ENTRY xPendingFragmentAcks;
} PutMedia_t;

#define JSON_KEY_EVENT_TYPE "EventType"
#define JSON_KEY_FRAGMENT_TIMECODE "FragmentTimecode"
#define JSON_KEY_ERROR_ID "ErrorId"

#define EVENT_TYPE_BUFFERING "\"BUFFERING\""
#define EVENT_TYPE_RECEIVED "\"RECEIVED\""
#define EVENT_TYPE_PERSISTED "\"PERSISTED\""
#define EVENT_TYPE_ERROR "\"ERROR\""
#define EVENT_TYPE_IDLE "\"IDLE\""

/*-----------------------------------------------------------*/

static int prvValidateServiceParameter(KvsServiceParameter_t *pServPara)
{
    if (pServPara == NULL || pServPara->pcAccessKey == NULL || pServPara->pcSecretKey == NULL || pServPara->pcRegion == NULL || pServPara->pcService == NULL ||
        pServPara->pcHost == NULL)
    {
        return KVS_ERROR_INVALID_ARGUMENT;
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
        return KVS_ERROR_INVALID_ARGUMENT;
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
        return KVS_ERROR_INVALID_ARGUMENT;
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
        return KVS_ERROR_INVALID_ARGUMENT;
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
        return KVS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        return KVS_ERRNO_NONE;
    }
}

static AwsSigV4Handle prvSign(KvsServiceParameter_t *pServPara, char *pcUri, char *pcQuery, HTTP_HEADERS_HANDLE xHeadersToSign, const char *pcHttpBody)
{
    int res = KVS_ERRNO_NONE;

    AwsSigV4Handle xAwsSigV4Handle = NULL;
    const char *pcVal;
    const char *pcXAmzDate;

    if ((xAwsSigV4Handle = AwsSigV4_Create(HTTP_METHOD_POST, pcUri, pcQuery)) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_CREATE_SIGV4_HANDLE;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_CONNECTION)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_CONNECTION, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_HOST)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_HOST, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_TRANSFER_ENCODING)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_TRANSFER_ENCODING, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if ((pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_USER_AGENT)) != NULL && AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_USER_AGENT, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcXAmzDate = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZ_DATE)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZ_DATE, pcXAmzDate) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZ_SECURITY_TOKEN)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZ_SECURITY_TOKEN, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_FRAG_ACK_REQUIRED)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_FRAG_ACK_REQUIRED, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_FRAG_T_TYPE)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_FRAG_T_TYPE, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_PRODUCER_START_T)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_PRODUCER_START_T, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (
        (pcVal = HTTPHeaders_FindHeaderValue(xHeadersToSign, HDR_X_AMZN_STREAM_NAME)) != NULL &&
        AwsSigV4_AddCanonicalHeader(xAwsSigV4Handle, HDR_X_AMZN_STREAM_NAME, pcVal) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_ADD_CANONICAL_HEADER;
    }
    else if (AwsSigV4_AddCanonicalBody(xAwsSigV4Handle, pcHttpBody, strlen(pcHttpBody)) != KVS_ERRNO_NONE)
    {
        res = KVS_ERRNO_FAIL;
    }
    else if ((res = AwsSigV4_Sign(xAwsSigV4Handle, pServPara->pcAccessKey, pServPara->pcSecretKey, pServPara->pcRegion, pServPara->pcService, pcXAmzDate)) != KVS_ERRNO_NONE)
    {
        /* Propagate the res error */
    }
    else
    {
        /* nop */
    }

    if (res != KVS_ERRNO_NONE)
    {
        AwsSigV4_Terminate(xAwsSigV4Handle);
        xAwsSigV4Handle = NULL;
    }

    return xAwsSigV4Handle;
}

static int prvParseDataEndpoint(const char *pcJsonSrc, size_t uJsonSrcLen, char **ppcEndpoint)
{
    int res = KVS_ERRNO_NONE;
    STRING_HANDLE xStJson = NULL;
    JSON_Value *pxRootValue = NULL;
    JSON_Object *pxRootObject = NULL;
    char *pcDataEndpoint = NULL;
    size_t uEndpointLen = 0;

    json_set_escape_slashes(0);

    if (pcJsonSrc == NULL || uJsonSrcLen == 0 || ppcEndpoint == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((xStJson = STRING_construct_n(pcJsonSrc, uJsonSrcLen)) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
        LogError("OOM: parse data endpoint");
    }
    else if (
        (pxRootValue = json_parse_string(STRING_c_str(xStJson))) == NULL || (pxRootObject = json_value_get_object(pxRootValue)) == NULL ||
        (pcDataEndpoint = json_object_dotget_serialize_to_string(pxRootObject, "DataEndpoint", true)) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_PARSE_DATA_ENDPOINT;
        LogError("Failed to parse data endpoint");
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

    return res;
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
        LogError("Invalid timecode type:%d", xTimecodeType);
        return "";
    }
}

static int prvGetEpochTimestampInStr(uint64_t uProducerStartTimestampMs, STRING_HANDLE *pxStProducerStartTimestamp)
{
    int res = KVS_ERRNO_NONE;
    uint64_t uProducerStartTimestamp = 0;
    STRING_HANDLE xStProducerStartTimestamp = NULL;

    uProducerStartTimestamp = (uProducerStartTimestampMs == 0) ? getEpochTimestampInMs() : uProducerStartTimestampMs;
    xStProducerStartTimestamp = STRING_construct_sprintf("%." PRIu64 ".%03d", uProducerStartTimestamp / 1000, uProducerStartTimestamp % 1000);

    if (xStProducerStartTimestamp == NULL)
    {
        res = KVS_ERROR_C_UTIL_STRING_ERROR;
    }
    else
    {
        *pxStProducerStartTimestamp = xStProducerStartTimestamp;
    }

    return res;
}

static int prvParseFragmentAckLength(char *pcSrc, size_t uLen, size_t *puMsgLen, size_t *puBytesRead)
{
    int res = KVS_ERRNO_NONE;
    size_t uMsgLen = 0;
    size_t uBytesRead = 0;
    size_t i = 0;
    char c = 0;

    if (pcSrc == NULL || uLen == 0 || puMsgLen == NULL || puBytesRead == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
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
                res = KVS_ERROR_FAIL_TO_PARSE_FRAGMENT_ACK_LENGTH;
            }
        }
    }

    if (res == KVS_ERRNO_NONE)
    {
        if (uBytesRead < 3 || (uBytesRead + uMsgLen + 2) > uLen || pcSrc[uBytesRead + uMsgLen] != '\r' || pcSrc[uBytesRead + uMsgLen + 1] != '\n')
        {
            res = KVS_ERROR_FAIL_TO_PARSE_FRAGMENT_ACK_LENGTH;
        }
        else
        {
            *puMsgLen = uMsgLen;
            *puBytesRead = uBytesRead;
        }
    }

    return res;
}

static ePutMediaFragmentAckEventType prvGetEventType(char *pcEventType)
{
    ePutMediaFragmentAckEventType ev = eUnknown;

    if (pcEventType != NULL)
    {
        if (strncmp(pcEventType, EVENT_TYPE_BUFFERING, sizeof(EVENT_TYPE_BUFFERING) - 1) == 0)
        {
            ev = eBuffering;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_RECEIVED, sizeof(EVENT_TYPE_RECEIVED) - 1) == 0)
        {
            ev = eReceived;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_PERSISTED, sizeof(EVENT_TYPE_PERSISTED) - 1) == 0)
        {
            ev = ePersisted;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_ERROR, sizeof(EVENT_TYPE_ERROR) - 1) == 0)
        {
            ev = eError;
        }
        else if (strncmp(pcEventType, EVENT_TYPE_IDLE, sizeof(EVENT_TYPE_IDLE) - 1) == 0)
        {
            ev = eIdle;
        }
    }

    return ev;
}

static int parseFragmentMsg(const char *pcFragmentMsg, FragmentAck_t *pxFragmentAck)
{
    int res = KVS_ERRNO_NONE;
    JSON_Value *pxRootValue = NULL;
    JSON_Object *pxRootObject = NULL;
    char *pcEventType = NULL;

    json_set_escape_slashes(0);

    if (pcFragmentMsg == NULL || pxFragmentAck == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((pxRootValue = json_parse_string(pcFragmentMsg)) == NULL || (pxRootObject = json_value_get_object(pxRootValue)) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_PARSE_FRAGMENT_ACK_MSG;
        LogInfo("Failed to parse fragment msg:%s", pcFragmentMsg);
    }
    else if ((pcEventType = json_object_dotget_serialize_to_string(pxRootObject, JSON_KEY_EVENT_TYPE, false)) == NULL)
    {
        res = KVS_ERROR_UNKNOWN_FRAGMENT_ACK_TYPE;
        LogInfo("Unknown fragment ack:%s", pcFragmentMsg);
    }
    else
    {
        pxFragmentAck->eventType = prvGetEventType(pcEventType);
        kvsFree(pcEventType);

        if (pxFragmentAck->eventType == eBuffering || pxFragmentAck->eventType == eReceived || pxFragmentAck->eventType == ePersisted ||
            pxFragmentAck->eventType == eError)
        {
            pxFragmentAck->uFragmentTimecode = json_object_dotget_uint64(pxRootObject, JSON_KEY_FRAGMENT_TIMECODE, 10);
            if (pxFragmentAck->eventType == eError)
            {
                pxFragmentAck->uErrorId = (unsigned int)json_object_dotget_uint64(pxRootObject, JSON_KEY_ERROR_ID, 10);
            }
        }
    }

    if (pxRootValue != NULL)
    {
        json_value_free(pxRootValue);
    }

    return res;
}

static int prvParseFragmentAck(char *pcSrc, size_t uLen, FragmentAck_t *pxFragAck, size_t *puFragAckLen)
{
    int res = KVS_ERRNO_NONE;
    size_t uMsgLen = 0;
    size_t uBytesRead = 0;
    STRING_HANDLE xStFragMsg = NULL;

    if (pcSrc == NULL || uLen == 0 || pxFragAck == NULL || puFragAckLen == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((res = prvParseFragmentAckLength(pcSrc, uLen, &uMsgLen, &uBytesRead)) != KVS_ERRNO_NONE)
    {
        LogInfo("Unknown fragment ack:%.*s", (int)uLen, pcSrc);
        /* Propagate the res error */
    }
    else if ((xStFragMsg = STRING_construct_n(pcSrc + uBytesRead, uMsgLen)) == NULL ||
             parseFragmentMsg(STRING_c_str(xStFragMsg), pxFragAck) != KVS_ERRNO_NONE)
    {
        res = KVS_ERROR_FAIL_TO_PARSE_FRAGMENT_ACK_MSG;
        LogInfo("Failed to parse fragment ack");
    }
    else
    {
        *puFragAckLen = uBytesRead + uMsgLen + 2;
    }

    STRING_delete(xStFragMsg);

    return res;
}

static void prvLogFragmentAck(FragmentAck_t *pFragmentAck)
{
    if (pFragmentAck != NULL)
    {
        if (pFragmentAck->eventType == eBuffering)
        {
            LogInfo("Fragment buffering, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == eReceived)
        {
            LogInfo("Fragment received, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == ePersisted)
        {
            LogInfo("Fragment persisted, timecode:%" PRIu64 "", pFragmentAck->uFragmentTimecode);
        }
        else if (pFragmentAck->eventType == eError)
        {
            LogError("PutMedia session error id:%d", pFragmentAck->uErrorId);
        }
        else if (pFragmentAck->eventType == eIdle)
        {
            LogInfo("PutMedia session Idle");
        }
        else
        {
            LogInfo("Unknown Fragment Ack");
        }
    }
}

static void prvLogPendingFragmentAcks(PutMedia_t *pPutMedia)
{
    PDLIST_ENTRY pxListHead = NULL;
    PDLIST_ENTRY pxListItem = NULL;
    FragmentAck_t *pFragmentAck = NULL;

    if (pPutMedia != NULL && Lock(pPutMedia->xLock) == LOCK_OK)
    {
        pxListHead = &(pPutMedia->xPendingFragmentAcks);
        pxListItem = pxListHead->Flink;
        while (pxListItem != pxListHead)
        {
            pFragmentAck = containingRecord(pxListItem, FragmentAck_t, xAckEntry);
            prvLogFragmentAck(pFragmentAck);

            pxListItem = pxListItem->Flink;
        }

        Unlock(pPutMedia->xLock);
    }
}

static int prvPushFragmentAck(PutMedia_t *pPutMedia, FragmentAck_t *pFragmentAckSrc)
{
    int res = KVS_ERRNO_NONE;
    FragmentAck_t *pFragmentAck = NULL;

    if (pPutMedia == NULL || pFragmentAckSrc == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((pFragmentAck = (FragmentAck_t *)kvsMalloc(sizeof(FragmentAck_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        memcpy(pFragmentAck, pFragmentAckSrc, sizeof(FragmentAck_t));
        DList_InitializeListHead(&(pFragmentAck->xAckEntry));

        if (Lock(pPutMedia->xLock) != LOCK_OK)
        {
            res = KVS_ERROR_LOCK_ERROR;
        }
        else
        {
            DList_InsertTailList(&(pPutMedia->xPendingFragmentAcks), &(pFragmentAck->xAckEntry));

            Unlock(pPutMedia->xLock);
        }
    }

    if (res != KVS_ERRNO_NONE)
    {
        if (pFragmentAck != NULL)
        {
            kvsFree(pFragmentAck);
        }
    }

    return res;
}

static PutMedia_t *prvCreateDefaultPutMediaHandle()
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = NULL;

    if ((pPutMedia = (PutMedia_t *)kvsMalloc(sizeof(PutMedia_t))) == NULL)
    {
        res = KVS_ERROR_OUT_OF_MEMORY;
        LogError("OOM: pPutMedia");
    }
    else
    {
        memset(pPutMedia, 0, sizeof(PutMedia_t));

        if ((pPutMedia->xLock = Lock_Init()) == NULL)
        {
            res = KVS_ERROR_LOCK_ERROR;
            LogError("Failed to initialize lock");
        }
        else
        {
            DList_InitializeListHead(&(pPutMedia->xPendingFragmentAcks));
        }
    }

    if (res != KVS_ERRNO_NONE)
    {
        if (pPutMedia != NULL)
        {
            if (pPutMedia->xLock != NULL)
            {
                Lock_Deinit(pPutMedia->xLock);
            }
            kvsFree(pPutMedia);
            pPutMedia = NULL;
        }
    }

    return pPutMedia;
}

static FragmentAck_t *prvReadFragmentAck(PutMedia_t *pPutMedia)
{
    int res = KVS_ERRNO_NONE;
    PDLIST_ENTRY pxListHead = NULL;
    PDLIST_ENTRY pxListItem = NULL;
    FragmentAck_t *pFragmentAck = NULL;

    if (Lock(pPutMedia->xLock) == LOCK_OK)
    {
        if (!DList_IsListEmpty(&(pPutMedia->xPendingFragmentAcks)))
        {
            pxListHead = &(pPutMedia->xPendingFragmentAcks);
            pxListItem = DList_RemoveHeadList(pxListHead);
            pFragmentAck = containingRecord(pxListItem, FragmentAck_t, xAckEntry);
        }
        Unlock(pPutMedia->xLock);
    }

    return pFragmentAck;
}

static void prvFlushFragmentAck(PutMedia_t *pPutMedia)
{
    FragmentAck_t *pFragmentAck = NULL;
    while ((pFragmentAck = prvReadFragmentAck(pPutMedia)) != NULL)
    {
        kvsFree(pFragmentAck);
    }
}

int Kvs_describeStream(KvsServiceParameter_t *pServPara, KvsDescribeStreamParameter_t *pDescPara, unsigned int *puHttpStatusCode)
{
    int res = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (puHttpStatusCode != NULL)
    {
        *puHttpStatusCode = 0; /* Set to zero to avoid misuse from the previous value. */
    }

    if ((res = prvValidateServiceParameter(pServPara)) != KVS_ERRNO_NONE ||
        (res = prvValidateDescribeStreamParameter(pDescPara)) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        /* Propagate the res error */
    }
    else if ((res = getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate))) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        /* Propagate the res error */
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(DESCRIBE_STREAM_HTTP_BODY_TEMPLATE, pDescPara->pcStreamName)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        res = KVS_ERROR_UNABLE_TO_ALLOCATE_HTTP_BODY;
        LogError("Failed to allocate HTTP body");
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        res = KVS_ERROR_UNABLE_TO_GENERATE_HTTP_HEADER;
        LogError("Failed to generate HTTP headers");
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_DESCRIBE_STREAM, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        res = KVS_ERROR_FAIL_TO_SIGN_HTTP_REQ;
        LogError("Failed to sign");
    }
    else if ((xNetIoHandle = NetIo_create()) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_CREATE_NETIO_HANDLE;
        LogError("Failed to create NetIo handle");
    }
    else if (
        (res = NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_DESCRIBE_STREAM, xHttpReqHeaders, STRING_c_str(xStHttpBody))) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen)) != KVS_ERRNO_NONE)
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else
    {
        if (puHttpStatusCode != NULL)
        {
            *puHttpStatusCode = uHttpStatusCode;
        }

        if (uHttpStatusCode != 200)
        {
            LogInfo("Describe Stream failed, HTTP status code: %u", uHttpStatusCode);
            LogInfo("HTTP response message:%.*s", (int)uRspBodyLen, pRspBody);
        }
    }

    NetIo_disconnect(xNetIoHandle);
    NetIo_terminate(xNetIoHandle);
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStContentLength);
    STRING_delete(xStHttpBody);

    return res;
}

int Kvs_createStream(KvsServiceParameter_t *pServPara, KvsCreateStreamParameter_t *pCreatePara, unsigned int *puHttpStatusCode)
{
    int res = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (puHttpStatusCode != NULL)
    {
        *puHttpStatusCode = 0; /* Set to zero to avoid misuse from previous value. */
    }

    if ((res = prvValidateServiceParameter(pServPara)) != KVS_ERRNO_NONE ||
        (res = prvValidateCreateStreamParameter(pCreatePara)) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        /* Propagate the res error */
    }
    else if ((res = getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate))) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        /* Propagate the res error */
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(CREATE_STREAM_HTTP_BODY_TEMPLATE, pCreatePara->pcStreamName, pCreatePara->uDataRetentionInHours)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        res = KVS_ERROR_UNABLE_TO_ALLOCATE_HTTP_BODY;
        LogError("Failed to allocate HTTP body");
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        res = KVS_ERROR_UNABLE_TO_GENERATE_HTTP_HEADER;
        LogError("Failed to generate HTTP headers");
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_CREATE_STREAM, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        LogError("Failed to sign");
        res = KVS_ERROR_FAIL_TO_SIGN_HTTP_REQ;
    }
    else if ((xNetIoHandle = NetIo_create()) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_CREATE_NETIO_HANDLE;
        LogError("Failed to create NetIo handle");
    }
    else if (
        (res = NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_CREATE_STREAM, xHttpReqHeaders, STRING_c_str(xStHttpBody))) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen)) != KVS_ERRNO_NONE)
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else
    {
        if (puHttpStatusCode != NULL)
        {
            *puHttpStatusCode = uHttpStatusCode;
        }

        if (uHttpStatusCode != 200)
        {
            LogInfo("Create Stream failed, HTTP status code: %u", uHttpStatusCode);
            LogInfo("HTTP response message:%.*s", (int)uRspBodyLen, pRspBody);
        }
    }

    NetIo_disconnect(xNetIoHandle);
    NetIo_terminate(xNetIoHandle);
    SAFE_FREE(pRspBody);
    HTTPHeaders_Free(xHttpReqHeaders);
    AwsSigV4_Terminate(xAwsSigV4Handle);
    STRING_delete(xStContentLength);
    STRING_delete(xStHttpBody);

    return res;
}

int Kvs_getDataEndpoint(KvsServiceParameter_t *pServPara, KvsGetDataEndpointParameter_t *pGetDataEpPara, unsigned int *puHttpStatusCode, char **ppcDataEndpoint)
{
    int res = KVS_ERRNO_NONE;

    STRING_HANDLE xStHttpBody = NULL;
    STRING_HANDLE xStContentLength = NULL;
    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};

    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;

    if (puHttpStatusCode != NULL)
    {
        *puHttpStatusCode = 0; /* Set to zero to avoid misuse from previous value. */
    }

    if ((res = prvValidateServiceParameter(pServPara)) != KVS_ERRNO_NONE ||
        (res = prvValidateGetDataEndpointParameter(pGetDataEpPara)) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        /* Propagate the res error */
    }
    else if ((res = getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate))) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        /* Propagate the res error */
    }
    else if (
        (xStHttpBody = STRING_construct_sprintf(GET_DATA_ENDPOINT_HTTP_BODY_TEMPLATE, pGetDataEpPara->pcStreamName)) == NULL ||
        (xStContentLength = STRING_construct_sprintf("%u", STRING_length(xStHttpBody))) == NULL)
    {
        res = KVS_ERROR_UNABLE_TO_ALLOCATE_HTTP_BODY;
        LogError("Failed to allocate HTTP body");
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcHost) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_ACCEPT, VAL_ACCEPT_ANY) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_LENGTH, STRING_c_str(xStContentLength)) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_CONTENT_TYPE, VAL_CONTENT_TYPE_APPLICATION_jSON) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_USER_AGENT, VAL_USER_AGENT) != HTTP_HEADERS_OK ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_DATE, pcXAmzDate) != HTTP_HEADERS_OK ||
        (pServPara->pcToken != NULL && (HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_X_AMZ_SECURITY_TOKEN, pServPara->pcToken) != HTTP_HEADERS_OK)))
    {
        res = KVS_ERROR_UNABLE_TO_GENERATE_HTTP_HEADER;
        LogError("Failed to generate HTTP headers");
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_GET_DATA_ENDPOINT, URI_QUERY_EMPTY, xHttpReqHeaders, STRING_c_str(xStHttpBody))) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        res = KVS_ERROR_FAIL_TO_SIGN_HTTP_REQ;
        LogError("Failed to sign");
    }
    else if ((xNetIoHandle = NetIo_create()) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_CREATE_NETIO_HANDLE;
        LogError("Failed to create NetIo handle");
    }
    else if (
        (res = NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_connect(xNetIoHandle, pServPara->pcHost, PORT_HTTPS)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_GET_DATA_ENDPOINT, xHttpReqHeaders, STRING_c_str(xStHttpBody))) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen)) != KVS_ERRNO_NONE)
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        /* Propagate the res error */
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
            if ((res = prvParseDataEndpoint(pRspBody, uRspBodyLen, ppcDataEndpoint)) != KVS_ERRNO_NONE)
            {
                LogError("Failed to parse data endpoint");
                /* Propagate the res error */
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

    return res;
}

int Kvs_putMediaStart(KvsServiceParameter_t *pServPara, KvsPutMediaParameter_t *pPutMediaPara, unsigned int *puHttpStatusCode, PutMediaHandle *pPutMediaHandle)
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = NULL;

    char pcXAmzDate[DATE_TIME_ISO_8601_FORMAT_STRING_SIZE] = {0};
    STRING_HANDLE xStProducerStartTimestamp = NULL;


    AwsSigV4Handle xAwsSigV4Handle = NULL;

    unsigned int uHttpStatusCode = 0;
    HTTP_HEADERS_HANDLE xHttpReqHeaders = NULL;
    char *pRspBody = NULL;
    size_t uRspBodyLen = 0;

    NetIoHandle xNetIoHandle = NULL;
    bool bKeepNetIo = false;

    if (puHttpStatusCode != NULL)
    {
        *puHttpStatusCode = 0; /* Set to zero to avoid misuse from previous value. */
    }

    if ((res = prvValidateServiceParameter(pServPara)) != KVS_ERRNO_NONE ||
        (res = prvValidatePutMediaParameter(pPutMediaPara)) != KVS_ERRNO_NONE)
    {
        LogError("Invalid argument");
        /* Propagate the res error */
    }
    else if (pPutMediaHandle == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else if ((res = getTimeInIso8601(pcXAmzDate, sizeof(pcXAmzDate))) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get time");
        /* Propagate the res error */
    }
    else if ((res = prvGetEpochTimestampInStr(pPutMediaPara->uProducerStartTimestampMs, &xStProducerStartTimestamp)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to get epoch time");
        /* Propagate the res error */
    }
    else if (
        (xHttpReqHeaders = HTTPHeaders_Alloc()) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_HOST, pServPara->pcPutMediaEndpoint) != HTTP_HEADERS_OK ||
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
        res = KVS_ERROR_UNABLE_TO_GENERATE_HTTP_HEADER;
        LogError("Failed to generate HTTP headers");
    }
    else if (
        (xAwsSigV4Handle = prvSign(pServPara, KVS_URI_PUT_MEDIA, URI_QUERY_EMPTY, xHttpReqHeaders, HTTP_BODY_EMPTY)) == NULL ||
        HTTPHeaders_AddHeaderNameValuePair(xHttpReqHeaders, HDR_AUTHORIZATION, AwsSigV4_GetAuthorization(xAwsSigV4Handle)) != HTTP_HEADERS_OK)
    {
        res = KVS_ERROR_FAIL_TO_SIGN_HTTP_REQ;
        LogError("Failed to sign");
    }
    else if ((xNetIoHandle = NetIo_create()) == NULL)
    {
        res = KVS_ERROR_FAIL_TO_CREATE_NETIO_HANDLE;
        LogError("Failed to create NetIo handle");
    }
    else if (
        (res = NetIo_setRecvTimeout(xNetIoHandle, pServPara->uRecvTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_setSendTimeout(xNetIoHandle, pServPara->uSendTimeoutMs)) != KVS_ERRNO_NONE ||
        (res = NetIo_connect(xNetIoHandle, pServPara->pcPutMediaEndpoint, PORT_HTTPS)) != KVS_ERRNO_NONE)
    {
        LogError("Failed to connect to %s", pServPara->pcPutMediaEndpoint);
        /* Propagate the res error */
    }
    else if ((res = Http_executeHttpReq(xNetIoHandle, HTTP_METHOD_POST, KVS_URI_PUT_MEDIA, xHttpReqHeaders, HTTP_BODY_EMPTY)) != KVS_ERRNO_NONE)
    {
        LogError("Failed send http request to %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else if ((res = Http_recvHttpRsp(xNetIoHandle, &uHttpStatusCode, &pRspBody, &uRspBodyLen)) != KVS_ERRNO_NONE)
    {
        LogError("Failed recv http response from %s", pServPara->pcHost);
        /* Propagate the res error */
    }
    else
    {
        if (puHttpStatusCode != NULL)
        {
            *puHttpStatusCode = uHttpStatusCode;
        }

        if (uHttpStatusCode != 200)
        {
            LogInfo("Put Media failed, HTTP status code: %u", uHttpStatusCode);
            LogInfo("HTTP response message:%.*s", (int)uRspBodyLen, pRspBody);
        }
        else
        {
            if ((pPutMedia = prvCreateDefaultPutMediaHandle()) == NULL)
            {
                res = KVS_ERROR_FAIL_TO_CREATE_PUT_MEDIA_HANDLE;
                LogError("Failed to create pPutMedia");
            }
            else
            {
                /* Change network I/O receiving timeout for streaming purpose. */
                NetIo_setRecvTimeout(xNetIoHandle, pPutMediaPara->uRecvTimeoutMs);
                NetIo_setSendTimeout(xNetIoHandle, pPutMediaPara->uSendTimeoutMs);

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

    return res;
}

int Kvs_putMediaUpdate(PutMediaHandle xPutMediaHandle, uint8_t *pMkvHeader, size_t uMkvHeaderLen, uint8_t *pData, size_t uDataLen)
{
    int res = KVS_ERRNO_NONE;
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
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        xChunkedHeaderLen = snprintf(pcChunkedHeader, sizeof(pcChunkedHeader), "%lx\r\n", (unsigned long)(uMkvHeaderLen + uDataLen));
        if (xChunkedHeaderLen <= 0)
        {
            res = KVS_ERROR_C_UTIL_STRING_ERROR;
            LogError("Failed to init chunk size");
        }
        else
        {
            if ((res = NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedHeader, (size_t)xChunkedHeaderLen)) != KVS_ERRNO_NONE ||
                (res = NetIo_send(pPutMedia->xNetIoHandle, pMkvHeader, uMkvHeaderLen)) != KVS_ERRNO_NONE ||
                (pData != NULL && uDataLen > 0 && (res = NetIo_send(pPutMedia->xNetIoHandle, pData, uDataLen)) != KVS_ERRNO_NONE) ||
                (res = NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedEnd, strlen(pcChunkedEnd))) != KVS_ERRNO_NONE)
            {
                LogError("Failed to send data frame");
                /* Propagate the res error */
            }
            else
            {
                /* nop */
            }
        }
    }

    return res;
}

int Kvs_putMediaUpdateRaw(PutMediaHandle xPutMediaHandle, uint8_t *pBuf, size_t uLen)
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    int xChunkedHeaderLen = 0;
    char pcChunkedHeader[sizeof(size_t) * 2 + 3];
    const char *pcChunkedEnd = "\r\n";

    if (pPutMedia == NULL || pBuf == NULL || uLen == 0)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        xChunkedHeaderLen = snprintf(pcChunkedHeader, sizeof(pcChunkedHeader), "%lx\r\n", (unsigned long)uLen);
        if (xChunkedHeaderLen <= 0)
        {
            res = KVS_ERROR_C_UTIL_STRING_ERROR;
            LogError("Failed to init chunk size");
        }
        else
        {
            if ((res = NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedHeader, (size_t)xChunkedHeaderLen)) != KVS_ERRNO_NONE ||
                (res = NetIo_send(pPutMedia->xNetIoHandle, pBuf, uLen)) != KVS_ERRNO_NONE ||
                (res = NetIo_send(pPutMedia->xNetIoHandle, (const unsigned char *)pcChunkedEnd, strlen(pcChunkedEnd))) != KVS_ERRNO_NONE)
            {
                LogError("Failed to send data frame");
                /* Propagate the res error */
            }
            else
            {
                /* nop */
            }
        }
    }

    return res;
}

int Kvs_putMediaDoWork(PutMediaHandle xPutMediaHandle)
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    BUFFER_HANDLE xBufRecv = NULL;
    size_t uBytesTotalReceived = 0;
    size_t uBytesReceived = 0;
    FragmentAck_t xFragmentAck = {0};
    size_t uFragAckLen = 0;

    if (pPutMedia == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
        LogError("Invalid argument");
    }
    else
    {
        if (NetIo_isDataAvailable(pPutMedia->xNetIoHandle))
        {
            if ((xBufRecv = BUFFER_create_with_size(DEFAULT_RECV_BUFSIZE)) == NULL)
            {
                res = KVS_ERROR_C_UTIL_UNABLE_TO_CREATE_BUFFER;
                LogError("OOM: xBufRecv");
            }
            else
            {
                prvFlushFragmentAck(pPutMedia);
                while (NetIo_isDataAvailable(pPutMedia->xNetIoHandle))
                {
                    if (BUFFER_length(xBufRecv) == uBytesTotalReceived && BUFFER_enlarge(xBufRecv, BUFFER_length(xBufRecv) * 2) != 0)
                    {
                        res = KVS_ERROR_C_UTIL_UNABLE_TO_ENLARGE_BUFFER;
                        LogError("OOM: xBufRecv");
                        break;
                    }

                    if ((res = NetIo_recv(pPutMedia->xNetIoHandle, BUFFER_u_char(xBufRecv) + uBytesTotalReceived, BUFFER_length(xBufRecv) - uBytesTotalReceived, &uBytesReceived)) !=KVS_ERRNO_NONE)
                    {
                        LogError("Failed to receive");
                        /* Propagate the res error */
                        break;
                    }

                    uBytesTotalReceived += uBytesReceived;
                }

                if (res == KVS_ERRNO_NONE && uBytesTotalReceived > 0)
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
                            prvPushFragmentAck(pPutMedia, &xFragmentAck);
                            if (xFragmentAck.eventType == eError)
                            {
                                res = KVS_GENERATE_PUTMEDIA_ERROR(xFragmentAck.uErrorId);
                                break;
                            }
                        }
                        uBytesReceived += uFragAckLen;
                    }
                    // prvLogPendingFragmentAcks(pPutMedia);
                }
            }
        }
    }

    BUFFER_delete(xBufRecv);

    return res;
}

void Kvs_putMediaFinish(PutMediaHandle xPutMediaHandle)
{
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia != NULL)
    {
        prvFlushFragmentAck(pPutMedia);
        Lock_Deinit(pPutMedia->xLock);
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
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia == NULL || pPutMedia->xNetIoHandle == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((res = NetIo_setRecvTimeout(pPutMedia->xNetIoHandle, uRecvTimeoutMs)) != KVS_ERRNO_NONE)
    {
        /* Propagate the res error */
    }
    else
    {
        /* nop */
    }

    return res;
}

int Kvs_putMediaUpdateSendTimeout(PutMediaHandle xPutMediaHandle, unsigned int uSendTimeoutMs)
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;

    if (pPutMedia == NULL || pPutMedia->xNetIoHandle == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((res = NetIo_setSendTimeout(pPutMedia->xNetIoHandle, uSendTimeoutMs)) != KVS_ERRNO_NONE)
    {
        /* Propagate the res error */
    }
    else
    {
        /* nop */
    }

    return res;
}

int Kvs_putMediaReadFragmentAck(PutMediaHandle xPutMediaHandle, ePutMediaFragmentAckEventType *peAckEventType, uint64_t *puFragmentTimecode, unsigned int *puErrorId)
{
    int res = KVS_ERRNO_NONE;
    PutMedia_t *pPutMedia = xPutMediaHandle;
    FragmentAck_t *pFragmentAck = NULL;

    if (pPutMedia == NULL)
    {
        res = KVS_ERROR_INVALID_ARGUMENT;
    }
    else if ((pFragmentAck = prvReadFragmentAck(pPutMedia)) == NULL)
    {
        res = KVS_ERROR_NO_PUTMEDIA_FRAGMENT_ACK_AVAILABLE;
    }
    else
    {
        if (peAckEventType != NULL)
        {
            *peAckEventType = pFragmentAck->eventType;
        }
        if (puFragmentTimecode != NULL)
        {
            *puFragmentTimecode = pFragmentAck->uFragmentTimecode;
        }
        if (puErrorId != NULL)
        {
            *puErrorId = pFragmentAck->uErrorId;
        }
        kvsFree(pFragmentAck);
    }

    return res;
}

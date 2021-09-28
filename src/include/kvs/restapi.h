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

#ifndef KVS_REST_API_H
#define KVS_REST_API_H

#include <inttypes.h>

typedef struct
{
    char *pcAccessKey;
    char *pcSecretKey;
    char *pcToken;

    char *pcRegion;
    char *pcService;
    char *pcHost;

    char *pcPutMediaEndpoint;

    unsigned int uRecvTimeoutMs;
    unsigned int uSendTimeoutMs;
} KvsServiceParameter_t;

typedef struct
{
    char *pcStreamName;
} KvsDescribeStreamParameter_t;

typedef struct
{
    char *pcStreamName;
    unsigned int uDataRetentionInHours;
} KvsCreateStreamParameter_t;

typedef struct
{
    char *pcStreamName;
} KvsGetDataEndpointParameter_t;

typedef enum
{
    TIMECODE_TYPE_ABSOLUTE = 0,
    TIMECODE_TYPE_RELATIVE = 1
} FragmentTimecodeType_t;

typedef struct
{
    char *pcStreamName;
    FragmentTimecodeType_t xTimecodeType;
    uint64_t uProducerStartTimestampMs;

    unsigned int uRecvTimeoutMs;
    unsigned int uSendTimeoutMs;
} KvsPutMediaParameter_t;

typedef struct PutMedia *PutMediaHandle;

/* PUT MEDIA Fragment ACK event type */
typedef enum
{
    eUnknown = 0,
    eBuffering,
    eReceived,
    ePersisted,
    eError,
    eIdle
} ePutMediaFragmentAckEventType;

/**
 * @brief Describe stream
 *
 * Get the description of a stream to make sure the stream is available. It returns error if the stream does not exist
 * and therefore needs creation before using it.
 *
 * @param[in] pServPara The parameter for KVS service
 * @param[in] pDescPara The parameter for describe stream
 * @param[out] puHttpStatusCode The HTTP status code
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_describeStream(KvsServiceParameter_t *pServPara, KvsDescribeStreamParameter_t *pDescPara, unsigned int* puHttpStatusCode);

/**
 * @brief Create a stream
 *
 * @param[in] pServPara The parameter for KVS service
 * @param[in] pCreatePara The parameter for create stream
 * @param[out] puHttpStatusCode The HTTP status code
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_createStream(KvsServiceParameter_t *pServPara, KvsCreateStreamParameter_t *pCreatePara, unsigned int* puHttpStatusCode);

/**
 * @brief Get data endpoint for PUT MEDIA
 *
 * @param[in] pServPara The parameter for KVS service
 * @param[in] pGetDataEpPara The parameter for get data endpoint
 * @param[out] puHttpStatusCode The HTTP status code
 * @param[out] ppcDataEndpoint The endpoint address that is memory allocated.
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_getDataEndpoint(KvsServiceParameter_t *pServPara, KvsGetDataEndpointParameter_t *pGetDataEpPara, unsigned int* puHttpStatusCode, char **ppcDataEndpoint);

/**
 * @brief Put media
 *
 * This RESTful API connect to the data endpoint which is retrieved from the "/getDataEndpoint", then start a streaming
 * without breaking the HTTP connection. The PUT MEDIA handle is passed in the parameter. User application can use this
 * handle to update MKV headers and data frames.
 *
 * @param[in] pServPara The parameter for KVS service
 * @param[in] pPutMediaPara The parameter for put media
 * @param[out] puHttpStatusCode The HTTP status code
 * @param[out] pPutMediaHandle The pointer of PUT MEDIA handle on success, or NULL if fail.
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaStart(KvsServiceParameter_t *pServPara, KvsPutMediaParameter_t *pPutMediaPara, unsigned int* puHttpStatusCode, PutMediaHandle *pPutMediaHandle);

/**
 * @brief Update MKV header and frame data by using PUT MEDIA handle
 *
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 * @param[in] pMkvHeader The MKV header
 * @param[in] uMkvHeaderLen The length of MKV header
 * @param[in] pData The data frame, or NULL if it's not available
 * @param[in] uDataLen The length of the data frame
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaUpdate(PutMediaHandle xPutMediaHandle, uint8_t *pMkvHeader, size_t uMkvHeaderLen, uint8_t *pData, size_t uDataLen);

/**
 * @brief Update raw data by using PUT MEDIA handle
 *
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 * @param[in] pBuf The MKV header
 * @param[in] uLen The length of MKV header
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaUpdateRaw(PutMediaHandle xPutMediaHandle, uint8_t *pBuf, size_t uLen);

/**
 * @brief Do PUT MEDIA regular work
 * 
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaDoWork(PutMediaHandle xPutMediaHandle);

/**
 * @brief Terminate the handle of PUT MEDIA
 *
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 */
void Kvs_putMediaFinish(PutMediaHandle xPutMediaHandle);

/**
 * @brief Update the value of receive timeout.
 *
 * Receive timeout has been set in service parameters and is applied during connection setup. It can be altered during streaming.
 *
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 * @param[in] uRecvTimeoutMs Receiving timeout in milliseconds
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaUpdateRecvTimeout(PutMediaHandle xPutMediaHandle, unsigned int uRecvTimeoutMs);

/**
* @brief Update the value of send timeout.
*
* Send timeout has been set in service parameters and is applied during connection setup. It can be altered during streaming.
*
* @param[in] xPutMediaHandle The handle of PUT MEDIA
* @param[in] uSendTimeoutMs Receiving timeout in milliseconds
* @return 0 on success, non-zero value otherwise
*/
int Kvs_putMediaUpdateSendTimeout(PutMediaHandle xPutMediaHandle, unsigned int uSendTimeoutMs);

/**
 * @brief Non-blocking read a fragment ACK if any.
 *
 * When Kvs_putMediaDoWork() is called, it will check if any incoming fragment ACKs, and clear fragment ACKs that buffered in the previous Kvs_putMediaDoWork() call.
 * Use Kvs_putMediaReadFragmentAck() to read one fragment ACK and the return value would be 0. If there is no fragment ACK available, then the return value would be non-zero value.
 *
 * @param[in] xPutMediaHandle The handle of PUT MEDIA
 * @param[out] peAckEventType Pointer to the fragment ACK event type
 * @param[out] puFragmentTimecode Pointer to the fragment timecode
 * @param[out] puErrorId Pointer to the error ID
 * @return 0 on success, non-zero value otherwise
 */
int Kvs_putMediaReadFragmentAck(PutMediaHandle xPutMediaHandle, ePutMediaFragmentAckEventType *peAckEventType, uint64_t *puFragmentTimecode, unsigned int *puErrorId);

#endif /* KVS_REST_API_H */
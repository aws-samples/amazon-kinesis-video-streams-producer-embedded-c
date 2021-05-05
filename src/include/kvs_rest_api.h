/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef _KVS_REST_API_H_
#define _KVS_REST_API_H_

#include <stdint.h>

/* It's callback function of PUT MEDIA operation. As device connected to the KVS endpoint of URI "/putMedia" and started
 * streaming, it calls this callback function repeatedly to get media data. */
typedef int (*PutMediaWantMoreDateCallbackFunc)(uint8_t **, uint32_t *, uint8_t *);

typedef struct
{
    char * pAccessKey;  // It's AWS access key if not using IoT certification.
    char * pSecretKey;  // It's secret of AWS access key if not using IoT certification.
    char * pToken;      // Set to NULL if not using IoT certification.

    char * pRegion;     // The desired region of KVS service
    char * pService;    // KVS service name
    char * pHost;       // Endpoint of the RESTful api
    char * pUserAgent;  // HTTP agent name
} KvsServiceParameter_t;

/**
 * @brief Create a Stream
 *
 * @param[in] pServiceParameter AWS Service related parameters
 * @param[in] pDeviceName Parameter of "/createStream".
 * @param[in] pStreamName Parameter of "/createStream"
 * @param[in] pMediaType Parameter of "/createStream"
 * @param[in] xDataRetentionInHours Parameter of "/createStream"
 *
 * @return KVS error code
 */
int Kvs_createStream( KvsServiceParameter_t * pServiceParameter,
                      char * pDeviceName,
                      char * pStreamName,
                      char * pMediaType,
                      uint32_t xDataRetentionInHours );

/**
 * @brief Describe a Stream
 *
 * Get the description of a stream to make sure the stream is available. It returns error if the stream does not exist
 * and therefore needs creation before using it.
 *
 * @param[in] pServiceParameter AWS Service related parameters
 * @param[in] pStreamName Parameter of "/describeStream"
 *
 * @return KVS error code
 */
int Kvs_describeStream( KvsServiceParameter_t * pServiceParameter,
                        char * pStreamName);

/**
 * @brief Get data endpoint
 *
 * Get the data endpoint and store it in the pEndPointBuffer.
 *
 * @param[in] pServiceParameter AWS Service related parameters
 * @param[in] pStreamName Parameter of "/getDataEndpoint"
 * @param[out] pEndPointBuffer The buffer for storing the endpoint address
 * @param[in] uEndPointBufferLen The buffer length
 *
 * @return KVS error code
 */
int Kvs_getDataEndpoint( KvsServiceParameter_t * pServiceParameter,
                         char * pStreamName,
                         char * pEndPointBuffer,
                         uint32_t uEndPointBufferLen );

/**
 * @brief Put media
 *
 * This RESTful API connect to the data endpoint which is retrieved from the "/getDataEndpoint", then start a streaming
 * without breaking the HTTP connection. For uplink, it calls the wantMoreCb callback repeatedly for media data and
 * sends it out. For downlink, it repeatedly check if there is any coming data then parse it.
 *
 * @param[in] pServiceParameter AWS Service related parameters
 * @param[in] pStreamName Parameter of "/putMedia".
 * @param[in] wantMoreCb Callback function when it wants more data to transmit
 * @param[in] pCustomData A pointer of custom data which is passed as an parameter of wantMoreCb
 *
 * @return KVS error code
 */
int Kvs_putMedia( KvsServiceParameter_t * pServiceParameter,
                  char * pStreamName,
                  PutMediaWantMoreDateCallbackFunc wantMoreCb,
                  void * pCustomData );

#endif // #ifndef _KVS_REST_API_H_
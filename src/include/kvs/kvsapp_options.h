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

#ifndef KVSAPP_OPTIONS_H
#define KVSAPP_OPTIONS_H

typedef enum KvsApp_streamPolicy
{
    STREAM_POLICY_NONE = 0,
    STREAM_POLICY_RING_BUFFER,
    STREAM_POLICY_MAX
} KvsApp_streamPolicy_t;

static const char * const OPTION_AWS_ACCESS_KEY_ID = "Aws_accessKeyId";
static const char * const OPTION_AWS_SECRET_ACCESS_KEY = "Aws_secretAccessKey";
static const char * const OPTION_AWS_SESSION_TOKEN = "Aws_sessionToken";

static const char * const OPTION_IOT_CREDENTIAL_HOST = "Iot_credentialHost";
static const char * const OPTION_IOT_ROLE_ALIAS = "Iot_roleAlias";
static const char * const OPTION_IOT_THING_NAME = "Iot_thingName";
static const char * const OPTION_IOT_X509_ROOTCA = "Iot_x509RootCa";
static const char * const OPTION_IOT_X509_CERT = "Iot_x509Certificate";
static const char * const OPTION_IOT_X509_KEY = "Iot_x509PrivateKey";

static const char * const OPTION_KVS_DATA_RETENTION_IN_HOURS = "Kvs_dataRetentionInHours";
static const char * const OPTION_KVS_VIDEO_TRACK_INFO = "Kvs_videoTrackInfo";
static const char * const OPTION_KVS_AUDIO_TRACK_INFO = "Kvs_audioTrackInfo";

static const char * const OPTION_STREAM_POLICY = "Stream_policy";
static const char * const OPTION_STREAM_POLICY_RING_BUFFER_MEM_LIMIT = "Stream_RbMemlimit";

static const char * const OPTION_NETIO_CONNECTION_TIMEOUT = "NetIo_connTimeout";
static const char * const OPTION_NETIO_STREAMING_RECV_TIMEOUT = "NetIo_recvTimeout";
static const char * const OPTION_NETIO_STREAMING_SEND_TIMEOUT = "NetIo_sendTimeout";

#endif

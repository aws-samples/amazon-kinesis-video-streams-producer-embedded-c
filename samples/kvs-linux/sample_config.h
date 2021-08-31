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

#ifndef SAMPLE_CONFIG_H
#define SAMPLE_CONFIG_H

#include "kvs/mkv_generator.h"

/* KVS general configuration */
#define AWS_ACCESS_KEY                  "xxxxxxxxxxxxxxxxxxxx"
#define AWS_SECRET_KEY                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

/* KVS stream configuration */
#define KVS_STREAM_NAME                 "kvs_example_camera_stream"
#define AWS_KVS_REGION                  "us-east-1"
#define AWS_KVS_SERVICE                 "kinesisvideo"
#define AWS_KVS_HOST                    AWS_KVS_SERVICE "." AWS_KVS_REGION ".amazonaws.com"

/* KVS optional configuration */
#define ENABLE_AUDIO_TRACK              0
#define ENABLE_IOT_CREDENTIAL           1
#define DEBUG_STORE_MEDIA_TO_FILE       0

/* Video configuration */
#define H264_FILE_FORMAT                "/path/to/samples/h264SampleFrames/frame-%03d.h264"
#define H264_FILE_IDX_BEGIN             1
#define H264_FILE_IDX_END               403

#define VIDEO_TRACK_NAME                "kvs video track"
#define VIDEO_FPS                       25

/* Audio configuration */
#if ENABLE_AUDIO_TRACK
#define AAC_FILE_FORMAT                 "/path/to/samples/aacSampleFrames/sample-%03d.aac"
#define AAC_FILE_IDX_BEGIN              1
#define AAC_FILE_IDX_END                582

#define AUDIO_TRACK_NAME                "kvs audio track"
#define AUDIO_FPS                       50
#define AUDIO_MPEG_OBJECT_TYPE          AAC_LC
#define AUDIO_FREQUENCY                 48000
#define AUDIO_CHANNEL_NUMBER            2
#endif /* ENABLE_AUDIO_TRACK */

/* IoT credential configuration */
#if ENABLE_IOT_CREDENTIAL
#define CREDENTIALS_HOST                "xxxxxxxxxxxxxx.credentials.iot.us-east-1.amazonaws.com"
#define ROLE_ALIAS                      "KvsCameraIoTRoleAlias"
#define THING_NAME                      KVS_STREAM_NAME

#define ROOT_CA \
"-----BEGIN CERTIFICATE-----\n" \
"......\n" \
"-----END CERTIFICATE-----\n"

#define CERTIFICATE \
"-----BEGIN CERTIFICATE-----\n" \
"......\n" \
"-----END CERTIFICATE-----\n"

#define PRIVATE_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"......\n" \
"-----END RSA PRIVATE KEY-----\n"
#endif

/* Debug configuration */
#if DEBUG_STORE_MEDIA_TO_FILE
#define MEDIA_FILENAME                  "temp.mkv"
#endif /* DEBUG_STORE_MEDIA_TO_FILE */

#ifdef KVS_USE_POOL_ALLOCATOR
#define POOL_ALLOCATOR_SIZE             (2 * 1024 * 1024 + 512 * 1024)
#endif

#endif /* SAMPLE_CONFIG_H */
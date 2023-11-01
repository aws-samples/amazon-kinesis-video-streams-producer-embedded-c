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
#define ENABLE_IOT_CREDENTIAL           0
#define ENABLE_RING_BUFFER_MEM_LIMIT    1
#define DEBUG_STORE_MEDIA_TO_FILE       0

/* Video configuration */
#define H264_FILE_FORMAT                "/sdcard/h264_annexb/frame-%03d.h264"
#define H264_FILE_IDX_BEGIN             1
#define H264_FILE_IDX_END               240

#define VIDEO_TRACK_NAME                "kvs video track"
#define VIDEO_FPS                       30

/* Audio configuration */
#if ENABLE_AUDIO_TRACK
#define USE_AUDIO_AAC_SAMPLE            1   /* Set to 1 to use AAC as audio track */
#define USE_AUDIO_G711_SAMPLE           0   /* Set to 1 to use G711 as audio track */

#if (USE_AUDIO_AAC_SAMPLE == 0) && (USE_AUDIO_G711_SAMPLE == 0)
#error "Please select audio source"
#endif

#if USE_AUDIO_AAC_SAMPLE
#define AAC_FILE_FORMAT                 "/sdcard/aac_raw/audio-%03d.aac"
#define AAC_FILE_IDX_BEGIN              1
#define AAC_FILE_IDX_END                325

#define AUDIO_TRACK_NAME                "kvs audio track"
#define AUDIO_FPS                       48
#define AUDIO_MPEG_OBJECT_TYPE          MPEG4_AAC_LC
#define AUDIO_FREQUENCY                 48000
#define AUDIO_CHANNEL_NUMBER            2
#endif /* USE_AUDIO_AAC_SAMPLE */

#if USE_AUDIO_G711_SAMPLE
#define G711_FILE_FORMAT                "/sdcard/g711_alaw/audio-%03d.alaw"
#define G711_FILE_IDX_BEGIN             1
#define G711_FILE_IDX_END               325

#define AUDIO_TRACK_NAME                "kvs audio track"
#define AUDIO_FPS                       25
#define AUDIO_PCM_OBJECT_TYPE           PCM_FORMAT_CODE_ALAW
#define AUDIO_FREQUENCY                 8000
#define AUDIO_CHANNEL_NUMBER            1
#endif /* USE_AUDIO_G711_SAMPLE */

#endif /* ENABLE_AUDIO_TRACK */

/* IoT credential configuration */
#if ENABLE_IOT_CREDENTIAL
#define AWS_IOT_CREDENTIALS_HOST                "xxxxxxxxxxxxxx.credentials.iot.us-east-1.amazonaws.com"
#define AWS_IOT_ROLE_ALIAS                      "KvsCameraIoTRoleAlias"
#define AWS_IOT_THING_NAME                      KVS_STREAM_NAME

#define AWS_IOT_ROOT_CA \
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

#if ENABLE_RING_BUFFER_MEM_LIMIT
/* Buffering options */
#define RING_BUFFER_MEM_LIMIT           (2 * 1024 * 1024)
#endif /* ENABLE_RING_BUFFER_MEM_LIMIT */

/* Debug configuration */
#if DEBUG_STORE_MEDIA_TO_FILE
/* Store MKV content in a file with timestamp in its filename. */
#define MEDIA_FILENAME_FORMAT                  "/sdcard/video_%" PRIu64 ".mkv"
#endif /* DEBUG_STORE_MEDIA_TO_FILE */

#endif /* SAMPLE_CONFIG_H */
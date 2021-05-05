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

#ifndef _EXAMPLE_CONFIG_H
#define _EXAMPLE_CONFIG_H

#define EXAMPLE_WIFI_SSID "your_ssid"
#define EXAMPLE_WIFI_PASS "your_pw"

#define AWS_ACCESS_KEY "xxxxxxxxxxxxxxxxxxxx"
#define AWS_SECRET_KEY "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#define DEVICE_NAME             "DEV_12345678"

#define KVS_DATA_ENDPOINT_MAX_SIZE  ( 128 )

#define AWS_KVS_REGION      "us-east-1"
#define AWS_KVS_SERVICE     "kinesisvideo"
#define AWS_KVS_HOST        AWS_KVS_SERVICE "." AWS_KVS_REGION ".amazonaws.com"
#define HTTP_AGENT          "myagent"
#define KVS_STREAM_NAME     "my-kvs-stream"
#define DATA_RETENTION_IN_HOURS ( 2 )

#define VIDEO_CODEC_NAME                    "V_MPEG4/ISO/AVC"
#define MEDIA_TYPE                          "video/h264"
#define VIDEO_FPS                           25
#define H264_SAMPLE_FRAMES_SIZE             403
#define DEFAULT_KEY_FRAME_INTERVAL          45
#define MAX_VIDEO_FRAME_SIZE                (120 * 1024)

#define H264_FORMAT_ANNEX_B          (1)
#define H264_FORMAT_AVCC             (2)

#define H264_SOURCE_FORMAT                  H264_FORMAT_ANNEX_B

#define EXAMPLE_SDCARD_FILE_COUNT           448
#define EXAMPLE_FILENAME_MAX_LEN            128

#if H264_SOURCE_FORMAT == H264_FORMAT_ANNEX_B
    #define SAMPLE_VIDEO_FILENAME_FORMAT        "/sdcard/h264SampleFrames/frame-%03d.h264"
    #define H264_CONVERT_ANNEX_B_TO_AVCC
#elif H264_SOURCE_FORMAT == H264_FORMAT_AVCC
    #define SAMPLE_VIDEO_FILENAME_FORMAT        "/sdcard/h264AvccSampleFrames/frame-%03d.h264"
#else
    #error "Invalid H264 source format"
#endif

#define VIDEO_NAME      "my video"
#define VIDEO_WIDTH     640
#define VIDEO_HEIGHT    480

#endif
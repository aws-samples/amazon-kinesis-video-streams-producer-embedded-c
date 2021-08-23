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

#define STREAM_MAX_BUFFERING_SIZE       (1 * 1024 * 1024)

/* KVS optional configuration */
#define ENABLE_AUDIO_TRACK              1
#define ENABLE_IOT_CREDENTIAL           0

/* Video configuration */
/* https://www.matroska.org/technical/codec_specs.html */
#define VIDEO_CODEC_NAME                    "V_MPEG4/ISO/AVC"
#define VIDEO_FPS                           30
#include "sensor.h"
#if SENSOR_USE == SENSOR_PS5270
    #define VIDEO_HEIGHT    VIDEO_1440SQR_HEIGHT
    #define VIDEO_WIDTH     VIDEO_1440SQR_WIDTH
#else
    #define VIDEO_HEIGHT    VIDEO_1080P_HEIGHT   //VIDEO_720P_HEIGHT
    #define VIDEO_WIDTH     VIDEO_1080P_WIDTH    //VIDEO_720P_WIDTH
#endif
#define VIDEO_NAME                          "my video"

/* Audio configuration */
#if ENABLE_AUDIO_TRACK
#define AUDIO_CODEC_NAME                    "A_AAC"
#define AUDIO_FPS                           5
#define AUDIO_SAMPLING_RATE                 8000
#define AUDIO_CHANNEL_NUMBER                1
#define AUDIO_NAME                          "my audio"
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

#endif /* SAMPLE_CONFIG_H */
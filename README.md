<h1 style = "text-align: center;">
  Amazon Kinesis Video Streams Producer Embedded C SDK
  <br>
</h1>

<h4 style = "text-align: center;">
  Light-wight Amazon Kinesis Video Streams Producer SDK For FreeRTOS/Embedded Linux
</h4>

<p style = "text-align: center;">
  <a href="https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c/actions/workflows/cmake.yml"> <img src="https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c/actions/workflows/cmake.yml/badge.svg?branch=main" alt="Build Status"> </a>
</p>

This project demonstrates how to port Amazon Kinesis Video Streams Producer to Embedded devices(FreeRTOS/Embedded Linux). In case of using the following platforms the reference examples can securely stream video to Kinesis Video Streams.

- [KVS producer on Linux](samples/kvsapp) - To evaluate KVS Producer without using MCU-based hardware, you can use the Linux machine. Please refer to the following section to setup the example.
- [KVS producer with WebRTC on Linux](samples/kvs-with-webrtc)
- [Ingenic T31](samples/kvsapp-ingenic-t31)
- [Espressif ESP-WROVER-KIT](samples/kvs-esp32/)
- [Realtek AmebaPro](samples/kvs-amebapro)
- [Raspberry Pi/V4L2](samples/kvsapp/RPi.md)

# Build and Run Examples

## Download

To download, run the following command:

```
git clone --recursive https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c.git
```

If you miss running `git clone` command with `--recursive` , run `git submodule update --init --recursive` within the repository.

## Configure IoT Core Certificate

Camera devices can use X.509 certificates to connect to AWS IoT and retrieve credentials from the AWS IoT credentials provider to authenticate with the Kinesis Video Streams service. Follow the instructions in this document "[how-iot](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html)" to setup the X.509 certificate.

## Prepare H264 sample frames

In the examples, we upload video from H264 frames which are stored as files. These files are put under "samples/h264SampleFrames/" with filename "frame-[index].h264".  For example, "frame-001.h264", "frame-002.h264", etc.

You can also use [sample frames of amazon-kinesis-video-streams-producer-c](https://github.com/awslabs/amazon-kinesis-video-streams-producer-c/tree/master/samples/h264SampleFrames).

## Linux Example

This section describes how to run the example on Linux. The reference OS is Ubuntu 18.04LTS.

### Configure Example Setting

Before running the example, you need to edit the file "*samples/kvsapp/sample_config.h*" and replace these settings:

```
#define KVS_STREAM_NAME                 "kvs_example_camera_stream"
#define AWS_KVS_REGION                  "us-east-1"
#define AWS_KVS_SERVICE                 "kinesisvideo"
#define AWS_KVS_HOST                    AWS_KVS_SERVICE "." AWS_KVS_REGION ".amazonaws.com"

#define H264_FILE_FORMAT                "/path/to/samples/h264SampleFrames/frame-%03d.h264"

#define CREDENTIALS_HOST                "xxxxxxxxxxxxxx.credentials.iot.us-east-1.amazonaws.com"
#define ROLE_ALIAS                      "KvsCameraIoTRoleAlias"
#define THING_NAME                      KVS_STREAM_NAME

#define ROOT_CA \
"-----BEGIN CERTIFICATE-----\n" \
"......" \
"-----END CERTIFICATE-----\n"

#define CERTIFICATE \
"-----BEGIN CERTIFICATE-----\n" \
"......" \
"-----END CERTIFICATE-----\n"

#define PRIVATE_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"......" \
"-----END RSA PRIVATE KEY-----\n"
#endif
```

The values of these settings come from the procedure of setting up the credentials provider by following instructions in this document "[how-iot](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html)".

*   KVS_STREAM_NAME: It's the stream name.
*   CREDENTIALS_HOST: It's the IoT credentials host we setup earier.
*   ROLE_ALIAS: It's the role alias we setup earier.
*   ROOT_CA, CERTIFICATE, PRIVATE_KEY: These are X509 certificates.  Please filled in your X509 certificates.
*   H264_FILE_FORMAT: It's H264 file location.

### Build and Run Example

Run the following command to configure the CMake project:

```
mkdir build
cd build
cmake ..
```

Run the following command to build the project.

```
cmake --build .
```

To run the example, run the following command.

```
./bin/kvsappcli
```

If everything works fine, you should see the following logs.

```
PUT MEDIA endpoint: s-xxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
Try to put media
Info: 100-continue
Info: Fragment buffering, timecode:1620367399995
Info: Fragment received, timecode:1620367399995
Info: Fragment buffering, timecode:1620367401795
Info: Fragment persisted, timecode:1620367399995
Info: Fragment received, timecode:1620367401795
Info: Fragment buffering, timecode:1620367403595
.....
```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo). 

# Porting Guide

Please refer to [port](src/port/) for how to port solution to your destination platform.  There are already platform Linux and ESP32 for reference.

In the KVS application, we use these platform-dependent components:

*   time: We need to get the current time for RESTful API and add timestamps to data frames.
*   random number: It's used to generate the UUID of the MKV segment header.  You could ignore it if you didn't use the MKV part of this library.
*   sleep: It's for avoiding race conditions when making RESTful requests.


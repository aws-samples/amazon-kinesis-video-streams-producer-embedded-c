# Amazon Kinesis Video Streams Producer SDK for FreeRTOS

This project demonstrates how to port Amazon Kinesis Video Streams Producer to FreeRTOS.  There are 3 examples.  One uses ESP-Wrover-kit as a reference platform.  The others uses Linux as a reference platform.

# Build and Run Examples

## Download

To download run the following command:

```
git clone --recursive https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c.git
```

If you miss running `git clone` command with `--recursive` , run `git submodule update --init --recursive` within repository.

## Apply patch

There is a patch needs to be applied to library **coreHTTP**:

```
cd libraries/aws/coreHTTP
git apply ../../../patch/00001-coreHTTP.patch
cd -
```

## Configure IoT Core Certificate

Camera devices can use X.509 certificates to connect to AWS IoT, then retrieve credentials from the AWS IoT credentials provider for authenticating with the Kinesis Video Streams service. Follow the instructions in this document "[how-iot](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html)" to setup the X.509 certificate.

## Prepare H264 sample frames

In the examples, we upload video from H264 frames which are stored as files. These files are put under "samples/h264SampleFrames/" with filename "frame-[index].h264".  For example, "frame-001.h264", "frame-002.h264", etc.

You can also use [sample frames of amazon-kinesis-video-streams-producer-c](https://github.com/awslabs/amazon-kinesis-video-streams-producer-c/tree/master/samples/h264SampleFrames).

## Linux Example

This section describe how to run example on Linux. The reference OS is Ubuntu 18.04LTS.

### Configure Example Setting

You need to edit file "*examples/kvs_video_only_linux/example_config.h*", and replace these settings:

```
#define KVS_STREAM_NAME                 "my-kvs-stream"
#define CREDENTIALS_HOST    "xxxxxxxxxxxxxx.credentials.iot.us-east-1.amazonaws.com"
#define ROLE_ALIAS          "KvsCameraIoTRoleAlias"

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

### Build and Run Example

Run the following command to configure cmake project:

```
mkdir build
cd build
cmake ..
```

Run the following command to build project.

```
cmake --build .
```

To run example run the following command.

```
./examples/kvs_video_only_linux/kvs_video_only_linux
```

If everything works fine, you should see the following logs.

```
try to describe stream
try to get data endpoint
Data endpoint: xxxxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
try to put media
......
Fragment buffering, timecode:1618214685798
......
Fragment received, timecode:1618214685798
Fragment buffering, timecode:1618214687659
......
Fragment persisted, timecode:1618214685798

```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo). 

## ESP-Wrover-Kit example

This section describe how to run example on [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview). Before run this example, these hardware are required.

- An evaluation board of [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview).
- A micro SD card that has been formatted for FAT file system.

### Prepare Sample Frames on Micro SD Card

Please copy sample frames to micro SD card.  These sample frames are being loaded while running examples.

```
cp -rf sampleFrames/h264AvccSampleFrames/ <sd_card_root>
```

Then put this micro SD card into ESP-Wrover-Kit.

### Prepare ESP IDF environment

If you don't have a ESP IDF environment, please follow this document to setup ESP IDF environment:

[https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

### Configure Example Setting

Before run the example, you need to edit file "*examples/kvs_video_only_esp32/main/example_config.h*", and replace these settings. Please note that ESP-Wrover-kit only support 2.4G WiFi network.  So you should provide ssid and password of 2.4G access point.

```
#define EXAMPLE_WIFI_SSID "your_ssid"
#define EXAMPLE_WIFI_PASS "your_pw"
```

You also need to setup these settings. Please refer to linux example for more information.

```
#define KVS_STREAM_NAME                 "my-kvs-stream"
#define CREDENTIALS_HOST    "xxxxxxxxxxxxxx.credentials.iot.us-east-1.amazonaws.com"
#define ROLE_ALIAS          "KvsCameraIoTRoleAlias"

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

### Build and Run Example

To build the example run the following command:

```
cd examples/kvs_video_only_esp32/
idf.py build
```

Make sure your ESP-Wrover-Kit is connected and powered on. Run following command to flash image and check the logs.

```
idf.py flash monitor
```

If everything works fine, you should see the following logs.

```
I (3776) wifi:connected with xxxxxxxx, aid = 3, channel 8, BW20, bssid = xx:xx:xx:xx:xx:xx
...
I (4676) aws: Connected to AP success!

I (8296) kvs: try to describe stream
I (11226) kvs: try to get data endpoint
I (13986) kvs: Data endpoint: xxxxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
I (13986) kvs: try to put media
I (17186) kvs: Fragment buffering, timecode:1618214685798
I (18676) kvs: Fragment received, timecode:1618214685798
...
I (20646) kvs: Fragment persisted, timecode:1618214685798
```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo).
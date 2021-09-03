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

- [Espressif ESP-WROVER-KIT](https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c#esp-wrover-kit-example)
- [Realtek AmebaPro](https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c#realtek-amebapro-example)
- [Raspberry Pi/V4L2](https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c#raspberry-pi-v4l2-example)
- [Linux](https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c#linux-example) - To evaluate KVS Producer without using MCU-based hardware, you can use the Linux machine.



# Build and Run Examples

## Download

To download run the following command:

```
git clone --recursive https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c.git
```

If you miss running `git clone` command with `--recursive` , run `git submodule update --init --recursive` within repository.

## Configure IoT Core Certificate

Camera devices can use X.509 certificates to connect to AWS IoT, then retrieve credentials from the AWS IoT credentials provider for authenticating with the Kinesis Video Streams service. Follow the instructions in this document "[how-iot](https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/how-iot.html)" to setup the X.509 certificate.

## Prepare H264 sample frames

In the examples, we upload video from H264 frames which are stored as files. These files are put under "samples/h264SampleFrames/" with filename "frame-[index].h264".  For example, "frame-001.h264", "frame-002.h264", etc.

You can also use [sample frames of amazon-kinesis-video-streams-producer-c](https://github.com/awslabs/amazon-kinesis-video-streams-producer-c/tree/master/samples/h264SampleFrames).

## Linux Example

This section describe how to run example on Linux. The reference OS is Ubuntu 18.04LTS.

### Configure Example Setting

Before run the example, you need to edit file "*samples/kvs-linux/sample_config.h*", and replace these settings:

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
./bin/kvs_linux
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

Before run the example, you need configure WiFi configuration by using this command.

```
cd samples/kvs-esp32/
idf.py menuconfig
```

Configure WiFi SSID and pasword in the menu "Example Configuration".  Please note that ESP-Wrover-kit only support 2.4G WiFi network.  So you should provide ssid and password of 2.4G access point.

You also need to setup these settings. Please refer to linux example for more information.

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
I (4676) KVS: Connected to ap SSID:xxxxxxxx password:xxxxxxxx

PUT MEDIA endpoint: s-xxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
Try to put media
Info: 100-continue
Info: Fragment buffering, timecode:1620367399995
Info: Fragment received, timecode:1620367399995
Info: Fragment buffering, timecode:1620367401795
Info: Fragment persisted, timecode:1620367399995
Info: Fragment received, timecode:1620367401795
Info: Fragment buffering, timecode:1620367403595
```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo).

## Realtek AmebaPro example

This section describe how to run example on [AmebaPro](https://www.amebaiot.com/en/amebapro). Before run this example, these hardware are required.  

- An evaluation board of [AmebaPro](https://www.amebaiot.com/en/amebapro/).
- A camera sensor for AmebaPro.

### Prepare AmebaPro SDK environment

If you don't have a AmebaPro SDK environment, please run the following command to download AmebaPro SDK:

```
git clone --recurse-submodules https://github.com/ambiot/ambpro1_sdk.git
```

the command will also download kvs producer repository simultaneously.

### Check the model of camera sensor 

Please check your camera sensor model, and define it in <AmebaPro_SDK>/project/realtek_amebapro_v0_example/inc/sensor.h.

```
#define SENSOR_USE      	SENSOR_XXXX
```

### Configure Example Setting

Before run the example, you need to check the KVS example is enabled in <AmebaPro_SDK>/project/realtek_amebapro_v0_example/inc/platform_opts.h.

```
/* For KVS Producer example*/
#define CONFIG_EXAMPLE_KVS_PRODUCER             1
```

You also need to setup these settings. Please refer to linux example for more information.

```
#define KVS_STREAM_NAME                 "kvs_example_camera_stream"
#define AWS_KVS_REGION                  "us-east-1"
#define AWS_KVS_SERVICE                 "kinesisvideo"
#define AWS_KVS_HOST                    AWS_KVS_SERVICE "." AWS_KVS_REGION ".amazonaws.com"
...
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

### Build and Run Example

To build the example run the following command:

```
cd <AmebaPro_SDK>/project/realtek_amebapro_v0_example/GCC-RELEASE
make all
```

The image is located in <AmebaPro_SDK>/project/realtek_amebapro_v0_example/GCC-RELEASE/application_is/flash_is.bin

Make sure your AmebaPro is connected and powered on. Use the Realtek image tool to flash image and check the logs.

[How to use Realtek image tool? See section 1.3 in AmebaPro's application note](https://github.com/ambiot/ambpro1_sdk/blob/main/doc/AN0300%20Realtek%20AmebaPro%20application%20note.en.pdf)

### Configure WiFi Connection

While runnung the example, you may need to configure WiFi connection by using these commands in uart terminal.

```
ATW0=<WiFi_SSID> : Set the WiFi AP to be connected
ATW1=<WiFi_Password> : Set the WiFi AP password
ATWC : Initiate the connection
```

If everything works fine, you should see the following logs.

```
Interface 0 IP address : xxx.xxx.xxx.xxx
WIFI initialized
...
[H264] init encoder
[ISP] init ISP
...
PUT MEDIA endpoint: s-xxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
Try to put media
Info: 100-continue
Info: Fragment buffering, timecode:1620367399995
Info: Fragment received, timecode:1620367399995
Info: Fragment buffering, timecode:1620367401795
Info: Fragment persisted, timecode:1620367399995
Info: Fragment received, timecode:1620367401795
Info: Fragment buffering, timecode:1620367403595
```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo).

## Raspberry Pi V4L2 Example

[Raspberry Pi V4L2 Example](https://github.com/awslabs/amazon-kinesis-video-streams-producer-c/tree/main/samples/kvsapp-raspberry-pi) provides an out-of-box sample that allows user to capture frames from v4l2 camera(i.e. Pi Camera) to KVS. It can be used on Raspberry Pi and other Linux based machine that with v4l2 camera supports.

### Check Camera Capabilities

Use `v4l2-ctl --list-formats` to check if camera can be used with this sample. This sample need camera support `H264`. Take Pi camera as example:

```bash
v4l2-ctl --list-formats
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'YU12' (Planar YUV 4:2:0)
	[1]: 'YUYV' (YUYV 4:2:2)
	[2]: 'RGB3' (24-bit RGB 8-8-8)
	[3]: 'JPEG' (JFIF JPEG, compressed)
	[4]: 'H264' (H.264, compressed)
	[5]: 'MJPG' (Motion-JPEG, compressed)
	[6]: 'YVYU' (YVYU 4:2:2)
	[7]: 'VYUY' (VYUY 4:2:2)
	[8]: 'UYVY' (UYVY 4:2:2)
	[9]: 'NV12' (Y/CbCr 4:2:0)
	[10]: 'BGR3' (24-bit BGR 8-8-8)
	[11]: 'YV12' (Planar YVU 4:2:0)
	[12]: 'NV21' (Y/CrCb 4:2:0)
	[13]: 'RX24' (32-bit XBGR 8-8-8-8)
```

### Build Raspberry Pi V4L2 Example

```bash
cd amazon-kinesis-video-streams-producer-embedded-c
mkdir build; cd build
cmake .. -DBOARD_RPI=ON
make -j16
```

### Run Raspberry Pi V4L2 Example

```bash
./bin/kvsappcli-raspberry-pi-static
```

# Porting Guide

Please refer to [port](src/port/) for how to port solution to your destinate platform.  There are already platform Linux and ESP32 for refrence.

In KVS application we use these platform dependent components:

*   time: We need to get current time for RESTful API, and add timestamp of data frames.
*   random number: It's used to generate the UUID of MKV segment header.  You can ignore it if you didn't use the MKV part of this library.


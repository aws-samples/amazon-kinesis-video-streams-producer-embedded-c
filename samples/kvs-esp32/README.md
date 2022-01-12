# ESP-Wrover-Kit example

This section describe how to run example on [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview). Before run this example, these hardware are required.

- An evaluation board of [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview).
- A micro SD card that has been formatted for FAT file system.

## Prepare Sample Frames on Micro SD Card

Please copy sample frames to micro SD card.  These sample frames are being loaded while running examples.

```
cp -rf sampleFrames/h264AvccSampleFrames/ <sd_card_root>
```

Then put this micro SD card into ESP-Wrover-Kit.

## Prepare ESP IDF environment

If you don't have a ESP IDF environment, please follow this document to setup ESP IDF environment:

[https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Configure Example Setting

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

## Build and Run Example

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

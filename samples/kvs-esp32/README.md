# ESP-Wrover-Kit example

This section describes how to run the sample on [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview). Before running this example, this hardware are required.

- An evaluation board of [ESP-Wrover-kit](https://www.espressif.com/en/products/hardware/esp-wrover-kit/overview).
- A micro SD card that has been formatted for the FAT file system.

## Prepare Sample Frames on Micro SD Card

Please copy sample frames to the micro SD card. These sample frames are being loaded while running examples.

```
cp -rf res/media/h264_annexb <sd_card_root>
```

Then put this micro SD card into ESP-Wrover-Kit.

## Prepare ESP IDF environment

If you don't have an ESP IDF environment, please follow this document to setup ESP IDF environment:

[https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Configure Example Setting

Before running the example, you need to configure the WiFi configuration by using this command.

```
cd samples/kvs-esp32/
idf.py menuconfig
```

Configure WiFi SSID and password in the menu "Example Configuration." Please note that ESP-Wrover-kit only supports a 2.4G WiFi network. So you should provide the SSID and password of the 2.4G access point.

You also need to setup these settings. Please refer to the Linux example for more information.

```
/* KVS general configuration */
#define AWS_ACCESS_KEY                  "xxxxxxxxxxxxxxxxxxxx"
#define AWS_SECRET_KEY                  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

/* KVS stream configuration */
#define KVS_STREAM_NAME                 "kvs_example_camera_stream"
#define AWS_KVS_REGION                  "us-east-1"
#define AWS_KVS_SERVICE                 "kinesisvideo"
#define AWS_KVS_HOST                    AWS_KVS_SERVICE "." AWS_KVS_REGION ".amazonaws.com"

#define H264_FILE_FORMAT                "/path/to/samples/h264SampleFrames/frame-%03d.h264"
```

## Build and Run Example

To build the sample, run the following command.

```
cd samples/kvs-esp32/
idf.py build
```

Make sure your ESP-Wrover-Kit is connected and powered on. Run the following command to flash the image and check the logs.

```
idf.py flash monitor
```

If everything works fine, you should be able to see the following logs.

```
I (3776) wifi:connected with xxxxxxxx, aid = 3, channel 8, BW20, bssid = xx:xx:xx:xx:xx:xx
...
I (4676) KVS: Connected to ap SSID:xxxxxxxx password:xxxxxxxx

Info: Try to describe stream
Info: PUT MEDIA endpoint: xxxxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
Info: 100-continue
Info: KVS stream buffer created
Info: Flush to next cluster
Info: No cluster frame is found
Buffer memory used: 2216
Info: Flush to next cluster
Info: No cluster frame is found
Info: Flush to next cluster
Info: No cluster frame is found
Info: Flush to next cluster
Buffer memory used: 23215
Info: Fragment buffering, timecode:1644206436032
Buffer memory used: 31164
Buffer memory used: 58334
Info: Fragment received, timecode:1644206436032
Info: Fragment buffering, timecode:1644206438445
Info: Fragment persisted, timecode:1644206436032
Buffer memory used: 93685
Buffer memory used: 126851
Buffer memory used: 168007
Info: Fragment received, timecode:1644206438445
Info: Fragment buffering, timecode:1644206439780
Info: Fragment persisted, timecode:1644206438445
Buffer memory used: 197205
```

You can also check the streaming video in the console of [Kinesis Video](https://console.aws.amazon.com/kinesisvideo).

# Raspberry Pi V4L2 Example

[Raspberry Pi V4L2 Example](https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c/tree/main/samples/kvsapp-raspberry-pi) provides an out-of-box sample that allows user to capture frames from v4l2 camera(i.e. Pi Camera) to KVS. It can be used on Raspberry Pi and other Linux based machine that with v4l2 camera supports.

## Check Camera Capabilities

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

## Build Raspberry Pi V4L2 Example

```bash
cd amazon-kinesis-video-streams-producer-embedded-c
mkdir build; cd build
cmake .. -DBOARD_RPI=ON
make -j16
```

## Run Raspberry Pi V4L2 Example

```bash
./bin/kvsappcli-raspberry-pi
```
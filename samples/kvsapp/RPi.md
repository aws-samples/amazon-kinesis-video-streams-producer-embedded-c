# Sample for Raspberry Pi

Sample can be configured as reading real frames from V4L2 camera(like Pi camera), then upload to AWS KVS backend.

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

## Build

```bash
cd amazon-kinesis-video-streams-producer-embedded-c
mkdir build
cd build
cmake .. -DBOARD_RPI=ON
cmake --build .
```

## Set AWS credentials

If you didn't configure sample configurations in file "samples/kvsapp/include/sample_config.h", you need to configure the following environment variables.

```bash
export AWS_ACCESS_KEY_ID=xxxxxxxxxxxxxxxxxxxx
export AWS_SECRET_ACCESS_KEY=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export AWS_DEFAULT_REGION=us-east-1
export AWS_KVS_HOST=kinesisvideo.us-east-1.amazonaws.com
```

## Run

```bash
cd amazon-kinesis-video-streams-producer-embedded-c/build/
./bin/kvsappcli
```

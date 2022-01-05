# KVS with WebRTC

## Introduction

This sample shows how KVS producer and WebRTC share video buffer to reduce memory usage.

The video buffer will be duplicated if we run KVS producer and WebRTC separately. The video buffer consumes many memories. If we need to run KVS producer and WebRTC simultaneously, and we can also make them share the same video buffer, then we can save memories.

## Run the sample

### Download

Run the following command to download the code:

```bash
git clone --recursive https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c.git
```

### Configure Sample Setting

Please refer to `sample_config.h` to configure the producer's setting. It's similar to the `sample_config.h` in sample ***kvsapp*** except that this one doesn't have video and audio sources.

To configure WebRTC's setting, please refer to `Samples.h`. It's similar to the configuration in SDK [https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c).

The quickest way is to configure the following environment variables.

```bash
export AWS_ACCESS_KEY_ID=xxxxxxxxxxxxxxxxxxxx
export AWS_SECRET_ACCESS_KEY=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
export AWS_DEFAULT_REGION=us-east-1
export AWS_KVS_CACERT_PATH=/path/to/cert.pem
```

You also need to configure the video file path as the video source in `kvs_with_webrtc.h`.

```c
#define NUMBER_OF_VIDEO_FRAME_FILES     240
#define VIDEO_FRAME_FILEPATH_FORMAT     "../res/media/h264_annexb/frame-%03d.h264"
#define VIDEO_FPS                       30
```

### Build and Run Example

Run the following command to configure the CMake project.

```bash
mkdir build
cd build
cmake .. -DBUILD_WEBRTC_SAMPLES=ON
```

It may take time to build external projects for WebRTC SDK.

Run the following command to build projects.

```bash
cmake --build .
```

Run the following command to run this sample.

```bash
./bin/kvs_with_webrtc
```

# How much memroy saved

There is a rolling buffer in WebRTC. It buffers RTP packets for resend purposes. By default it buffers `DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS * ( HIGHEST_EXPECTED_BIT_RATE / 8 ) / DEFAULT_MTU_SIZE = 3277` RTP packets. Each RTP packet contains an RTP header and its payload. The payload is a chunk of a video or audio frame.

Using a data pointer instead of copying the chunk from a video or audio frame, we can save memories.

Ideally, it would save *3277 * ( MTU - RTP header + refactor overhead )* which is around **3.5MB** in maximum. It won't be that much in practice because not all RTP packets use full MTU size.

Each WebRTC viewer has its rolling buffer, saving more when we use this design with multiple WebRTC viewers.


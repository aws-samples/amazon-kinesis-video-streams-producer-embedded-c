This sample demonstrates how to port Amazon Kinesis Video Streams Producer to Ingenic T31 evaluation board.

# Prerequisite

These are prerequisites:

*   Ingenic T31 SDK ISVP-T31-1.1.3-20210223: This sample "DOES NOT" inlcude T31 SDK.  To able to build and run this sample, you need to have a T31 SDK.
*   Ingenic T31 EVB
*   Ethernet cable and network connectivity

## Device setup

Please follow "T31 SNIPE_user_guide.pdf" in the SDK to setup device and network.

### Setup DNS server

Use the following command to setup DNS server.  IP "8.8.8.8" is DNS server hosted by google.  You can choose your desired DNS server.

    echo 'nameserver 8.8.8.8' > /etc/resolv.conf

Use the following command to check if DNS is set correctly.

    # ping aws.amazon.com
    PING aws.amazon.com (13.35.25.75): 56 data bytes
    64 bytes from 13.35.25.75: seq=0 ttl=242 time=10.026 ms
    64 bytes from 13.35.25.75: seq=1 ttl=242 time=9.244 ms

### Setup NTP time

Make sure system time is synced with NTP server.  You can choose your desired NTP server.

    ntpd -nqp pool.ntp.org

Use the `date` command to check if date is set correctly.

    [root@Ingenic-uc1_1:mnt]# date
    Fri Jul 16 03:30:05 UTC 2021

### Mount SD card

In this sample, we would put the executable on the SD card.  You can skip this section if you'd like to use other folder on device.

Use the following command to mound SD card.

    mount -t vfat /dev/mmcblk0p1 /mnt

### Enable audio driver

Use following command to insert audio module.  You can skip this section if you don't want to enable audio.

    insmod /system/lib/modules/audio.ko

# Build sample

## Get code

To download run the following command:

```
git clone --recursive https://github.com/aws-samples/amazon-kinesis-video-streams-producer-embedded-c.git
```

If you miss running `git clone` command with `--recursive` , run `git submodule update --init --recursive` within repository.

## Configure and setup T31 SDK

Please put the T31 software SDK under "samples/kvsapp-ingenic-t31/sdk/".

    cp -rf /path/to/ISVP-T31-1.1.3-20210223/software/sdk/Ingenic-SDK-T31-1.1.3-20210223/sdk/4.7.2 \
        /path/to/amazon-kinesis-video-streams-producer-embedded-c/samples/kvsapp-ingenic-t31/sdk/

And the folder structure would be lik this:

    .
    ├── app
    ├── libraries
    ├── misc
    ├── samples
    │   ├── kvsapp-ingenic-t31
    │   │   ├── include
    │   │   ├── library
    │   │   ├── sdk
    │   │   │   ├── include
    │   │   │   │   ├── imp
    │   │   │   │   └── sysutils
    │   │   │   ├── lib
    │   │   │   │   ├── glibc
    │   │   │   │   └── uclibc
    │   │   │   └── samples
    │   │   │       ├── libimp-samples
    │   │   │       └── libsysutils-samples
    │   │   └── source
    │   └── mkv_uploader
    └── src

## Configure toolchain

Please add toolchain "mips-gcc472-glibc216-64bit" into you path.  Ex.

    export PATH=$PATH:/path/to/your/toolchain/mips-gcc472-glibc216-64bit/bin

## Configure sample settings

You need to configure sample settings in file "*samples/kvsapp-ingenic-t31/include/sample_config.h*".

## Build

Create a build folder.

    mkdir build && cd build

Setup cmake project

    cmake -DCMAKE_C_COMPILER=mips-linux-gnu-gcc \
        -DCMAKE_CXX_COMPILER=mips-linux-gnu-g++ \
        -DBOARD_INGENIC_T31=ON \
        ..

Build project.

    cmake --build .

After build completed, there will be exectuable files named "kvsappcli-ingenic-t31" and "kvsappcli-ingenic-t31-static" in the "build/bin/" folder.  Executable "kvsappcli-ingenic-t31" using shared library under "build/lib/" folder.  Executable "kvsappcli-ingenic-t31-static" is a static build and it doesn't need any other library except common runtime library like "libc" or "pthread".  Copy these file to T31 SD card.  To copy these files, you can either using some network tools to send this file or use a SD card reader to do that.

# Run sample

Now the executable is on the device, use the following command to run this sample.

    cd /mnt
    ./kvsappcli-ingenic-t31-static

You wold see following log if everythins works fine.

    [root@Ingenic-uc1_1:mnt]# ./kvsappcli-ingenic-t31
    AACEncoderInit   channels = 1, pcm_frame_len = 2048
    [ 5577.435278] gc2053 chip found @ 0x37 (i2c0) version H20210315a
    ---- FPGA board is ready ----
    Board UID : 30AB6E51
    Board HW ID : 72000460
    Board rev.  : 5DE5A975
    Board date  : 20190326
    -----------------------------
    !! The specified ScalingList is not allowed; it will be adjusted!!
    [ 5577.814323] codec_set_device: set device: MIC...
    Info: SPS is found
    Info: SPS is set
    Info: PPS is found
    Info: PPS is set
    Info: KVS stream buffer created
    Info: Try to describe stream
    Info: PUT MEDIA endpoint: s-xxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
    Info: 100-continue
    Info: Flush to next cluster
    Info: Fragment buffering, timecode:1628131208569
    Info: Fragment received, timecode:1628131208569
    Info: Fragment buffering, timecode:1628131212771
    Info: Fragment persisted, timecode:1628131208569
    Info: Fragment received, timecode:1628131212771
    Info: Fragment buffering, timecode:1628131216972
    Info: Fragment persisted, timecode:1628131212771
    Info: Fragment received, timecode:1628131216972
    Info: Fragment buffering, timecode:1628131220975
    Info: Fragment persisted, timecode:1628131216972
    Info: Fragment received, timecode:1628131220975
    Info: Fragment buffering, timecode:1628131224975
    Info: Fragment persisted, timecode:1628131220975

# KVS sample for Ingenic T31

After build, there will be a folder named "ingenic_t31_kvs_sample" under "build" folder.  It's a sample with prebuilt libraries.  It can ease the impelemntation without re-build all libraries.

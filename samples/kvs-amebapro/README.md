# Realtek AmebaPro example

This section describe how to run example on [AmebaPro](https://www.amebaiot.com/en/amebapro). Before run this example, these hardware are required.  

- An evaluation board of [AmebaPro](https://www.amebaiot.com/en/amebapro/).
- A camera sensor for AmebaPro.

## Prepare AmebaPro SDK environment

If you don't have a AmebaPro SDK environment, please run the following command to download AmebaPro SDK:

```
git clone --recurse-submodules https://github.com/ambiot/ambpro1_sdk.git
```

the command will also download kvs producer repository simultaneously.

## Check the model of camera sensor 

Please check your camera sensor model, and define it in <AmebaPro_SDK>/project/realtek_amebapro_v0_example/inc/sensor.h.

```
#define SENSOR_USE      	SENSOR_XXXX
```

## Configure Example Setting

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

## Build and Run Example

To build the example run the following command:

```
cd <AmebaPro_SDK>/project/realtek_amebapro_v0_example/GCC-RELEASE
make all
```

The image is located in <AmebaPro_SDK>/project/realtek_amebapro_v0_example/GCC-RELEASE/application_is/flash_is.bin

Make sure your AmebaPro is connected and powered on. Use the Realtek image tool to flash image and check the logs.

[How to use Realtek image tool? See section 1.3 in AmebaPro's application note](https://github.com/ambiot/ambpro1_sdk/blob/main/doc/AN0300%20Realtek%20AmebaPro%20application%20note.en.pdf)

## Configure WiFi Connection

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
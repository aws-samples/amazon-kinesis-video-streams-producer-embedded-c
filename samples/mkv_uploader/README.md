# MKV Uploader

In this sample, it uploads a MKV file to KVS.  It demonstrates 2 ways to upload media data to KVS.

1.  The media is stored as MKV file, or
2.  The media is buffered as MKV format.

In second way it means application can implement their solution of buffering if the buffer content follows MKV format.

## Restriction of MKV format

The timesamps of each cluster must be in order and in UNIX time (a.k.a. epoch time).

In most cases, MKV file start with timestamp zero.  If we upload this MKV file, KVS would think it starts at midnight 1/1/1970 (absolute time) or at current time (relative time).  Both of 2 timestamps are not accetable.  To fix this, the timestamp of MKV file must be edited with a proper epoch timestamp.

# Build and Run Examples

## Build sample

Please refer to the sample "kvs-linux" to setup IoT certification and KVS settings.

After build, there will be 2 executables:

*   mkv_update_time: This is a tool that can update timestamps of a MKV file.
*   mkv_uploader: This is sample that can upload a MKV file to KVS.

## Prepare a MKV file

If you don't have a MKV file you can download sample file here ![mp4-sample-video-files-download](https://www.learningcontainer.com/mp4-sample-video-files-download/).  Assume the file name is `sample-mp4-file.mp4`, and we can convert it to MKV by using this ffmpeg command:

```
ffmpeg -i sample-mp4-file.mp4 -vcodec copy -acodec copy sample.mkv
```

This MKV file starts with timestamp 0.  We can use `mkv_update_time` to update its timestamps:

```
./mkv_update_time -i /path/to/sample.mkv -o /path/to/sample_updated.mkv -t -180000
```

This command specifiy the input MKV file, the output MKV file, and the updated timestamp.  In this example, it updates to the current time minus 180000 milliseconds (which is 3 minutes).

## Upload MKV file to KVS

Use following command to upload MKV file to KVS.

```
./mkv_uploader -i /path/to/sample_updated.mkv
```

It everything works fine, you would see similar logs to sample kvs-linux.

```
Try to describe stream
PUT MEDIA endpoint: s-xxxxxxxx.kinesisvideo.us-east-1.amazonaws.com
Try to put media
Info: 100-continue
Info: Fragment buffering, timecode:1621392521084
Info: Fragment received, timecode:1621392521084
Info: Fragment buffering, timecode:1621392526097
Info: Fragment persisted, timecode:1621392521084
......
Info: Fragment buffering, timecode:1621392641404
Info: Fragment persisted, timecode:1621392636668
Info: Fragment persisted, timecode:1621392639612
Leaving put media
```

Now you can check this video on ![KVS dashboard](https://console.aws.amazon.com/kinesisvideo/home?region=us-east-1#/dashboard).  You can modify the HLS timestamp to earlier, then the HLS should able to find this video and play.
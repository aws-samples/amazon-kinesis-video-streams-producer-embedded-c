# Porting AAC encoder

Advanced Audio Coding (AAC) is an audio coding standard commonly used. No licenses or payments are required for a user to stream or distribute content in AAC format. However, all manufacturers of AAC codecs require a patent license. Once the manufacturers get the patent license, they need an AAC encoder in the embedded producer. The AAC encoder may come with a hardware/software encoder in SDK provided by ODM. If it doesn't, they need to port an AAC software encoder independently.

# ADTS header

Most AAC raw data comes with Audio Data Transport Stream (ADTS) header. It describes the data length, audio sample rate, channel number, and other information.

However, the MKV format uploaded to Kinesis Video Stream doesn't require an ADTS header. So we have to remove it. We may disable the ADTS header by configuring the AAC encoder or removing it manually. The ADTS header is usually 7 or 9 bytes long, and the first 12 bits are all 1s. So we can see "`0xFFF.....`" when we check the AAC data in hex format. The following page shows more details of the ADTS format.

[https://wiki.multimedia.cx/index.php/ADTS](https://wiki.multimedia.cx/index.php/ADTS)

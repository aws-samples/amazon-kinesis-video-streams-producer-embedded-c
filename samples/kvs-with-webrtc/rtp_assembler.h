#ifndef RTP_BUFFER_H
#define RTP_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Refer these payload type values in PeerConnection/SessionDescription.h */
#define RTP_PAYLOAD_TYPE_MULAW  0
#define RTP_PAYLOAD_TYPE_ALAW   8
#define RTP_PAYLOAD_TYPE_OPUS   111
#define RTP_PAYLOAD_TYPE_VP8    96
#define RTP_PAYLOAD_TYPE_H264   125

typedef struct RtpAssembler *RtpAssemblerHandle;

RtpAssemblerHandle RtpAssembler_create();

void RtpAssembler_terminate(RtpAssemblerHandle handle);

int RtpAssembler_pushRtpPacket(RtpAssemblerHandle handle, uint8_t *pRtpPacket, size_t uRtpPacketLen);

bool RtpAssembler_isFrameAvailable(RtpAssemblerHandle handle);

int RtpAssembler_getFrame(RtpAssemblerHandle handle, uint8_t **ppFrame, size_t *puFrameLen, uint8_t *puPayloadType, uint64_t *puTimestampMs);

#endif
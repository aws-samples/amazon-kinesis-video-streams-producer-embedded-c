#include "Samples.h"

extern PSampleConfiguration gSampleConfiguration;

#include <inttypes.h>
#include <kvs/kvsapp.h>
#include <kvs/port.h>
#include "rtp_assembler.h"

// #define VERBOSE
#ifdef KVS_USE_POOL_ALLOCATOR
#include "kvs/pool_allocator.h"
static char pMemPool[POOL_ALLOCATOR_SIZE];
#endif

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    PSampleConfiguration pSampleConfiguration = NULL;
    SignalingClientMetrics signalingClientMetrics;
    PCHAR pChannelName;
    signalingClientMetrics.version = 0;
    CHAR filePath[MAX_PATH_LEN + 1];

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorInit((void *)pMemPool, sizeof(pMemPool));
#endif

    SET_INSTRUMENTED_ALLOCATORS();

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

    // do trickleIce by default
    printf("[KVS Master] Using trickleICE by default\n");

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = getenv(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

    retStatus = createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, &pSampleConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] createSampleConfiguration(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Master] Created signaling channel %s\n", pChannelName);

    if (pSampleConfiguration->enableFileLogging) {
        retStatus =
            createFileLogger(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE, TRUE, NULL);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] createFileLogger(): operation returned status code: 0x%08x \n", retStatus);
            pSampleConfiguration->enableFileLogging = FALSE;
        }
    }

    // Set the audio and video handlers
    pSampleConfiguration->videoSource = sendVideoPackets;
    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveVideoFrame;
    pSampleConfiguration->onDataChannel = onDataChannel;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    printf("[KVS Master] Finished setting audio and video handlers\n");

    // Check if the samples are present
    snprintf(filePath, MAX_PATH_LEN, VIDEO_FRAME_FILEPATH_FORMAT, 1);
    retStatus = readFrameFromDisk(NULL, &frameSize, filePath);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Checked sample video frame availability....available\n");

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    retStatus = initKvsWebRtc();
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] initKvsWebRtc(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] KVS WebRTC initialization completed successfully\n");

    pSampleConfiguration->signalingClientCallbacks.messageReceivedFn = signalingMessageReceived;

    strcpy(pSampleConfiguration->clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

    retStatus = createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                          &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                          &pSampleConfiguration->signalingClientHandle);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] createSignalingClientSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Signaling client created successfully\n");

    // Enable the processing of the messages
    retStatus = signalingClientConnectSync(pSampleConfiguration->signalingClientHandle);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] signalingClientConnectSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Master] Signaling client connection to socket established\n");

    gSampleConfiguration = pSampleConfiguration;

    printf("[KVS Master] Channel %s set up done \n", pChannelName);

    THREAD_CREATE(&pSampleConfiguration->producerTid, producer, (PVOID) pSampleConfiguration);

    // Checking for termination
    retStatus = sessionCleanupWait(pSampleConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] sessionCleanupWait(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Master] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    printf("[KVS Master] Cleaning up....\n");
    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
        }

        if (pSampleConfiguration->enableFileLogging) {
            freeFileLogger();
        }
        retStatus = signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);
        if (retStatus == STATUS_SUCCESS) {
            logSignalingClientStats(&signalingClientMetrics);
        } else {
            printf("[KVS Master] signalingClientGetMetrics() operation returned status code: 0x%08x", retStatus);
        }
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSignalingClient(): operation returned status code: 0x%08x", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSampleConfiguration(): operation returned status code: 0x%08x", retStatus);
        }
    }
    printf("[KVS Master] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();

#ifdef KVS_USE_POOL_ALLOCATOR
    poolAllocatorDeinit();
#endif

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;

    if (pSize == NULL) {
        printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    size = *pSize;

    // Get the size and read into frame
    retStatus = readFile(frameFilePath, TRUE, pFrame, &size);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] readFile(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

PVOID sendVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    RtcEncoderStats encoderStats;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;
    UINT64 startTime, lastFrameTime, elapsed;
    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));

    printf("%s(): ++\n", __FUNCTION__);

    if (pSampleConfiguration == NULL) {
        printf("[KVS Master] sendVideoPackets(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        snprintf(filePath, MAX_PATH_LEN, VIDEO_FRAME_FILEPATH_FORMAT, fileIndex);

        retStatus = readFrameFromDisk(NULL, &frameSize, filePath);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            if (pSampleConfiguration->pVideoFrameBuffer == NULL) {
                printf("[KVS Master] Video frame Buffer reallocation failed...%s (code %d)\n", strerror(errno), errno);
                printf("[KVS Master] MEMREALLOC(): operation returned status code: 0x%08x \n", STATUS_NOT_ENOUGH_MEMORY);
                goto CleanUp;
            }

            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;

        retStatus = readFrameFromDisk(frame.frameData, &frameSize, filePath);
        if (retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] readFrameFromDisk(): operation returned status code: 0x%08x \n", retStatus);
            goto CleanUp;
        }

        // based on bitrate of samples/h264SampleFrames/frame-*
        encoderStats.width = 640;
        encoderStats.height = 480;
        encoderStats.targetBitrate = 262000;
        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            encoderStats.encodeTimeMsec = 4; // update encode time to an arbitrary number to demonstrate stats update
            updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &encoderStats);
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
#ifdef VERBOSE
                    printf("writeFrame() failed with 0x%08x\n", status);
#endif
                }
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        // Adjust sleep in the case the sleep itself and writeFrame take longer than expected. Since sleep makes sure that the thread
        // will be paused at least until the given amount, we can assume that there's no too early frame scenario.
        // Also, it's very unlikely to have a delay greater than SAMPLE_VIDEO_FRAME_DURATION, so the logic assumes that this is always
        // true for simplicity.
        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    printf("%s(): --\n", __FUNCTION__);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    if (pSampleStreamingSession == NULL) {
        printf("[KVS Master] sampleReceiveVideoFrame(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    retStatus = transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleFrameHandler);
    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] transceiverOnFrame(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}

static uint8_t gRawPacketBuffer[65536];

static uint32_t writeToFile( char *pFilename, uint8_t *pBuf, uint32_t uBuflen)
{
    FILE *fp = NULL;

    if (pFilename == NULL ) return 0;
    fp = fopen( pFilename, "wb" );
    if (fp == NULL) return 0;

    fwrite(pBuf, 1, uBuflen, fp);
    fclose( fp);
}

PVOID producer(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    PRtcRtpTransceiver pRtcRtpTransceiver = NULL;
    RtpAssemblerHandle rtpAssemblerHandle = NULL;

    uint64_t uProcessedPktIndex = 0;
    uint64_t uHeadPktIndex = 0;
    uint64_t uTailPktIndex = 0;
    uint8_t *pRawPacket = gRawPacketBuffer;
    uint32_t uRawPacketLength = 0;

    KvsAppHandle kvsAppHandle = NULL;
    uint64_t uProducerTimestampBase = 0;

    if (pSampleConfiguration == NULL) {
        printf("[KVS Master] producer(): operation returned status code: 0x%08x \n", STATUS_NULL_ARG);
        goto CleanUp;
    }

    if ((rtpAssemblerHandle = RtpAssembler_create()) == NULL) {
        printf("Failed to create RtpAssembler\n");
        goto CleanUp;
    }

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag))
    {
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        if (pSampleConfiguration->streamingSessionCount > 0)
        {
            pRtcRtpTransceiver = pSampleConfiguration->sampleStreamingSessionList[0]->pVideoRtcRtpTransceiver;

            getRtpRollingBufferIndex(pRtcRtpTransceiver, &uHeadPktIndex, &uTailPktIndex);
            while (uProcessedPktIndex < uHeadPktIndex)
            {
                getRtpRollingBufferData(pRtcRtpTransceiver, uProcessedPktIndex, pRawPacket, 65536, &uRawPacketLength);

                RtpAssembler_pushRtpPacket(rtpAssemblerHandle, pRawPacket, uRawPacketLength);
                uProcessedPktIndex++;

                /* If we have a complete frame to send, then we should unlock session list then send this frame. */
                if (RtpAssembler_isFrameAvailable(rtpAssemblerHandle))
                {
                    break;
                }
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        if (RtpAssembler_isFrameAvailable(rtpAssemblerHandle))
        {
            uint8_t *pFrame = NULL;
            size_t uFrameLen = 0;
            uint8_t uPayloadType = 0;
            uint64_t uTimestamp = 0;

            while(RtpAssembler_getFrame(rtpAssemblerHandle, &pFrame, &uFrameLen, &uPayloadType, &uTimestamp) == 0)
            {
                if (kvsAppHandle == NULL)
                {
                    kvsAppHandle = KvsApp_create(AWS_KVS_HOST, AWS_KVS_REGION, AWS_KVS_SERVICE, KVS_STREAM_NAME);
                    KvsApp_setoption(kvsAppHandle, OPTION_AWS_ACCESS_KEY_ID, (const char *)AWS_ACCESS_KEY);
                    KvsApp_setoption(kvsAppHandle, OPTION_AWS_SECRET_ACCESS_KEY, (const char *)AWS_SECRET_KEY);
                    KvsApp_open(kvsAppHandle);
                }

                if (uProducerTimestampBase == 0)
                {
                    uProducerTimestampBase = getEpochTimestampInMs() - uTimestamp;
                }

                uTimestamp += uProducerTimestampBase;
                KvsApp_addFrame(kvsAppHandle, pFrame, uFrameLen, uFrameLen, uTimestamp, TRACK_VIDEO);
                KvsApp_doWork(kvsAppHandle);

#ifdef KVS_USE_POOL_ALLOCATOR
                PoolStats_t stats = {0};
                poolAllocatorGetStats(&stats);
                printf("Sum of used/free memory:%zu/%zu, size of largest used/free block:%zu/%zu, number of used/free blocks:%zu/%zu\n",
                       stats.uSumOfUsedMemory, stats.uSumOfFreeMemory,
                       stats.uSizeOfLargestUsedBlock, stats.uSizeOfLargestFreeBlock,
                       stats.uNumberOfUsedBlocks, stats.uNumberOfFreeBlocks
                );
#endif

                //free(pFrame);
            }
        }
        else
        {
            THREAD_SLEEP(20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

CleanUp:

    RtpAssembler_terminate(rtpAssemblerHandle);

    if (kvsAppHandle != NULL)
    {
        KvsApp_close(kvsAppHandle);
        KvsApp_terminate(kvsAppHandle);
    }

    CHK_LOG_ERR(retStatus);

    return (PVOID) (ULONG_PTR) retStatus;
}
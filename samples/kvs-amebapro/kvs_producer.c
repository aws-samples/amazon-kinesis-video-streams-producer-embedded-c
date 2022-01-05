#include "platform_opts.h"
#if CONFIG_EXAMPLE_KVS_PRODUCER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Headers for example */
#include "sample_config.h"
#include "kvs_producer.h"

/* Headers for FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Headers for video */
#include "video_common_api.h"
#include "h264_encoder.h"
#include "isp_api.h"
#include "h264_api.h"
#include "sensor.h"

#if ENABLE_AUDIO_TRACK
#include "audio_api.h"
#include "faac.h"
#include "faac_api.h"
#endif /* ENABLE_AUDIO_TRACK */

/* Headers for KVS */
#include "kvs/port.h"
#include "kvs/nalu.h"
#include "kvs/restapi.h"
#include "kvs/stream.h"

#if ENABLE_IOT_CREDENTIAL
#include "kvs/iot_credential_provider.h"
#endif /* ENABLE_IOT_CREDENTIAL */

#define ERRNO_NONE      0
#define ERRNO_FAIL      __LINE__

#define KVS_VIDEO_THREAD_STACK_SIZE     1024
#define KVS_AUDIO_THREAD_STACK_SIZE     1024

#define ISP_SW_BUF_NUM      3

#define ISP_STREAM_ID       0
#define ISP_HW_SLOT         1
#define VIDEO_OUTPUT_BUFFER_SIZE    ( VIDEO_HEIGHT * VIDEO_WIDTH / 10 )

#if ENABLE_AUDIO_TRACK
#define AUDIO_DMA_PAGE_NUM 2
#define AUDIO_DMA_PAGE_SIZE ( 2 * 1024 )

#define RX_PAGE_SIZE 	AUDIO_DMA_PAGE_SIZE //64*N bytes, max: 4095. 128, 4032
#define RX_PAGE_NUM 	AUDIO_DMA_PAGE_NUM

static audio_t audio;
static uint8_t dma_rxdata[ RX_PAGE_SIZE * RX_PAGE_NUM ]__attribute__ ((aligned (0x20))); 

#define AUDIO_QUEUE_DEPTH         ( 10 )
static xQueueHandle audioQueue;
#endif /* ENABLE_AUDIO_TRACK */

typedef struct isp_s
{
    isp_stream_t* stream;
    
    isp_cfg_t cfg;
    
    isp_buf_t buf_item[ISP_SW_BUF_NUM];
    xQueueHandle output_ready;
    xQueueHandle output_recycle;
} isp_t;

typedef struct Kvs
{
#if ENABLE_IOT_CREDENTIAL
    IotCredentialRequest_t xIotCredentialReq;
#endif

    KvsServiceParameter_t xServicePara;
    KvsDescribeStreamParameter_t xDescPara;
    KvsCreateStreamParameter_t xCreatePara;
    KvsGetDataEndpointParameter_t xGetDataEpPara;
    KvsPutMediaParameter_t xPutMediaPara;

    StreamHandle xStreamHandle;
    PutMediaHandle xPutMediaHandle;

    VideoTrackInfo_t *pVideoTrackInfo;
    AudioTrackInfo_t *pAudioTrackInfo;
} Kvs_t;

static void sleepInMs( uint32_t ms )
{
    vTaskDelay( ms / portTICK_PERIOD_MS );
}

void isp_frame_cb(void* p)
{
    BaseType_t xTaskWokenByReceive = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken;
    
    isp_t* ctx = (isp_t*)p;
    isp_info_t* info = &ctx->stream->info;
    isp_cfg_t *cfg = &ctx->cfg;
    isp_buf_t buf;
    isp_buf_t queue_item;
    
    int is_output_ready = 0;
    
    u32 timestamp = xTaskGetTickCountFromISR();
    
    if(info->isp_overflow_flag == 0){
        is_output_ready = xQueueReceiveFromISR(ctx->output_recycle, &buf, &xTaskWokenByReceive) == pdTRUE;
    }else{
        info->isp_overflow_flag = 0;
        //ISP_DBG_INFO("isp overflow = %d\r\n", cfg->isp_id);
    }
    
    if(is_output_ready){
        isp_handle_buffer(ctx->stream, &buf, MODE_EXCHANGE);
        xQueueSendFromISR(ctx->output_ready, &buf, &xHigherPriorityTaskWoken);    
    }else{
        isp_handle_buffer(ctx->stream, NULL, MODE_SKIP);
    }
    if( xHigherPriorityTaskWoken || xTaskWokenByReceive )
        taskYIELD ();
}

int KvsVideoInitTrackInfo(VIDEO_BUFFER *pVideoBuf, Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    uint8_t *pVideoCpdData = NULL;
    uint32_t uCpdLen = 0;

//    printf("Buf(%d):\r\n", pVideoBuf->output_size);
//    for (size_t i = 0; i<64; i++)
//    {
//        printf("%02X ", (pVideoBuf->output_buffer[i]) & 0xFF);
//        if ( ((i+1) % 16) == 0 ) printf("\r\n");
//    }
//    printf("\r\n");

    if (pVideoBuf == NULL || pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else if (pKvs->pVideoTrackInfo != NULL)
    {
        res = ERRNO_FAIL;
    }
    else if (Mkv_generateH264CodecPrivateDataFromAnnexBNalus(pVideoBuf->output_buffer, pVideoBuf->output_size, &pVideoCpdData, &uCpdLen) != 0)
    {
        res = ERRNO_FAIL;
    }
    else if ((pKvs->pVideoTrackInfo = (VideoTrackInfo_t *)malloc(sizeof(VideoTrackInfo_t))) == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pKvs->pVideoTrackInfo, 0, sizeof(VideoTrackInfo_t));

        pKvs->pVideoTrackInfo->pTrackName = VIDEO_NAME;
        pKvs->pVideoTrackInfo->pCodecName = VIDEO_CODEC_NAME;
        pKvs->pVideoTrackInfo->uWidth = VIDEO_WIDTH;
        pKvs->pVideoTrackInfo->uHeight = VIDEO_HEIGHT;
        pKvs->pVideoTrackInfo->pCodecPrivate = pVideoCpdData;
        pKvs->pVideoTrackInfo->uCodecPrivateLen = uCpdLen;
        printf("\r[Video resolution] w: %d h: %d\r\n", pKvs->pVideoTrackInfo->uWidth, pKvs->pVideoTrackInfo->uHeight);
    }

    return res;
}

static void sendVideoFrame(VIDEO_BUFFER *pBuffer, Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    DataFrameIn_t xDataFrameIn = {0};
    uint32_t uAvccLen = 0;
    uint8_t *pVideoCpdData = NULL;
    uint32_t uCpdLen = 0;
    size_t uMemTotal = 0;

    if (pBuffer == NULL || pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pKvs->xStreamHandle != NULL)
        {
            xDataFrameIn.bIsKeyFrame = h264_is_i_frame(pBuffer->output_buffer) ? true : false;
            xDataFrameIn.uTimestampMs = getEpochTimestampInMs();
            xDataFrameIn.xTrackType = TRACK_VIDEO;

            xDataFrameIn.xClusterType = (xDataFrameIn.bIsKeyFrame) ? MKV_CLUSTER : MKV_SIMPLE_BLOCK;

            if (NALU_convertAnnexBToAvccInPlace(pBuffer->output_buffer, pBuffer->output_size, pBuffer->output_buffer_size, &uAvccLen) != ERRNO_NONE)
            {
                printf("Failed to convert Annex-B to AVCC\r\n");
                res = ERRNO_FAIL;
            }
            else if ((xDataFrameIn.pData = (char *)malloc(uAvccLen)) == NULL)
            {
                printf("OOM: xDataFrameIn.pData\r\n");
                res = ERRNO_FAIL;
            }
            else if (Kvs_streamMemStatTotal(pKvs->xStreamHandle, &uMemTotal) != 0)
            {
                printf("Failed to get stream mem state\r\n");
            }
            else
            {
                if (uMemTotal < STREAM_MAX_BUFFERING_SIZE)
                {
                    memcpy(xDataFrameIn.pData, pBuffer->output_buffer, uAvccLen);
                    xDataFrameIn.uDataLen = uAvccLen;

                    Kvs_streamAddDataFrame(pKvs->xStreamHandle, &xDataFrameIn);
                }
                else
                {
                    free(xDataFrameIn.pData);
                }
            }
        }
        free(pBuffer->output_buffer);
    }

}

static void camera_thread(void *param)
{
    int ret;
    VIDEO_BUFFER video_buf;
    u8 start_recording = 0;
    isp_init_cfg_t isp_init_cfg;
    isp_t isp_ctx;
    Kvs_t *pKvs = (Kvs_t *)param;

    vTaskDelay( 2000 / portTICK_PERIOD_MS );

    printf("[H264] init video related settings\n\r");
    
    memset(&isp_init_cfg, 0, sizeof(isp_init_cfg));
    isp_init_cfg.pin_idx = ISP_PIN_IDX;
    isp_init_cfg.clk = SENSOR_CLK_USE;
    isp_init_cfg.ldc = LDC_STATE;
    isp_init_cfg.fps = SENSOR_FPS;
    isp_init_cfg.isp_fw_location = ISP_FW_LOCATION;
    
    video_subsys_init(&isp_init_cfg);
#if CONFIG_LIGHT_SENSOR
    init_sensor_service();
#else
    ir_cut_init(NULL);
    ir_cut_enable(1);
#endif
    
    printf("[H264] create encoder\n\r");
    struct h264_context* h264_ctx;
    ret = h264_create_encoder(&h264_ctx);
    if (ret != H264_OK) {
        printf("\n\rh264_create_encoder err %d\n\r",ret);
        goto exit;
    }

    printf("[H264] get & set encoder parameters\n\r");
    struct h264_parameter h264_parm;
    ret = h264_get_parm(h264_ctx, &h264_parm);
    if (ret != H264_OK) {
        printf("\n\rh264_get_parmeter err %d\n\r",ret);
        goto exit;
    }
    
    h264_parm.height = VIDEO_HEIGHT;
    h264_parm.width = VIDEO_WIDTH;
    h264_parm.rcMode = H264_RC_MODE_CBR;
    h264_parm.bps = 1 * 1024 * 1024;
    h264_parm.ratenum = VIDEO_FPS;
    h264_parm.gopLen = 30;
    
    ret = h264_set_parm(h264_ctx, &h264_parm);
    if (ret != H264_OK) {
        printf("\n\rh264_set_parmeter err %d\n\r",ret);
        goto exit;
    }
    
    printf("[H264] init encoder\n\r");
    ret = h264_init_encoder(h264_ctx);
    if (ret != H264_OK) {
        printf("\n\rh264_init_encoder_buffer err %d\n\r",ret);
        goto exit;
    }
    
    printf("[ISP] init ISP\n\r");
    memset(&isp_ctx,0,sizeof(isp_ctx));
    isp_ctx.output_ready = xQueueCreate(ISP_SW_BUF_NUM, sizeof(isp_buf_t));
    isp_ctx.output_recycle = xQueueCreate(ISP_SW_BUF_NUM, sizeof(isp_buf_t));
    
    isp_ctx.cfg.isp_id = ISP_STREAM_ID;
    isp_ctx.cfg.format = ISP_FORMAT_YUV420_SEMIPLANAR;
    isp_ctx.cfg.width = VIDEO_WIDTH;
    isp_ctx.cfg.height = VIDEO_HEIGHT;
    isp_ctx.cfg.fps = VIDEO_FPS;
    isp_ctx.cfg.hw_slot_num = ISP_HW_SLOT;
    
    isp_ctx.stream = isp_stream_create(&isp_ctx.cfg);
    
    isp_stream_set_complete_callback(isp_ctx.stream, isp_frame_cb, (void*)&isp_ctx);
    
    for (int i=0; i<ISP_SW_BUF_NUM; i++ ) {
        unsigned char *ptr =(unsigned char *) malloc( VIDEO_WIDTH * VIDEO_HEIGHT * 3 / 2 );
        if (ptr==NULL) {
            printf("[ISP] Allocate isp buffer[%d] failed\n\r",i);
            while(1);
        }
        isp_ctx.buf_item[i].slot_id = i;
        isp_ctx.buf_item[i].y_addr = (uint32_t) ptr;
        isp_ctx.buf_item[i].uv_addr = isp_ctx.buf_item[i].y_addr + VIDEO_WIDTH * VIDEO_HEIGHT;
        
        if( i < ISP_HW_SLOT ) {
            // config hw slot
            //printf("\n\rconfig hw slot[%d] y=%x, uv=%x\n\r",i,isp_ctx.buf_item[i].y_addr,isp_ctx.buf_item[i].uv_addr);
            isp_handle_buffer(isp_ctx.stream, &isp_ctx.buf_item[i], MODE_SETUP);
        }
        else {
            // extra sw buffer
            //printf("\n\rextra sw buffer[%d] y=%x, uv=%x\n\r",i,isp_ctx.buf_item[i].y_addr,isp_ctx.buf_item[i].uv_addr);
            if(xQueueSend(isp_ctx.output_recycle, &isp_ctx.buf_item[i], 0)!= pdPASS) {
                printf("[ISP] Queue send fail\n\r");
                while(1);
            }
        }
    }
    
    isp_stream_apply(isp_ctx.stream);
    isp_stream_start(isp_ctx.stream);
    
    printf("Start recording\n");
    int enc_cnt = 0;
    while ( 1 )
    {
        // [ISP] get isp data
        isp_buf_t isp_buf;
        if(xQueueReceive(isp_ctx.output_ready, &isp_buf, 10) != pdTRUE) {
            continue;
        }

        // [H264] encode data
        video_buf.output_buffer_size = VIDEO_OUTPUT_BUFFER_SIZE;
        video_buf.output_buffer = malloc( VIDEO_OUTPUT_BUFFER_SIZE );
        if (video_buf.output_buffer== NULL) {
            printf("Allocate output buffer fail\n\r");
            continue;
        }
        ret = h264_encode_frame(h264_ctx, &isp_buf, &video_buf);
        if (ret != H264_OK) {
            printf("\n\rh264_encode_frame err %d\n\r",ret);
            if (video_buf.output_buffer != NULL)
                free(video_buf.output_buffer);
            continue;
        }
        enc_cnt++;
        
        // [ISP] put back isp buffer
        xQueueSend(isp_ctx.output_recycle, &isp_buf, 10);
        
        // send video frame
        if (start_recording)
        {
            sendVideoFrame(&video_buf, pKvs);
        }
        else
        {
            if (h264_is_i_frame(video_buf.output_buffer))
            {
                start_recording = 1;
                if (pKvs->pVideoTrackInfo == NULL)
                {
                    KvsVideoInitTrackInfo(&video_buf, pKvs);
                }
                sendVideoFrame(&video_buf, pKvs);
            }
            else
            {
                if (video_buf.output_buffer != NULL)
                    free(video_buf.output_buffer);
            }
        }
    }
    
exit:
    isp_stream_stop(isp_ctx.stream);
    xQueueReset(isp_ctx.output_ready);
    xQueueReset(isp_ctx.output_recycle);
    for (int i=0; i<ISP_SW_BUF_NUM; i++ ) {
        unsigned char* ptr = (unsigned char*) isp_ctx.buf_item[i].y_addr;
        if (ptr) 
            free(ptr);
    }

    vTaskDelete( NULL );
}

static int initCamera(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;

    if (xTaskCreate(camera_thread, ((const char*)"camera_thread"), KVS_VIDEO_THREAD_STACK_SIZE, pKvs, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    {
        printf("Failed to create camera thread\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        while (pKvs->pVideoTrackInfo == NULL)
        {
            sleepInMs(50);
        }
    }

    return res;
}

#if ENABLE_AUDIO_TRACK
static void audio_rx_complete(uint32_t arg, uint8_t *pbuf)
{
    BaseType_t xHigherPriorityTaskWoken = pdTRUE;

    xQueueSendFromISR(audioQueue, &pbuf, &xHigherPriorityTaskWoken);
}

int KvsAudioInitTrackInfo(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    uint8_t *pCodecPrivateData = NULL;
    uint32_t uCodecPrivateDataLen = 0;

    pKvs->pAudioTrackInfo = (AudioTrackInfo_t *)malloc(sizeof(AudioTrackInfo_t));
    memset(pKvs->pAudioTrackInfo, 0, sizeof(AudioTrackInfo_t));
    pKvs->pAudioTrackInfo->pTrackName = AUDIO_NAME;
    pKvs->pAudioTrackInfo->pCodecName = AUDIO_CODEC_NAME;
    pKvs->pAudioTrackInfo->uFrequency = AUDIO_SAMPLING_RATE;
    pKvs->pAudioTrackInfo->uChannelNumber = AUDIO_CHANNEL_NUMBER;

    if (Mkv_generateAacCodecPrivateData(MPEG4_AAC_LC, AUDIO_SAMPLING_RATE, AUDIO_CHANNEL_NUMBER, &pCodecPrivateData, &uCodecPrivateDataLen) == 0)
    {
        pKvs->pAudioTrackInfo->pCodecPrivate = pCodecPrivateData;
        pKvs->pAudioTrackInfo->uCodecPrivateLen = (uint32_t)uCodecPrivateDataLen;
    }
}

static void audio_thread(void *param)
{
    uint8_t *pAudioRxBuffer = NULL;
    uint8_t *pAudioFrame = NULL;
    DataFrameIn_t xDataFrameIn = {0};
    Kvs_t *pKvs = (Kvs_t *)param;
    size_t uMemTotal = 0;

    vTaskDelay( 2000 / portTICK_PERIOD_MS );

    /* AAC initialize */
    faacEncHandle faac_enc;
    int samples_input;
    int max_bytes_output;
    uint8_t *pAacFrame = NULL;

    aac_encode_init(&faac_enc, FAAC_INPUT_16BIT, 0, AUDIO_SAMPLING_RATE, AUDIO_CHANNEL_NUMBER, MPEG4, &samples_input, &max_bytes_output );

    audioQueue = xQueueCreate(AUDIO_QUEUE_DEPTH, sizeof(uint8_t *));
    xQueueReset(audioQueue);

    audio_init(&audio, OUTPUT_SINGLE_EDNED, MIC_DIFFERENTIAL, AUDIO_CODEC_2p8V);
    audio_set_param(&audio, ASR_8KHZ, WL_16BIT);
    audio_mic_analog_gain(&audio, 1, MIC_40DB);

    audio_set_rx_dma_buffer(&audio, dma_rxdata, RX_PAGE_SIZE);
    audio_rx_irq_handler(&audio, audio_rx_complete, NULL);

    audio_rx_start(&audio);

    /* Initialize audio track info */
    KvsAudioInitTrackInfo(pKvs);

    while (1)
    {
        xQueueReceive(audioQueue, &pAudioRxBuffer, portMAX_DELAY);

        pAudioFrame = (uint8_t *)malloc(RX_PAGE_SIZE);
        if( pAudioFrame == NULL )
        {
            printf("OOM: pAudioFrame\r\n");
            continue;
        }

        memcpy(pAudioFrame, pAudioRxBuffer, RX_PAGE_SIZE);
        audio_set_rx_page(&audio);

        memset(&xDataFrameIn, 0, sizeof(DataFrameIn_t));
        xDataFrameIn.bIsKeyFrame = false;
        xDataFrameIn.uTimestampMs = getEpochTimestampInMs();
        xDataFrameIn.xTrackType = TRACK_AUDIO;

        xDataFrameIn.xClusterType = MKV_SIMPLE_BLOCK;

        pAacFrame = (uint8_t *)malloc( max_bytes_output );
        if( pAacFrame == NULL )
        {
            printf("%s(): out of memory\r\n");
            continue;
        }
        int xFrameSize = aac_encode_run(faac_enc, pAudioFrame, 1024, pAacFrame, max_bytes_output);
        xDataFrameIn.pData = (char *)pAacFrame;
        xDataFrameIn.uDataLen = xFrameSize;
        free(pAudioFrame);
        
        if (Kvs_streamMemStatTotal(pKvs->xStreamHandle, &uMemTotal) != 0)
        {
            printf("Failed to get stream mem state\r\n");
        }
        else if ( (pKvs->xStreamHandle != NULL) && (uMemTotal < STREAM_MAX_BUFFERING_SIZE))
        {
            Kvs_streamAddDataFrame(pKvs->xStreamHandle, &xDataFrameIn);
        }
        else
        {
            free(xDataFrameIn.pData);
        }
    }
}

static int initAudio(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;

    if (xTaskCreate(audio_thread, ((const char*)"audio_thread"), KVS_AUDIO_THREAD_STACK_SIZE, pKvs, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    {
        printf("Failed to create camera thread\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        while (pKvs->pAudioTrackInfo == NULL)
        {
            sleepInMs(50);
            printf("");
        }
    }

    return res;
}
#endif /* ENABLE_AUDIO_TRACK */

static int kvsInitialize(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    char *pcStreamName = KVS_STREAM_NAME;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        memset(pKvs, 0, sizeof(Kvs_t));

        pKvs->xServicePara.pcHost = AWS_KVS_HOST;
        pKvs->xServicePara.pcRegion = AWS_KVS_REGION;
        pKvs->xServicePara.pcService = AWS_KVS_SERVICE;
        pKvs->xServicePara.pcAccessKey = AWS_ACCESS_KEY;
        pKvs->xServicePara.pcSecretKey = AWS_SECRET_KEY;

        pKvs->xDescPara.pcStreamName = pcStreamName;

        pKvs->xCreatePara.pcStreamName = pcStreamName;
        pKvs->xCreatePara.uDataRetentionInHours = 2;

        pKvs->xGetDataEpPara.pcStreamName = pcStreamName;

        pKvs->xPutMediaPara.pcStreamName = pcStreamName;
        pKvs->xPutMediaPara.xTimecodeType = TIMECODE_TYPE_ABSOLUTE;

#if ENABLE_IOT_CREDENTIAL
        pKvs->xIotCredentialReq.pCredentialHost = CREDENTIALS_HOST;
        pKvs->xIotCredentialReq.pRoleAlias = ROLE_ALIAS;
        pKvs->xIotCredentialReq.pThingName = THING_NAME;
        pKvs->xIotCredentialReq.pRootCA = ROOT_CA;
        pKvs->xIotCredentialReq.pCertificate = CERTIFICATE;
        pKvs->xIotCredentialReq.pPrivateKey = PRIVATE_KEY;
#endif
    }

    return res;
}

static void kvsTerminate(Kvs_t *pKvs)
{
    if (pKvs != NULL)
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
            free(pKvs->xServicePara.pcPutMediaEndpoint);
            pKvs->xServicePara.pcPutMediaEndpoint = NULL;
        }
    }
}

static int setupDataEndpoint(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

    if (pKvs == NULL)
    {
        res = ERRNO_FAIL;
    }
    else
    {
        if (pKvs->xServicePara.pcPutMediaEndpoint != NULL)
        {
        }
        else
        {
            printf("Try to describe stream\r\n");
            if (Kvs_describeStream(&(pKvs->xServicePara), &(pKvs->xDescPara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
            {
                printf("Failed to describe stream\r\n");
                printf("Try to create stream\r\n");
                if (Kvs_createStream(&(pKvs->xServicePara), &(pKvs->xCreatePara), &uHttpStatusCode) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to create stream\r\n");
                    res = ERRNO_FAIL;
                }
            }

            if (res == ERRNO_NONE)
            {
                if (Kvs_getDataEndpoint(&(pKvs->xServicePara), &(pKvs->xGetDataEpPara), &uHttpStatusCode, &(pKvs->xServicePara.pcPutMediaEndpoint)) != 0 || uHttpStatusCode != 200)
                {
                    printf("Failed to get data endpoint\r\n");
                    res = ERRNO_FAIL;
                }
            }
        }
    }

    if (res == ERRNO_NONE)
    {
        printf("PUT MEDIA endpoint: %s\r\n", pKvs->xServicePara.pcPutMediaEndpoint);
    }

    return res;
}

static void streamFlush(StreamHandle xStreamHandle)
{
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while ((xDataFrameHandle = Kvs_streamPop(xStreamHandle)) != NULL)
    {
        pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
        free(pDataFrameIn->pData);
        Kvs_dataFrameTerminate(xDataFrameHandle);
    }
}

static void streamFlushToNextCluster(StreamHandle xStreamHandle)
{
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;

    while (1)
    {
        xDataFrameHandle = Kvs_streamPeek(xStreamHandle);
        if (xDataFrameHandle == NULL)
        {
            sleepInMs(50);
        }
        else
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            if (pDataFrameIn->xClusterType == MKV_CLUSTER)
            {
                break;
            }
            else
            {
                xDataFrameHandle = Kvs_streamPop(xStreamHandle);
                pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
                free(pDataFrameIn->pData);
                Kvs_dataFrameTerminate(xDataFrameHandle);
            }
        }
    }
}

static int putMediaSendData(Kvs_t *pKvs, int *pxSendCnt)
{
    int res = 0;
    DataFrameHandle xDataFrameHandle = NULL;
    DataFrameIn_t *pDataFrameIn = NULL;
    uint8_t *pData = NULL;
    size_t uDataLen = 0;
    uint8_t *pMkvHeader = NULL;
    size_t uMkvHeaderLen = 0;
    int xSendCnt = 0;

    if (Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_VIDEO)
#if ENABLE_AUDIO_TRACK
           && Kvs_streamAvailOnTrack(pKvs->xStreamHandle, TRACK_AUDIO)
#endif
    )
    {
        if ((xDataFrameHandle = Kvs_streamPop(pKvs->xStreamHandle)) == NULL)
        {
            printf("Failed to get data frame\r\n");
            res = ERRNO_FAIL;
        }
        else if (Kvs_dataFrameGetContent(xDataFrameHandle, &pMkvHeader, &uMkvHeaderLen, &pData, &uDataLen) != 0)
        {
            printf("Failed to get data and mkv header to send\r\n");
            res = ERRNO_FAIL;
        }
        else if (Kvs_putMediaUpdate(pKvs->xPutMediaHandle, pMkvHeader, uMkvHeaderLen, pData, uDataLen) != 0)
        {
            printf("Failed to update\r\n");
            res = ERRNO_FAIL;
        }
        else
        {
            xSendCnt++;
        }

        if (xDataFrameHandle != NULL)
        {
            pDataFrameIn = (DataFrameIn_t *)xDataFrameHandle;
            free(pDataFrameIn->pData);
            Kvs_dataFrameTerminate(xDataFrameHandle);
        }
    }

    if (pxSendCnt != NULL)
    {
        *pxSendCnt = xSendCnt;
    }

    return res;
}

static int putMedia(Kvs_t *pKvs)
{
    int res = 0;
    unsigned int uHttpStatusCode = 0;
    uint8_t *pEbmlSeg = NULL;
    size_t uEbmlSegLen = 0;
	int xSendCnt = 0;

    printf("Try to put media\r\n");
    if (pKvs == NULL)
    {
        printf("Invalid argument: pKvs\r\n");
        res = ERRNO_FAIL;
    }
    else if (Kvs_putMediaStart(&(pKvs->xServicePara), &(pKvs->xPutMediaPara), &uHttpStatusCode, &(pKvs->xPutMediaHandle)) != 0 || uHttpStatusCode != 200)
    {
        printf("Failed to setup PUT MEDIA\r\n");
        res = ERRNO_FAIL;
    }
    else if (Kvs_streamGetMkvEbmlSegHdr(pKvs->xStreamHandle, &pEbmlSeg, &uEbmlSegLen) != 0 ||
             Kvs_putMediaUpdateRaw(pKvs->xPutMediaHandle, pEbmlSeg, uEbmlSegLen) != 0)
    {
        printf("Failed to upadte MKV EBML and segment\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        /* The beginning of a KVS stream has to be a cluster frame. */
        streamFlushToNextCluster(pKvs->xStreamHandle);

        while (1)
        {
            if (putMediaSendData(pKvs, &xSendCnt) != ERRNO_NONE)
            {
                break;
            }
            if (Kvs_putMediaDoWork(pKvs->xPutMediaHandle) != ERRNO_NONE)
            {
                break;
            }
            if (xSendCnt == 0)
            {
                sleepInMs(50);
            }
        }
    }

    printf("Leaving put media\r\n");
    Kvs_putMediaFinish(pKvs->xPutMediaHandle);
    pKvs->xPutMediaHandle = NULL;

    return res;
}

void Kvs_run(Kvs_t *pKvs)
{
    int res = ERRNO_NONE;
    unsigned int uHttpStatusCode = 0;

#if ENABLE_IOT_CREDENTIAL
    IotCredentialToken_t *pToken = NULL;
#endif /* ENABLE_IOT_CREDENTIAL */

    if (kvsInitialize(pKvs) != 0)
    {
        printf("Failed to initialize KVS\r\n");
        res = ERRNO_FAIL;
    }
    else if (initCamera(pKvs) != 0)
    {
        printf("Failed to init camera\r\n");
        res = ERRNO_FAIL;
    }
#if ENABLE_AUDIO_TRACK
    else if (initAudio(pKvs) != 0)
    {
        printf("Failed to init audio\r\n");
        res = ERRNO_FAIL;
    }
#endif /* ENABLE_AUDIO_TRACK */
    else if ((pKvs->xStreamHandle = Kvs_streamCreate(pKvs->pVideoTrackInfo, pKvs->pAudioTrackInfo)) == NULL)
    {
        printf("Failed to create stream\r\n");
        res = ERRNO_FAIL;
    }
    else
    {
        while (1)
        {
#if ENABLE_IOT_CREDENTIAL
            Iot_credentialTerminate(pToken);
            if ((pToken = Iot_getCredential(&(pKvs->xIotCredentialReq))) == NULL)
            {
                printf("Failed to get Iot credential\r\n");
                break;
            }
            else
            {
                pKvs->xServicePara.pcAccessKey = pToken->pAccessKeyId;
                pKvs->xServicePara.pcSecretKey = pToken->pSecretAccessKey;
                pKvs->xServicePara.pcToken = pToken->pSessionToken;
            }
#endif

            if (setupDataEndpoint(pKvs) != ERRNO_NONE)
            {
                printf("Failed to get PUT MEDIA endpoint");
            }
            else if (putMedia(pKvs) != ERRNO_NONE)
            {
                printf("End of PUT MEDIA\r\n");
                break;
            }
            
            Kvs_putMediaFinish(pKvs->xPutMediaHandle);

            sleepInMs(100); /* Wait for retry */
        }
    }
}

void kvs_producer()
{
    Kvs_t xKvs = {0};

    Kvs_run(&xKvs);
}

#endif /* CONFIG_EXAMPLE_KVS_PRODUCER */
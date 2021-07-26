#include "platform_opts.h"

#if CONFIG_EXAMPLE_KVS_PRODUCER

#include "example_kvs_producer.h"
#include "kvs_producer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "wifi_conf.h"

static void amebapro_platform_init(void);

extern char log_buf[100];
extern xSemaphoreHandle log_rx_interrupt_sema;
static void kvs_init_thread( void * param )
{
    vTaskDelay( 1000 / portTICK_PERIOD_MS );

#if 0
    sprintf(log_buf, "ATW0=ssid");
    xSemaphoreGive(log_rx_interrupt_sema);
    vTaskDelay( 1000 / portTICK_PERIOD_MS );

    sprintf(log_buf, "ATW1=pw");
    xSemaphoreGive(log_rx_interrupt_sema);
    vTaskDelay( 1000 / portTICK_PERIOD_MS );

    sprintf(log_buf, "ATWC");
    xSemaphoreGive(log_rx_interrupt_sema);
    vTaskDelay( 1000 / portTICK_PERIOD_MS );
#endif

    // amebapro platform init
    amebapro_platform_init();

    // kvs produccer init
    platformInit();

    kvs_producer();

    vTaskDelete(NULL);
}

static void amebapro_platform_init(void)
{
    /* initialize HW crypto, do this if mbedtls using AES hardware crypto */
    platform_set_malloc_free( (void*(*)( size_t ))calloc, vPortFree);
    
    /* CRYPTO_Init -> Configure mbedtls to use FreeRTOS mutexes -> mbedtls_threading_set_alt(...) */
    CRYPTO_Init();
    
    while( wifi_is_ready_to_transceive( RTW_STA_INTERFACE ) != RTW_SUCCESS )
    {
        vTaskDelay( 200 / portTICK_PERIOD_MS );
    }
    printf( "wifi connected\r\n" );

    sntp_init();
    while( getEpochTimestampInMs() < 100000000ULL )
    {
        vTaskDelay( 200 / portTICK_PERIOD_MS );
        printf("waiting get epoch timer\r\n");
    }
}

void example_kvs_producer( void )
{
    if( xTaskCreate(kvs_init_thread, ((const char*)"kvs_init_thread"), 4096, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
        printf("\n\r%s xTaskCreate(kvs_init_thread) failed", __FUNCTION__);
}

#endif // #ifdef CONFIG_EXAMPLE_KVS_PRODUCER
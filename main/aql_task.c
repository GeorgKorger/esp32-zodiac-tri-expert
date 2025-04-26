#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "aql_task.h"

static const char *TAG = "aql_task";

const char sIdCommand[]     = { PACKET_HEADER, PACKET_DEST_AQUALINK, 0x14, 0x01, 0xD7, PACKET_FOOTER };
char sOutputCommand[] = { PACKET_HEADER, PACKET_DEST_AQUALINK, 0x11, 0x00, 0x00, PACKET_FOOTER };

//Buffer for static Semaphore in AquaVal Instance
StaticSemaphore_t xSemaphoreBuffer;

// Note: Some pins on target chip cannot be assigned for UART communication.
// Please refer to documentation for selected board and target to configure pins using Kconfig.
#define AQUAL_TXD           (CONFIG_AQUAL_UART_TXD)
#define AQUAL_RXD           (CONFIG_AQUAL_UART_RXD)

// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define AQUAL_RTS           (CONFIG_AQUAL_UART_RTS)

// CTS is not used in RS485 Half-Duplex Mode
#define AQUAL_CTS           (UART_PIN_NO_CHANGE)

#define BUF_SIZE                (127)
#define BAUD_RATE               (CONFIG_AQUAL_UART_BAUD_RATE)

// Read packet timeout
#define PACKET_READ_TICS        (100 / portTICK_PERIOD_MS)
#define AQUAL_UART_PORT          (CONFIG_AQUAL_UART_PORT_NUM)

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define AQUAL_READ_TOUT          (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#define AQUAL_TASK_STACK_SIZE    (CONFIG_AQUAL_TASK_STACK_SIZE)
#define AQUAL_TASK_PRIO          (10)

/***************
** RS485 stuff *
****************/


static int aqual_send(const int port, const char* str, uint8_t length)
{
  ESP_LOGI(TAG, "Transmit %u bytes to %d:", length, port);
  printf("[ ");
  for (int i = 0; i < length; i++) {
      printf("0x%.2X ", (uint8_t)str[i]);
  }
  printf("] \n");
//    if (uart_write_bytes(port, str, length) != length) {
//        ESP_LOGE(TAG, "Send data critical failure.");
//        // add your code to handle sending failure here
//        return -1;
//    }
    return 0;
}

// An example of echo test with hardware flow control on UART
static void aqual_task(void *arg)
{
    aquaVal_t * pAquaVal = (aquaVal_t *)arg;
    
    //Prepare Semaphore in AquaVal Instance
    pAquaVal->lock = xSemaphoreCreateBinaryStatic( &xSemaphoreBuffer );
    xSemaphoreGive( pAquaVal->lock );
    
    const int uart_num = AQUAL_UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "Configure UART.");

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    ESP_LOGI(TAG, "UART set pins, mode and install driver.");

    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(uart_num, AQUAL_TXD, AQUAL_RXD, AQUAL_RTS, AQUAL_CTS));

    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, AQUAL_READ_TOUT));

    // Allocate buffers for UART
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);

    ESP_LOGI(TAG, "Start loop.\r");

    while (1) {

      //Write id_command to UART
      aqual_send(uart_num, sIdCommand, (sizeof(sIdCommand)));
      ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
      //Read data from UART
      //int len = uart_read_bytes(uart_num, data, BUF_SIZE, PACKET_READ_TICS);
      char testIdResponse[] = {PACKET_HEADER,'G','e','o','L','i',0xE2,PACKET_FOOTER};
      //char testIdResponse[] = {'y'};
      int len = sizeof(testIdResponse);
      memcpy(data, testIdResponse, len);
      if(len > 0) {
          ESP_LOGI(TAG, "Received %u bytes:", len);
          printf("[ ");
          for (int i = 0; i < len; i++) {
              printf("0x%.2X ", (uint8_t)data[i]);
          }
          printf("] \n");
          if(parseIdResponse(data, len) == 0) {
            printf("ID: %s\n",data);
          }
      } else {
        printf("No Received Bytes\n");
      }
      vTaskDelay( 5000 / portTICK_PERIOD_MS ); //5 sec delay

      //Write SetPower command to UART
      prepareOutputCommand(20, sOutputCommand);
      aqual_send(uart_num, sOutputCommand, (sizeof(sOutputCommand)));
      ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
      //Read data from UART
      len = uart_read_bytes(uart_num, data, BUF_SIZE, PACKET_READ_TICS);
      char testOutputResponse[] = {PACKET_HEADER,2,3,4,5,6,7,75,62,77,45,0x30,PACKET_FOOTER};
      //char testOutputResponse[] = {'x'};
      len = sizeof(testOutputResponse);
      memcpy(data, testOutputResponse, len);

      if(len > 0) {
          ESP_LOGI(TAG, "Received %u bytes:", len);
          printf("[ ");
          for (int i = 0; i < len; i++) {
              printf("0x%.2X ", (uint8_t)data[i]);
          }
          printf("] \n");
          if(parseOutputResponse(data, len, pAquaVal) == 0) {
            float ph_setpoint = -1, ph_current = -1;
            int acl_setpoint = -1, acl_current = -1;
            if( xSemaphoreTake( pAquaVal->lock, ( TickType_t ) 10 ) == pdTRUE ) {
              ph_setpoint  = pAquaVal->ph_setpoint;
              acl_setpoint = pAquaVal->acl_setpoint;
              ph_current   = pAquaVal->ph_current;
              acl_current  = pAquaVal->acl_current;
              xSemaphoreGive( pAquaVal->lock );
            }
            printf("pH setpoint/current: %3.1f/%3.1f\n",ph_setpoint,ph_current);
            ESP_LOGD(TAG, "acl setpoint/current: %3d/%3d\n",acl_setpoint,acl_current);
          }
        } else {
          printf("No Received Bytes\n");
        }
      vTaskDelay( 5000 / portTICK_PERIOD_MS ); //5 sec delay
    }
    vTaskDelete(NULL);
}

void create_aql_task(aquaVal_t * aquaVal, TaskHandle_t * handle) {
    xTaskCreate(aqual_task, "uart_aqual_task", AQUAL_TASK_STACK_SIZE, (void*)aquaVal, AQUAL_TASK_PRIO, handle);
}


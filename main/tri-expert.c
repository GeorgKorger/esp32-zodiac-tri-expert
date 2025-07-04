#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/semphr.h"
//#include "freertos/queue.h"
#include "esp_system.h"
//#include "nvs_flash.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "aql_config.h"
#include "mqtt_stuff.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "tri-expert.h"

static const char *TAG = "tri-expert";

extern aquaVal_t aquaVal;
extern uint8_t power;

const uint8_t sIdCommand[]     = { 0x00, PACKET_HEADER, PACKET_DEST_AQUALINK, 0x14, 0x01, 0xD7, PACKET_FOOTER };
uint8_t sOutputCommand[] = { 0x00, PACKET_HEADER, PACKET_DEST_AQUALINK, 0x11, 0x00, 0x00, PACKET_FOOTER };

#define AQUAL_UART_PORT          (1)
// CTS is not used in RS485 Half-Duplex Mode
#define AQUAL_CTS           (UART_PIN_NO_CHANGE)
#define BUF_SIZE  (127)
// Read packet timeout
#define PACKET_READ_TICS        (500 / portTICK_PERIOD_MS)

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define AQUAL_READ_TOUT          (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

#define AQUAL_TASK_STACK_SIZE    (3072)
#define AQUAL_TASK_PRIO          (10)

/***************
** RS485 stuff *
****************/


static int aqual_send(const int port, const uint8_t* str, uint8_t length)
{
  ESP_LOGI(TAG, "Transmit %u bytes to %d:", length, port);
  for (int i = 0; i < length; i++) {
      ESP_LOGD(TAG,"0x%.2X ", str[i]);
  }
  if (uart_write_bytes(port, str, length) != length) {
      ESP_LOGE(TAG, "Send data critical failure.");
      // add your code to handle sending failure here
      return -1;
	  ESP_ERROR_CHECK(uart_wait_tx_done(AQUAL_UART_PORT, 10));
  }
return 0;
}

void triExpertInit(void)
{
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
  ESP_ERROR_CHECK(uart_driver_install(AQUAL_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));

  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(AQUAL_UART_PORT, &uart_config));

  ESP_LOGI(TAG, "UART set pins, mode and install driver.");

  // Set UART pins as per KConfig settings
  ESP_ERROR_CHECK(uart_set_pin(AQUAL_UART_PORT, AQUAL_TXD, AQUAL_RXD, AQUAL_RTS, AQUAL_CTS));

  // Set RS485 half duplex mode
  ESP_ERROR_CHECK(uart_set_mode(AQUAL_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

  // Set read timeout of UART TOUT feature
  ESP_ERROR_CHECK(uart_set_rx_timeout(AQUAL_UART_PORT, AQUAL_READ_TOUT));

  // Configure BLUE_LED
  gpio_set_intr_type(BLUE_LED, GPIO_INTR_DISABLE);
  gpio_intr_disable(BLUE_LED);
  gpio_set_level(BLUE_LED,0);
  gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);
}

int setPowerReadVal() {
  int len, err = -1;
  // Allocate buffers for UART
  uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  //Write SetPower command to UART
  prepareOutputCommand();
  aquaVal.retries = 0;
  uint8_t old_conn = aquaVal.connected;
  while( aquaVal.retries < 6 ) {
    aqual_send(AQUAL_UART_PORT, sOutputCommand, (sizeof(sOutputCommand)));

    //Read data from UART
    len = uart_read_bytes(AQUAL_UART_PORT, data, BUF_SIZE, PACKET_READ_TICS);
  #ifdef AQUAL_WITHOUT_UART
  char testOutputResponse[] = {0x00,PACKET_HEADER,2,3,4,5,6,7,75,62,77,45,0x30,PACKET_FOOTER};
    //char testOutputResponse[] = {'x'};
    len = sizeof(testOutputResponse);
    memcpy(data, testOutputResponse, len);
  #endif
      err = parseOutputResponse(data, len);
      if(err) {
        ESP_LOGE(TAG, "Error parse response in setPowerReadVal, %u. retry", ++aquaVal.retries);
        vTaskDelay( 3 * 1000 / portTICK_PERIOD_MS ); //delay between attempts
      }
      else {
        aquaVal.connected = 1;
		    esp_event_post(AQL_EVENTS, AQL_EVENT_PUBLISH_AQLVAL, NULL, 0, 100 / portTICK_PERIOD_MS);
		    //flash blue led
		    gpio_set_level(BLUE_LED, 1);
		    vTaskDelay( BLUE_LED_DELAY / portTICK_PERIOD_MS ); //flash time
		    gpio_set_level(BLUE_LED, 0);
		    goto goodEnd;
      }
    }
    aquaVal.connected = 0;
goodEnd:
    free(data);
    if( aquaVal.connected != old_conn ) esp_event_post(AQL_EVENTS, AQL_EVENT_PUBLISH_CONNECTED, NULL, 0, 100 / portTICK_PERIOD_MS);
    return err;
}

int readID(void) {
  int len, err = -1;
  // Allocate buffers for UART
  uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  //Write ID command to UART
  aqual_send(AQUAL_UART_PORT, sIdCommand, (sizeof(sIdCommand)));

  //Read data from UART
  len = uart_read_bytes(AQUAL_UART_PORT, data, BUF_SIZE, PACKET_READ_TICS);
#ifdef AQUAL_WITHOUT_UART
  char testIdResponse[] = {0x00, 0x10, 0x02, 0x00, 0x03, 0x01, 0x5A, 0x4F, 0x44, 0x49, 0x41, 0x43, 0x20, 0x54, 0x52, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x10, 0x03};
  len = sizeof(testIdResponse);
  memcpy(data, testIdResponse, len);
#endif
    err = parseIdResponse(data, len);
    if( err<0 ) {
      ESP_LOGE(TAG, "Error parse response in readID");
    }
    else {
      //flash blue led
      gpio_set_level(BLUE_LED, 1);
      vTaskDelay( BLUE_LED_DELAY / portTICK_PERIOD_MS ); //flash time
      gpio_set_level(BLUE_LED, 0);
    }
    free(data);
    return err;
}

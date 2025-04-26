#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
//#include "nvs_flash.h"
//#include "driver/uart.h"
//#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "aql_task.h"

static const char *TAG = "aql_main";

static aquaVal_t aquaVal;

void app_main(void)
{
  ESP_LOGI(TAG,"Main App start");
  TaskHandle_t aql_tast_handle = NULL;
  create_aql_task(&aquaVal, &aql_tast_handle);
  configASSERT( aql_tast_handle );
  while(true) {
    vTaskDelay( 60000 / portTICK_PERIOD_MS ); //1 min delay
    ESP_LOGD(TAG, "still running...");
  }
    
  
}


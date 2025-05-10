#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "freertos/semphr.h"
#include "esp_system.h"
//#include "nvs_flash.h"
//#include "driver/uart.h"
//#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "aql_config.h"
#include "tri-expert.h"
#include "flash.h"

static const char *TAG = "aql_main";

static aquaVal_t aquaVal;
static int8_t power;

void app_main(void)
{
  //esp_event_loop_create_default();    
  power = restorePowerFromFlash();
  if(power >= 0) {
    ESP_LOGI(TAG,"Power restored from flash: %d",power);
  } 
  else {
    ESP_LOGW(TAG,"Power restore from flash failed");
    power = DEFAULT_POWER;
    storePowerToFlash(power);
  }
  
  ESP_LOGI(TAG, "Inital delay %d s",INIT_DELAY);
  vTaskDelay( INIT_DELAY * 1000 / portTICK_PERIOD_MS ); //delay on startup

  triExpertInit();
  
  readID();
  while(true) {
    vTaskDelay( TRI_EXPERT_LOOP_DELAY * 1000 / portTICK_PERIOD_MS ); //delay between tri-expert commands
    setPowerReadVal(power, &aquaVal);
  }
}


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

#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "aql_config.h"
#include "tri-expert.h"
#include "flash.h"
#include "mqtt_stuff.h"

static const char *TAG = "aql_main";

static aquaVal_t aquaVal;
static int8_t power;

void app_main(void)
{
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("mqtt_client", ESP_LOG_INFO);
	esp_log_level_set("mqtt", ESP_LOG_INFO);
	esp_log_level_set("transport_base", ESP_LOG_INFO);
	esp_log_level_set("esp-tls", ESP_LOG_INFO);
	esp_log_level_set("transport", ESP_LOG_INFO);
	esp_log_level_set("outbox", ESP_LOG_INFO);


  power = restorePowerFromFlash();
  if(power >= 0) {
    ESP_LOGI(TAG,"Power restored from flash: %d",power);
  } 
  else {
    ESP_LOGW(TAG,"Power restore from flash failed");
    power = DEFAULT_POWER;
    storePowerToFlash(power);
  }
  
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());
  mqtt_app_start();

  ESP_LOGI(TAG, "Inital delay %d s",INIT_DELAY);
  vTaskDelay( INIT_DELAY * 1000 / portTICK_PERIOD_MS ); //delay on startup

  triExpertInit();
  
  readID();
  while(true) {
    vTaskDelay( TRI_EXPERT_LOOP_DELAY * 1000 / portTICK_PERIOD_MS ); //delay between tri-expert commands
    setPowerReadVal(power, &aquaVal);
  }
}


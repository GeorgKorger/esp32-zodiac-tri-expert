#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_log_level.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "freertos/semphr.h"
#include "esp_system.h"
//#include "nvs_flash.h"
//#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_connect.h"
#include "aql_config.h"
#include "tri-expert.h"
#include "flash.h"
#include "mqtt_stuff.h"
#include "my_ota.h"

static const char *TAG = "aql_main";

//global Variables
aquaVal_t aquaVal;
uint8_t power;
QueueHandle_t powerQueue;

void app_main(void)
{
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	//esp_log_level_set("mqtt", ESP_LOG_DEBUG);

/*	esp_log_level_set("mqtt_client", ESP_LOG_INFO);
	esp_log_level_set("mqtt", ESP_LOG_INFO);
	esp_log_level_set("transport_base", ESP_LOG_INFO);
	esp_log_level_set("esp-tls", ESP_LOG_INFO);
	esp_log_level_set("transport", ESP_LOG_INFO);
	esp_log_level_set("outbox", ESP_LOG_INFO);
*/
  esp_err_t post_power_trigger = ESP_OK; // if not ESP_OK will trigger an AQL_EVENT_POWER_SET which at least stores power to flash and publishes it
  power = restorePowerFromFlash();
  if( power <= 100) {
    ESP_LOGI(TAG,"Power restored from flash: %d",power);
  } 
  else {
    power = DEFAULT_POWER;
    ESP_LOGW(TAG,"restore from flash failed, set default power: %d",power);
    storePowerToFlash(power);
  }

  powerQueue = xQueueCreate(1, sizeof(uint8_t));

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // connect wifi without waiting for IP Address
  ESP_ERROR_CHECK(wifi_connect(false));
  mqtt_app_start();

  ESP_LOGI(TAG, "Inital delay %d s",INIT_DELAY);
  vTaskDelay( INIT_DELAY * 1000 / portTICK_PERIOD_MS ); //delay on startup

  triExpertInit();
  
  readID();

  // MAIN LOOP
  aquaVal.connected = 0;
  uint8_t pow;
  while(true) {
    vTaskDelay( 10 * 1000 / portTICK_PERIOD_MS ); //minimal 10s delay between tri-expert commands
    // delay until new power val is available or at least after (TRI_EXPERT_LOOP_DELAY - 10sec)
    if( pdTRUE == xQueueReceive( powerQueue, &pow, ( (TRI_EXPERT_LOOP_DELAY - 10) * 1000 / portTICK_PERIOD_MS ))) {
      if( pow > 101 ) { // special events - reboot or OTA
        break; // exit main loop
      }
      else {
        if( pow != power) {
          ESP_LOGI(TAG, "Got new power val: %u",pow);
          power = pow;
          post_power_trigger = ESP_FAIL; // triggers event posting
        }
      }
    }

    if( post_power_trigger != ESP_OK ) post_power_trigger = esp_event_post(AQL_EVENTS, AQL_EVENT_POWER_SET, NULL, 0, 100 / portTICK_PERIOD_MS);

    setPowerReadVal();
  }
  // Destroy anything
  mqtt_app_stopp();
  wifi_disconnect();
  nvs_flash_deinit();

  if( pow == POWERQUEUE_OTA ) start_ota();
  
  esp_restart(); // For all other reasons in the moment  
  
}



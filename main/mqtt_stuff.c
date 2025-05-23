#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "aql_config.h"
#include "mqtt_stuff.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client;

static char printBuf[4];
#ifdef AQUAL_WITHOUT_UART
	static char topicBuf[] = "pool/dummy/---------------";
	#undef MQTT_CLIENT_ID
	#define MQTT_CLIENT_ID "PumpBoothDummy"
#else
	static char topicBuf[] = "pool/swg/---------------";
#endif
char* topic(char* end, size_t end_len, size_t* len) {
  memcpy(topicBuf+(sizeof(topicBuf)-16), end, end_len);
  *len = sizeof(topicBuf)-17 + end_len;
  return topicBuf;  
}

void mqtt_publish(aquaVal_t * aquaVal) {
	// MQTT_SKIP_PUBLISH_IF_DISCONNECTED should be enabled in menuconfig!
	size_t len;
	sprintf(printBuf, "%3.1f", aquaVal->ph_setpoint);
	esp_mqtt_client_publish(client, topic("ph_setpoint",sizeof("ph_setpoint"),&len), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3.1f", aquaVal->ph_current);
	esp_mqtt_client_publish(client, topic("ph_current",sizeof("ph_current"),&len), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3d", aquaVal->acl_setpoint);
	esp_mqtt_client_publish(client, topic("acl_setpoint",sizeof("acl_setpoint"),&len), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3d", aquaVal->acl_current);
	esp_mqtt_client_publish(client, topic("acl_current",sizeof("acl_current"),&len), printBuf, 0, 1, 1);
}

bool onlyDigits( char* s, size_t len ) {
  char* end = s + len;
  do {
    if( !isdigit( (uint8_t)*s )) return false; 
  } while( ++s < end );
  return true;
}
	
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    QueueHandle_t powerQueue = *( (QueueHandle_t*)handler_args);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    size_t len;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, topic("power",sizeof("power"),&len), 1);
        msg_id = esp_mqtt_client_subscribe(client, topic("loglevel",sizeof("loglevel"),&len), 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
    		if(( event->data_len > 0 ) && ( event->data_len <= 3 ) && onlyDigits( event->data, event->data_len )) {
    		  topic("power", sizeof("power"), &len);
    		  if(( event->topic_len == len ) && ( 0 == memcmp( event->topic, topicBuf, len ))) {
    		    memcpy(printBuf, event->data, event->data_len);
    		    printBuf[event->data_len] = '\0';
    		    int pow = atoi(printBuf);
    		    if( pow > 101 ) {
    		      ESP_LOGW(TAG, "Power > 101 was set, will be set to 101");
    		      pow = 101;
    		      esp_mqtt_client_publish(client, topicBuf, "101", 3, 1, 1);
    		    }
    		    xQueueOverwrite(powerQueue, &pow);
    		  }
    		}
    		else {
    		  topic("loglevel", sizeof("loglevel"), &len);
    		  if(( event->topic_len == len ) && ( 0 == memcmp( event->topic, topicBuf, len ))) {
    		    ESP_LOGD(TAG, "received loglevel, data_len: %d", event->data_len);
    		    if(( event->data_len == sizeof("info")-1 ) && ( 0 == memcmp( event->data, "info", event->data_len ))) {
    		      ESP_LOGI(TAG, "set loglevel INFO");
    		      esp_log_level_set("*", ESP_LOG_INFO);
    		    }
    		    else if(( event->data_len == sizeof("debug1")-1 ) && ( 0 == memcmp( event->data, "debug1", event->data_len ))) {
    		      ESP_LOGI(TAG, "set loglevel DEBUG (without wifi)");
    		      esp_log_level_set("*", ESP_LOG_DEBUG);
    		      esp_log_level_set("wifi", ESP_LOG_INFO);
    		    }
    		    else if(( event->data_len == sizeof("debug2")-1 ) && ( 0 == memcmp( event->data, "debug2", event->data_len ))) {
    		      ESP_LOGI(TAG, "set loglevel DEBUG");
    		      esp_log_level_set("*", ESP_LOG_DEBUG);
    		      esp_log_level_set("wifi", ESP_LOG_DEBUG);
    		    }
    		  }
    		}
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(QueueHandle_t *pPowerQueue)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, pPowerQueue);
    esp_mqtt_client_start(client);
}

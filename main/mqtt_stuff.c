#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "aql_config.h"
#include "mqtt_stuff.h"

#ifdef AQUAL_WITHOUT_UART
	char topicBuf[] = "pool/dummy/---------------";
#else
	char topicBuf[] = "pool/swg/---------------";
#endif
char* topic(char* end) {
  strcpy(topicBuf+(sizeof(topicBuf)-16), end);
  return topicBuf;  
}

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client;

void mqtt_publish(aquaVal_t * aquaVal) {
	// MQTT_SKIP_PUBLISH_IF_DISCONNECTED should be enabled in menuconfig!
	char printBuf[4];
	sprintf(printBuf, "%3.1f", aquaVal->ph_setpoint);
	esp_mqtt_client_publish(client, topic("ph_setpoint"), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3.1f", aquaVal->ph_current);
	esp_mqtt_client_publish(client, topic("ph_current"), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3d", aquaVal->acl_setpoint);
	esp_mqtt_client_publish(client, topic("acl_setpoint"), printBuf, 0, 1, 1);
	sprintf(printBuf, "%3d", aquaVal->acl_current);
	esp_mqtt_client_publish(client, topic("acl_current"), printBuf, 0, 1, 1);
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
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, topic("power"), 1);
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
    		if( strcmp( event->topic, topic("power") ) == 0 ) {
    		    int pow = atoi(event->data);
    		    xQueueOverwrite(powerQueue, &pow);
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
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, pPowerQueue);
    esp_mqtt_client_start(client);
}

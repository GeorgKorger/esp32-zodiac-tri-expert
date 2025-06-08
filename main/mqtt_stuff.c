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
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_app_desc.h"

#include "esp_log.h"

#include "aql_config.h"
#include "flash.h"
#include "mqtt_stuff.h"

extern aquaVal_t aquaVal;
extern uint8_t power;
extern QueueHandle_t powerQueue;
bool powerQueue_closed = false; // avoids overwriting special values in the queue

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client;

/*************************
** Topic and data stuff **
**************************/

#ifdef AQUAL_WITHOUT_UART
	static char topicBuf[] = "pool/dummy/---------------";
	#define BASE_TOPIC_SIZE sizeof("pool/dummy/")
	#undef MQTT_CLIENT_ID
	#define MQTT_CLIENT_ID "PumpBoothDummy"
#else
	static char topicBuf[] = "pool/swg/---------------";
	#define BASE_TOPIC_SIZE sizeof("pool/swg/")
#endif
#define BASE_TOPIC_LEN (BASE_TOPIC_SIZE - 1)

char* fTopic(char* subTopic, size_t subTopic_size, size_t* len) {
  ESP_LOGV(TAG, "subTopic: \"%s\"; subTopic_size: %lu\n",subTopic,subTopic_size);
  memcpy(topicBuf+BASE_TOPIC_LEN, subTopic, subTopic_size);
  *len = BASE_TOPIC_LEN + subTopic_size - 1;
  return topicBuf;  
}

/**
 *  Macro to put a subtopic in the topicBuf - for const subtopic
 *
 * @param st (in)char* the subtopic.
 * @param l (out)size_t* the len of the complete topic.
 * @return char* the topicBuf.
 */
#define TOPIC(st,l) fTopic(st, sizeof(st), l)

/**
 *  Macro to put a subtopic in the topicBuf - for subtopic in char* Variable
 *
 * @param st (in)char* the subtopic.
 * @param sts size_t size of subtopic.
 * @param l (out)size_t* the len of the complete topic.
 * @return char* the topicBuf.
 */
#define TOPIC2(st,sts,l) fTopic(st, sts, l)

bool check_subTopic(char* subTopic, size_t subTopic_len, char* data, size_t data_len) {
  size_t dataSubtopic_len = data_len - BASE_TOPIC_LEN;
  ESP_LOGV(TAG, "subTopic_len: %lu; dataSubtopic_len: %lu\n", subTopic_len, dataSubtopic_len); 
  if( subTopic_len != dataSubtopic_len ) return false;
  ESP_LOGV(TAG, "data: \"%s\", subTopic in data: \"%s\"\n",data,data+BASE_TOPIC_LEN);
  return ( ( 0 == memcmp( subTopic, data+BASE_TOPIC_LEN, subTopic_len )) );
}

/**
 *  Macro to check the subtopic.
 *
 * @param st (in)char* the subtopic.
 * @param l (out)size_t* the len of the complete topic.
 * @return char* the topicBuf.
 */
#define IS_SUBTOPIC(st) check_subTopic( st, sizeof(st)-1, event->topic, event->topic_len )

/**
 *  Macro to check the data.
 *
 * @param v (in)char* the value to check against.
 * @param d (in)char* the data buffer.
 * @param d size_t the length of data to compare.
 * @return char* the topicBuf.
 */
#define IS_DATA(v) ( event->data_len == sizeof(v)-1 ) && ( 0 == memcmp( event->data, v, sizeof(v)-1 ))

/**********************
** Publish Functions **
***********************/

int doPublish(char *subTopic, size_t subTopicSize, const char* buf) {
	size_t len;
	return esp_mqtt_client_publish(client, TOPIC2(subTopic, subTopicSize, &len), buf, 0, 1, 1);
}

int doPublish_u16(char *subTopic, size_t subTopicSize, uint16_t val) {
	char printBuf[6];
	sprintf(printBuf, "%u", val);
	ESP_LOGD(TAG, "Val: %u, prntBuf: %s",val,printBuf);
	return doPublish(subTopic, subTopicSize, printBuf);
}

void doPublish_f(char *subTopic, size_t subTopicSize, float val) {
	char printBuf[6];
	sprintf(printBuf, "%3.1f", val);
	ESP_LOGD(TAG, "Val: %f, prntBuf: %s",val,printBuf);
	doPublish(subTopic, subTopicSize, printBuf);
}

void doPublish_h(char *subTopic, size_t subTopicSize, uint8_t *val, size_t valLen) {
	char* printBuf = (char*) malloc(( 3 * valLen * sizeof(char) ) + 1 );
	if( printBuf != NULL ) {
  	char* c = printBuf;
	  for( int i = 0; i < valLen; i++ ) {
	    sprintf(c, "%02X.", val[i]);
	    c += 3;
	  }
	  *(--c) = '\0'; // remove last dot
	ESP_LOGD(TAG, "\"%s\"", printBuf);
	doPublish(subTopic, subTopicSize, printBuf);
	free(printBuf);
  }
}


/*********************
** Helper Functions **
**********************/

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

static void set_powerQueue(uint8_t val) {
  if( !powerQueue_closed ) xQueueOverwrite(powerQueue, &val);
}

static void set_and_close_powerQueue(uint8_t val) {
  set_powerQueue(val);
  powerQueue_closed = true;
}

/****************
** Boost Timer **
*****************/
struct boostTimer_s {
  bool started;
  bool running;
  uint16_t retries;
  TimerHandle_t handle;
} boostTimer;

void cbBoostTimer( TimerHandle_t t ) {
  ESP_LOGD(TAG, "Boost stopped by timer handle");
  uint8_t pow = restorePowerFromFlash();
  set_powerQueue(pow);
  stopBoostTimer();
}

bool checkBoostTimer() { // returns true if boostTimer is not started after MAX_BOOST_RETRIES
  if( boostTimer.started ) {
    if( !boostTimer.running ) {
      boostTimer.running = ( pdPASS == xTimerStart(boostTimer.handle, 10)) ? true : false ;
      if( !boostTimer.running ) {
        if( ++boostTimer.retries == MAX_BOOST_TIMER_RETRIES ) { //something must be really wrong....
          return true;
        }
      }
      else {
        ESP_LOGI(TAG, "BoostTimer started after %u retries", boostTimer.retries);
      }
    }
  }
  else {
    if( boostTimer.running ) {
      boostTimer.running = ( pdPASS == xTimerDelete(boostTimer.handle, 10)) ? false : true ;
      if( !boostTimer.running ) ESP_LOGI(TAG, "BoostTimer stopped and deleted");
    }
  }
  return false;
}

void startBoostTimer() {
  boostTimer.handle = xTimerCreate( "boost", BOOST_DURATION*60*1000/portTICK_PERIOD_MS, pdFALSE, NULL, cbBoostTimer );
  boostTimer.retries = 0;
  boostTimer.started = true;
  checkBoostTimer();
}

void stopBoostTimer() {
  boostTimer.started = false;
  checkBoostTimer();
}

/***************
** AQL Events **
****************/

ESP_EVENT_DEFINE_BASE(AQL_EVENTS);

// handler for my AquaLink events
static void aql_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    ESP_LOGD(TAG, "AquaLink event, id=%" PRIi32 "", event_id);
    switch (event_id) {
      case AQL_EVENT_POWER_SET:
        ESP_LOGD(TAG, "AquaLink power event");
        if( power == 101 ) { //temporay, create Timer to stop, dont store to flash
          startBoostTimer();
        }
        else {
          stopBoostTimer();
          storePowerToFlash(power);
        }
        doPublish_u16("power", sizeof("power"), power);
        break;
      case AQL_EVENT_PUBLISH_AQLVAL:
	      // MQTT_SKIP_PUBLISH_IF_DISCONNECTED should be enabled in menuconfig!
        doPublish_f ("ph_setpoint", sizeof("ph_setpoint"),  aquaVal.ph_setpoint);
        doPublish_f ("ph_current",  sizeof("ph_current"),   aquaVal.ph_current);
        doPublish_u16("acl_setpoint",sizeof("acl_setpoint"), aquaVal.acl_setpoint);
        doPublish_u16("acl_current", sizeof("acl_current"),  aquaVal.acl_current);
        doPublish_h("extra_bytes", sizeof("extra_bytes"),  aquaVal.extra_bytes, 6);
        doPublish_u16("retries", sizeof("retries"),  aquaVal.retries);
        break;
      case AQL_EVENT_PUBLISH_CONNECTED:
        doPublish_u16("connected", sizeof("connected"),  aquaVal.connected);
        break;

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
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    static int powerMsg = -1;
    static int rebootMsg = -1;
    size_t len;
    char printBuf[5];
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        // clear the reboot reason, so that we dont end in an endless loop....
        rebootMsg = esp_mqtt_client_publish(client, TOPIC("reboot", &len), "", 0, 1, 1);
        if( rebootMsg < 1) {
          ESP_LOGW(TAG,"reboot published unsuccessful, subscribe now");
          esp_mqtt_client_subscribe(client, TOPIC("reboot", &len), 1); // subscribe anyway, if publish errors
        }
        else ESP_LOGD(TAG,"reboot published successful, id=%d",powerMsg);

        powerMsg = doPublish_u16("power", sizeof("power"), power); // publish "our" power value before subscribe
        if( powerMsg < 1) {
          ESP_LOGW(TAG,"power published unsuccessful, subscribe now");
          esp_mqtt_client_subscribe(client, TOPIC("power", &len), 1); // subscribe anyway, if publish errors
        }
        else ESP_LOGD(TAG,"power published successful, id=%d",powerMsg);

        esp_mqtt_client_subscribe(client, TOPIC("loglevel", &len), 1);
        ESP_LOGI(TAG, "sent subscribe");
        
        // publish App Version
        const esp_app_desc_t* appDesc = esp_app_get_description();
        doPublish( "appVersion", sizeof("appVersion"), appDesc->version );

		    break;

    case MQTT_EVENT_DISCONNECTED:
        esp_mqtt_client_unsubscribe(client, TOPIC("power", &len)); // unsubscribe power when disconnected, to avoid wrong values later
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
        if( event->msg_id == powerMsg ) {
          ESP_LOGI(TAG,"subscribed to power later");
          esp_mqtt_client_subscribe(client, TOPIC("power", &len), 1); // subscribe after power value was published
          powerMsg = -1;
        }

        else if( event->msg_id == rebootMsg ) {
          ESP_LOGI(TAG,"subscribed to reboot later");
          esp_mqtt_client_subscribe(client, TOPIC("reboot", &len), 1); // subscribe after reboot was cleared
          rebootMsg = -1;
        }

        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
  		  if( IS_SUBTOPIC( "power" )) {
      		if(( event->data_len > 0 ) && ( event->data_len <= 3 ) && onlyDigits( event->data, event->data_len )) {
    		    memcpy(printBuf, event->data, event->data_len);
    		    printBuf[event->data_len] = '\0';
    		    uint8_t pow = atoi(printBuf);
    		    if( pow > 101 ) {
    		      ESP_LOGW(TAG, "Power > 101 was set, will be set to 101");
    		      pow = 101;
    		    }
    		    set_powerQueue(pow);
    		  }
    		  else {
    		    ESP_LOGW(TAG, "mqtt data for power malformed");
    		    doPublish_u16("power", sizeof("power"), power);
    		  }
    		}
    		else if( IS_SUBTOPIC( "loglevel" )) {
		      ESP_LOGD(TAG, "received loglevel, data_len: %d", event->data_len);
  		    if( IS_DATA( "info" )) {
  		      ESP_LOGI(TAG, "set loglevel INFO");
  		      esp_log_level_set("*", ESP_LOG_INFO);
  		    }
  		    else if( IS_DATA( "debug1" )) {
  		      ESP_LOGI(TAG, "set loglevel DEBUG (without wifi)");
  		      esp_log_level_set("*", ESP_LOG_DEBUG);
  		      esp_log_level_set("wifi", ESP_LOG_INFO);
  		    }
  		    else if( IS_DATA( "debug2" )) {
  		      ESP_LOGI(TAG, "set loglevel DEBUG");
  		      esp_log_level_set("*", ESP_LOG_DEBUG);
  		      esp_log_level_set("wifi", ESP_LOG_DEBUG);
  		    }
    		  else ESP_LOGW(TAG, "mqtt data for loglevel malformed");
  		  }
    		else if( IS_SUBTOPIC( "reboot" )) {
    		  if( event->data_len == 0 ) ESP_LOGD(TAG, "received reboot without data, ignoring");
		      else {
		        ESP_LOGD(TAG, "received reboot, data_len: %d", event->data_len);
    		    if( IS_DATA( "ota" )) {
    		      ESP_LOGD(TAG, "reboot for ota");
    		      uint8_t pow = POWERQUEUE_OTA;
    		      set_and_close_powerQueue(pow); // trigger ota over powerQueue
    		    }
    		    else if( IS_DATA( "boot" )) {
    		      ESP_LOGD(TAG, "reboot only");
    		      uint8_t pow = POWERQUEUE_REBOOT;
    		      set_and_close_powerQueue(pow); // trigger reboot over powerQueue
    		    }
      		  else ESP_LOGW(TAG, "mqtt data for reboot malformed");
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

void mqtt_app_start()
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    esp_event_handler_register(AQL_EVENTS, ESP_EVENT_ANY_ID, aql_event_handler, NULL);
    
  boostTimer.started = false;
  boostTimer.running = false;
}

void mqtt_app_stopp()
{
  size_t len;
  // clear the reboot reason, so that we dont end in an endless loop....
  esp_mqtt_client_publish(client, TOPIC("reboot", &len), "", 0, 1, 1);
  esp_mqtt_client_disconnect(client);
  esp_event_handler_unregister(AQL_EVENTS, ESP_EVENT_ANY_ID, aql_event_handler);
  esp_mqtt_client_unregister_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler);
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
  
  stopBoostTimer();
}


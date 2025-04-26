#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "aql_prot.h"

static const char *TAG = "aql_prot";

// Checksum is calculated as sum of all previous bytes mod 256.
uint8_t calc_checksum(uint8_t* dataPtr, size_t len) {
  uint16_t sum = 0;
  for(uint8_t i=0;i<len;++i) {
    sum += *(dataPtr++);
  }
  return (uint8_t)(sum % 256);
}


/*######################################################################
  # Generic (command/response) packet
  ######################################################################*/

const uint8_t packetHeader[] = { PACKET_HEADER };
const uint8_t packetFooter[] = { PACKET_FOOTER };

/*######################################################################
  # Responses
  ######################################################################*/

int aquaLinkResponse(uint8_t *response, int len){
  ESP_LOGD(TAG, "Parsing response");
  // Check common errors
  if(len < 5) {
      ESP_LOGE(TAG, "Response too short!");
      return(-21);
  }
  if(memcmp(response, packetHeader, sizeof(packetHeader)) != 0) {
      ESP_LOGE(TAG, "Reponse header malformed!");
      return(-22);
  }
  if(memcmp(response+len-sizeof(packetFooter), packetFooter, sizeof(packetFooter)) != 0) {
      ESP_LOGE(TAG, "Reponse footer malformed!");
      return(-23);
  }
  if(response[len-3] != calc_checksum(response, len-3)) {
      ESP_LOGE(TAG, "Reponse checksum malformed!");
      return(-24);
  }
  return(0);
}

int parseIdResponse(uint8_t *response, int len) {
  int err = aquaLinkResponse(response, len);
  if( !err ) {
    response[len-3] = '\0';
    response += 3;
  }
  return err;
}

int parseOutputResponse(uint8_t *response, int len, aquaVal_t* val) {
  if(len < 15) {
    ESP_LOGE(TAG, "SetOutputResponse to small");
    return(-31);
  }
  int err = aquaLinkResponse(response, len);
  if( !err ) {
    float ph_setpoint  = (float)response[8] / 10;
    int   acl_setpoint = response[9] * 10;
    float ph_current   = (float)response[10] / 10;
    int   acl_current  = response[11] * 10;
    if( xSemaphoreTake( val->lock, ( TickType_t ) 10 ) == pdTRUE )
    {
      val->ph_setpoint  = ph_setpoint;
      val->acl_setpoint = acl_setpoint;
      val->ph_current   = ph_current;
      val->acl_current  = acl_current;
      xSemaphoreGive( val->lock );
    }
    ESP_LOGD(TAG, "pH setpoint/current: %3.1f/%3.1f",val->ph_setpoint,val->ph_current);
    ESP_LOGD(TAG, "acl setpoint/current: %3d/%3d",val->acl_setpoint,val->acl_current);
  }
  return(err);
}

/*######################################################################
  # Commands
  ######################################################################*/

int prepareOutputCommand(uint8_t output_percent, char* cmd) {
  if(output_percent >= 101) {
    ESP_LOGE(TAG, "Output percent not in range!"); // 101 for Boost mode.
    return(-15);
  }
  cmd[4] = (char)output_percent;
  cmd[5] = (char)calc_checksum((uint8_t*)cmd, LEN_POWER_COMMAND-2);
  return 0;
}


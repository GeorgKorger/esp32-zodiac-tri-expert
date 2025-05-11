#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
//#include "freertos/semphr.h"

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

int aquaLinkResponse(uint8_t **response, int len){
  uint8_t* afterHeader;
  uint8_t* checkSum;
  int8_t i, newLen;
  ESP_LOGI(TAG, "Received %u bytes:", len);
  for (int i = 0; i < len; i++) {
      ESP_LOGD(TAG, "0x%.2X", (uint8_t)*(*response+i));
  }
  ESP_LOGD(TAG, "Parsing response");
  // Check common errors
  if(len < 5) {
      ESP_LOGE(TAG, "Response too short!");
      return(-21);
  }
  // search response header
  for( i=0; i<=len-sizeof(packetHeader);) {
	if( memcmp((*response + i), packetHeader, sizeof(packetHeader)) == 0) break;
	++i;
  }
  if( i <= len-sizeof(packetHeader) ) { // header found
    *response += i;
	newLen = len - i;
    afterHeader = *response + sizeof(packetHeader);
  }
  else {
      ESP_LOGE(TAG, "Reponse header malformed!");
      return(-22);
  }
  // search response footer
  for( i=newLen-sizeof(packetFooter); i>=(sizeof(packetHeader));) {
    if(memcmp(*response + i, packetFooter, sizeof(packetFooter)) == 0) break;
	--i;
  }
  if( i >= (sizeof(packetHeader)) ) { // footer found
    checkSum = *response + i - 1;
  } 
  else {
      ESP_LOGE(TAG, "Reponse footer malformed!");
      return(-23);
  }
  if( *checkSum != calc_checksum(*response, newLen-3)) {
      ESP_LOGE(TAG, "Reponse checksum malformed!");
      return(-24);
  }
  *response = afterHeader; // new start of Response Address
  return( checkSum - afterHeader ); // new length
}

int parseIdResponse(uint8_t *response, int len) {
  int newLen = aquaLinkResponse(&response, len) - 1;
  int8_t i,k=0;
  if( newLen >= 0 ) {
    for(i=0; i<(newLen); i++) {
      if( (*(response+i)>=0x20) && (*(response+i)<=0x7E) ) {  // search for printable chars
		*(response+k++) = *(response+i);
	  }
    }
    *(response+k) = '\0';
    ESP_LOGD(TAG,"Id: %s",response);
  }
  return newLen;
}

int parseOutputResponse(uint8_t *response, int len, aquaVal_t* val) {
  if(len < 15) {
    ESP_LOGE(TAG, "SetOutputResponse to small");
    return(-31);
  }
  int newLen = aquaLinkResponse(&response, len);
  if( newLen>=10 ) {
    val->ph_setpoint  = (float)response[6] / 10;
    val->acl_setpoint = response[7] * 10;
    val->ph_current   = (float)response[8] / 10;
    val->acl_current  = response[9] * 10;
    ESP_LOGD(TAG, "pH setpoint/current: %3.1f/%3.1f",val->ph_setpoint,val->ph_current);
    ESP_LOGD(TAG, "acl setpoint/current: %3d/%3d",val->acl_setpoint,val->acl_current);
    return 0;
  }
  return -41;
}

/*######################################################################
  # Commands
  ######################################################################*/

int prepareOutputCommand(uint8_t output_percent, uint8_t* cmd) {
  if(output_percent >= 101) {
    ESP_LOGE(TAG, "Output percent not in range!"); // 101 for Boost mode.
    return(-15);
  }
  cmd[5] = output_percent;
  cmd[6] = calc_checksum(cmd, 6);
  return 0;
}


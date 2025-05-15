#ifndef MAIN_MQTT_STUFF_H_
#define MAIN_MQTT_STUFF_H_

#include "aql_prot.h"
void mqtt_publish(aquaVal_t * aquaVal);
void mqtt_app_start(QueueHandle_t *pPowerQueue);

#endif /* MAIN_MQTT_STUFF_H_ */

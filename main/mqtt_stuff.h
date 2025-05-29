#ifndef MAIN_MQTT_STUFF_H_
#define MAIN_MQTT_STUFF_H_

#include "aql_prot.h"
#include "mqtt_client.h"

void mqtt_publish(void);
void mqtt_app_start(void);

ESP_EVENT_DECLARE_BASE(AQL_EVENTS); // declaration of the aql events family

enum {                              // declaration of the specific events under the aql event family
    AQL_EVENT_POWER_SET             // raised when new power value is set
};

#endif /* MAIN_MQTT_STUFF_H_ */

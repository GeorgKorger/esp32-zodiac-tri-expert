#ifndef MAIN_MQTT_STUFF_H_
#define MAIN_MQTT_STUFF_H_

#include "aql_prot.h"
#include "mqtt_client.h"

void mqtt_app_start(void);
void mqtt_app_stopp(void);

ESP_EVENT_DECLARE_BASE(AQL_EVENTS); // declaration of the aql events family

enum {                              // declaration of the specific events under the aql event family
    AQL_EVENT_POWER_SET,             // raised when new power value is set
    AQL_EVENT_PUBLISH_AQLVAL,        // raised when aqlVal should be published
    AQL_EVENT_PUBLISH_CONNECTED,     // raised when connected should be published
};

enum {                              // special events sent over powerQueue (reboot messages)
    POWERQUEUE_REBOOT = 111,         // a reboot should be triggered in main loop
    POWERQUEUE_OTA = 112,            // OTA should be triggered in main loop
};

#endif /* MAIN_MQTT_STUFF_H_ */

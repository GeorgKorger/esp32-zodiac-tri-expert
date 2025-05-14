idf_component_register(SRCS "flash.c" "mqtt_stuff.c" "tri-expert.c" "aql_prot.c" "aql_main.c"
                    REQUIRES mqtt nvs_flash esp_driver_uart esp_driver_gpio esp_event esp_netif protocol_examples_common
                    INCLUDE_DIRS "." )

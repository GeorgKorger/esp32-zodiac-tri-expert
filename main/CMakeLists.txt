idf_component_register(SRCS "aql_main.c" "aql_task.c" "aql_prot.c" "wifi.c"
                    REQUIRES nvs_flash esp_driver_uart esp_wifi
                    INCLUDE_DIRS ".")

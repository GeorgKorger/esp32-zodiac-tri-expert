idf_component_register(SRCS "flash.c" "tri-expert.c" "aql_prot.c" "aql_main.c"
                    REQUIRES nvs_flash esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS "." )

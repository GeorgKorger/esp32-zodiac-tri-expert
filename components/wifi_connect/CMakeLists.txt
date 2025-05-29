idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    # Header only library for linux

    idf_component_register(INCLUDE_DIRS include
                           SRCS )
    return()
endif()

set(srcs "connect.c"
         "wifi_connect.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_netif esp_wifi)
#                       PRIV_REQUIRES esp_netif esp_driver_gpio esp_driver_uart esp_wifi vfs console esp_eth openthread)



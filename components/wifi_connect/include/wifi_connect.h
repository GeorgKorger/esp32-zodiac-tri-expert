#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_netif.h"
#endif // !CONFIG_IDF_TARGET_LINUX

#ifdef __cplusplus
extern "C" {
#endif

#if !CONFIG_IDF_TARGET_LINUX

#define NETIF_DESC_STA "netif_sta"


#if CONFIG_WIFI_SCAN_METHOD_FAST
#define WIFI_SCAN_METHOD WIFI_FAST_SCAN
#elif CONFIG_WIFI_SCAN_METHOD_ALL_CHANNEL
#define WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_WIFI_CONNECT_AP_BY_SIGNAL
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_CONNECT_AP_BY_SECURITY
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_WIFI_AUTH_OPEN
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_WIFI_AUTH_WEP
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_WIFI_AUTH_WPA_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_WIFI_AUTH_WPA2_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_WIFI_AUTH_WPA_WPA2_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_WIFI_AUTH_WPA3_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_WIFI_AUTH_WPA2_WPA3_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_WIFI_AUTH_WAPI_PSK
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define INTERFACE get_netif_from_desc(NETIF_DESC_STA)
#define get_netif() get_netif_from_desc(NETIF_DESC_STA)

/**
 * @return ESP_OK on successful connection
 */
esp_err_t wifi_connect(void);

/**
 * Counterpart to connect, de-initializes Wi-Fi or Ethernet
 */
esp_err_t disconnect(void);

/**
 * @brief Returns esp-netif pointer created by connect() described by
 * the supplied desc field
 *
 * @param desc Textual interface of created network interface, for example "sta"
 * indicate default WiFi station.
 *
 */
esp_netif_t *get_netif_from_desc(const char *desc);

#else
static inline esp_err_t wifi_connect(void) {return ESP_OK;}
#endif // !CONFIG_IDF_TARGET_LINUX

#ifdef __cplusplus
}
#endif

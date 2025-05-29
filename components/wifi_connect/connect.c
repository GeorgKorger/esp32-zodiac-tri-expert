#include <string.h>
#include "wifi_connect.h"
#include "wifi_connect_private.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi_common";


/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static bool netif_desc_matches_with(esp_netif_t *netif, void *ctx)
{
    return strcmp(ctx, esp_netif_get_desc(netif)) == 0;
}

esp_netif_t *get_netif_from_desc(const char *desc)
{
    return esp_netif_find_if(netif_desc_matches_with, (void*)desc);
}

static esp_err_t print_all_ips_tcpip(void* ctx)
{
    const char *prefix = ctx;
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if (is_our_netif(prefix, netif)) {
            ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
            esp_netif_ip_info_t ip;
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&ip.ip));
        }
    }
    return ESP_OK;
}

void print_all_netif_ips(const char *prefix)
{
    // Print all IPs in TCPIP context to avoid potential races of removing/adding netifs when iterating over the list
    esp_netif_tcpip_exec(print_all_ips_tcpip, (void*) prefix);
}


esp_err_t wifi_connect(void)
{
    if (wi_connect() != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_shutdown));

    print_all_netif_ips(NETIF_DESC_STA);

    return ESP_OK;
}


esp_err_t disconnect(void)
{
    wifi_shutdown();
    ESP_ERROR_CHECK(esp_unregister_shutdown_handler(&wifi_shutdown));

    return ESP_OK;
}

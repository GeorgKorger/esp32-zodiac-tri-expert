#include <string.h>
#include <stdbool.h>
#include "esp_wifi_types_generic.h"
#include "wifi_connect.h"
#include "wifi_connect_private.h"
#include "esp_log.h"
#include "../../main/credentials.h"

#ifdef CONFIG_WIFI_WAIT
  #define WIFI_WAIT true
#else
  #define WIFI_WAIT false
#endif

static const char *TAG = "wifi_connect";
static esp_netif_t *s_wifi_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;

static int s_retry_num = 0;

static void handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    s_retry_num++;
    if (s_retry_num > CONFIG_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, delay reconnect.", s_retry_num);
        vTaskDelay( (60 * 1000) / portTICK_PERIOD_MS);
        s_retry_num = 0;
    }
    wifi_event_sta_disconnected_t *disconn = event_data;
    if (disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected %d, trying to reconnect...", disconn->reason);
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
}

static void handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}


void wifi_start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    esp_netif_config.if_desc = NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_wifi_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}


void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_wifi_sta_netif));
    esp_netif_destroy(s_wifi_sta_netif);
    s_wifi_sta_netif = NULL;
}


esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    if (wait) {
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &handler_on_wifi_connect, s_wifi_sta_netif));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP(s)");
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        vSemaphoreDelete(s_semph_get_ip_addrs);
        s_semph_get_ip_addrs = NULL;
        if (s_retry_num > CONFIG_WIFI_CONN_MAX_RETRY) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &handler_on_wifi_connect));
    return esp_wifi_disconnect();
}

void wifi_shutdown(void)
{
    wifi_sta_do_disconnect();
    wifi_stop();
}

esp_err_t wi_connect(void)
{
    ESP_LOGI(TAG, "Start wifi_connect.");
    wifi_start();
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = WIFI_SCAN_METHOD,
            .sort_method = WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = CONFIG_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    return wifi_sta_do_connect(wifi_config, WIFI_WAIT);
}




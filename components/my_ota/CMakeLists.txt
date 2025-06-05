# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(SRCS "my_ota.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_client app_update esp_https_ota
                                  esp_netif mbedtls nvs_flash esp_wifi esp_psram wifi_connect
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)

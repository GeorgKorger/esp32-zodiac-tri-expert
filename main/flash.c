#include "nvs_flash.h"
#include <stdint.h>
#include <stdbool.h>

static nvs_handle_t aql_handle;
static bool nvs_initialzed = false;

esp_err_t initFlash(void) {
  esp_err_t err = ESP_FAIL;
  // Initialize NVS
  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if( err != ESP_OK ) return( ESP_FAIL );

  // opening handle with namespace "aql"
  err = nvs_open("aql", NVS_READWRITE, &aql_handle);
  if( err != ESP_OK ) return( ESP_FAIL );
  nvs_initialzed = true;
  return ESP_OK;
}

esp_err_t reInitFlash(void) {
  nvs_initialzed = false;
  nvs_flash_deinit();
  return initFlash();
}

int restorePowerFromFlash(void) {
  esp_err_t err = ESP_FAIL;
  if( !nvs_initialzed ) {
    // init
    err = initFlash();
    if( err != ESP_OK ) return( ESP_FAIL );
  }
  // read
  int8_t p;
  err = nvs_get_i8(aql_handle, "power", &p);
  if( err != ESP_OK ) {
    nvs_close(aql_handle);
    err = initFlash();
    if( err != ESP_OK ) return( ESP_FAIL );
    err = nvs_get_i8(aql_handle, "power", &p);
    if( err != ESP_OK ) return( ESP_FAIL );
  }
  return p;
}

void storePowerToFlash(int8_t p) {
  esp_err_t err = ESP_FAIL;
  if( !nvs_initialzed ) {
    // init
    err = initFlash();
    if( err != ESP_OK ) return;
  }
  // write
  err = nvs_set_i8(aql_handle, "power", p);
  if( err != ESP_OK ) {
    nvs_close(aql_handle);
    err = initFlash();
    if( err != ESP_OK ) return;
    err = nvs_set_i8(aql_handle, "power", p);
    if( err != ESP_OK ) return;
  }
  return;
}

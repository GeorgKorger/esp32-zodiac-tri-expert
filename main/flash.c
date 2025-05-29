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

uint8_t restorePowerFromFlash(void) {
  esp_err_t err = ESP_FAIL;
  if( !nvs_initialzed ) {
    // init
    err = initFlash();
    if( err != ESP_OK ) return( 255 );
  }
  // read
  uint8_t p;
  err = nvs_get_u8(aql_handle, "power", &p);
  if( err != ESP_OK ) {
    nvs_close(aql_handle);
    err = initFlash();
    if( err != ESP_OK ) return( 255 );
    err = nvs_get_u8(aql_handle, "power", &p);
    if( err != ESP_OK ) return( 255 );
  }
  return p;
}

//ToDo: store only, if different value in flash
void storePowerToFlash(uint8_t p) {
  if( p > 100 ) return; //do not store 101 (aka boost) to flash - temporar only
  esp_err_t err = ESP_FAIL;
  if( !nvs_initialzed ) {
    // init
    err = initFlash();
    if( err != ESP_OK ) return;
  }
  // write
  err = nvs_set_u8(aql_handle, "power", p);
  if( err != ESP_OK ) {
    nvs_commit(aql_handle);
    nvs_close(aql_handle);
    err = initFlash();
    if( err != ESP_OK ) return;
    err = nvs_set_u8(aql_handle, "power", p);
    if( err != ESP_OK ) return;
  }
  return;
}

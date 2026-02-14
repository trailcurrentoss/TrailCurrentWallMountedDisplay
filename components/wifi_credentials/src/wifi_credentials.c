#include "wifi_credentials.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>

static const char *TAG = "WiFi_Credentials";

// Function to save SSID and Password to NVS
void wifi_save_credentials(const char* ssid, const char* password) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;
    err = nvs_open("wifi_creds", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(my_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing SSID to NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return;
    }

    err = nvs_set_str(my_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing Password to NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing changes to NVS: %s", esp_err_to_name(err));
    }

    nvs_close(my_handle);
}

// Function to load SSID and Password from NVS
int wifi_load_credentials(char* ssid, size_t ssid_size, char* password, size_t password_size) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;
    err = nvs_open("wifi_creds", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    size_t ssid_len = ssid_size;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading SSID from NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    size_t password_len = password_size;
    err = nvs_get_str(my_handle, "password", password, &password_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading Password from NVS: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    nvs_close(my_handle);
    return 0;
}
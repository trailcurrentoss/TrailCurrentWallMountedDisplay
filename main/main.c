#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lvgl.h"
#include "screens.h"
#include "ui.h"
#include "vars.h"
#include "wifi_credentials.h"
#include "app_mqtt.h"

/* Check if WiFi is enabled (ESP-Hosted for ESP32-P4 via ESP-WIFI-REMOTE) */
#if defined(CONFIG_ESP_HOSTED_ENABLED)
#define WIFI_ENABLED 1
#include "esp_wifi.h"
#include "esp_hosted.h"
/* WiFi event handler init from actions.c */
extern void wifi_event_handler_init(void);
#else
#define WIFI_ENABLED 0
#endif

static const char *TAG = "MAIN";

void app_main(void)
{
    /* Initialize NVS - required for WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if WIFI_ENABLED
    /* Initialize ESP-Hosted transport to communicate with C6 slave */
    ESP_LOGI(TAG, "ESP-Hosted: Initializing...");
    ret = esp_hosted_init();
    ESP_LOGI(TAG, "ESP-Hosted: esp_hosted_init() = %d", ret);
    if (ret != 0) {
        ESP_LOGE(TAG, "ESP-Hosted initialization failed!");
    }

    /* Initialize networking stack */
    ESP_LOGI(TAG, "WiFi: Initializing netif...");
    ret = esp_netif_init();
    ESP_LOGI(TAG, "WiFi: esp_netif_init() = %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "WiFi: Creating event loop...");
    ret = esp_event_loop_create_default();
    ESP_LOGI(TAG, "WiFi: esp_event_loop_create_default() = %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "WiFi: Creating default STA netif...");
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    ESP_LOGI(TAG, "WiFi: STA netif created: %p", sta_netif);

    /* Initialize WiFi with default config */
    ESP_LOGI(TAG, "WiFi: Initializing WiFi driver (ESP-Hosted for P4)...");
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_cfg);
    ESP_LOGI(TAG, "WiFi: esp_wifi_init() = %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "WiFi: Setting mode to STA...");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "WiFi: esp_wifi_set_mode(STA) = %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "WiFi: Starting WiFi...");
    ret = esp_wifi_start();
    ESP_LOGI(TAG, "WiFi: esp_wifi_start() = %s", esp_err_to_name(ret));
    ESP_ERROR_CHECK(ret);

    /* Give ESP-Hosted time to initialize the C6 slave */
    ESP_LOGI(TAG, "WiFi: Waiting for ESP-Hosted slave to be ready...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Log WiFi status */
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "WiFi: Current mode = %d (1=STA, 2=AP, 3=APSTA)", mode);

    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi: STA MAC = %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGW(TAG, "WiFi: Could not get STA MAC address");
    }

    ESP_LOGI(TAG, "WiFi initialized in STA mode - ready for scanning");

    /* Initialize MQTT client (connection starts when WiFi gets IP) */
    static const mqtt_client_config_t mqtt_cfg = {
        .broker_uri = "mqtt://YOUR_MQTT_BROKER_IP_OR_HOSTNAME",
        .client_id = "wall_display",
        .username = NULL,
        .password = NULL,
    };
    mqtt_client_init(&mqtt_cfg);
#endif

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = true,
        }};
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    if (disp != NULL)
    {
        bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
        set_var_rotation_degrees(180);
    }

    bsp_display_lock(0);
    ui_init();
#if WIFI_ENABLED
    /* Register WiFi event handlers for UI updates (must be after ui_init) */
    wifi_event_handler_init();

    /* Auto-connect to saved WiFi credentials on boot */
    {
        char saved_ssid[33] = {0};
        char saved_password[65] = {0};
        if (wifi_load_credentials(saved_ssid, sizeof(saved_ssid),
                                  saved_password, sizeof(saved_password)) == 0
            && strlen(saved_ssid) > 0) {
            ESP_LOGI(TAG, "Found saved WiFi credentials for: %s", saved_ssid);
            wifi_config_t wifi_config = {0};
            strlcpy((char *)wifi_config.sta.ssid, saved_ssid,
                    sizeof(wifi_config.sta.ssid));
            strlcpy((char *)wifi_config.sta.password, saved_password,
                    sizeof(wifi_config.sta.password));
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            lv_label_set_text(objects.label_wifi_connection_status,
                              "Status: Connecting...");
            ESP_LOGI(TAG, "WiFi auto-connect initiated");
        } else {
            ESP_LOGI(TAG, "No saved WiFi credentials found");
        }
    }
#endif
    /* TODO: REMOVE THESE DEFAULTS USED FOR TESTING */
    set_var_current_interior_temperature(65);
    set_var_desired_temperature(70);
    set_var_current_exterior_temperature(-4);
    bsp_display_unlock();
    ESP_LOGI(TAG,"Setup done");
    while (1)
    {
        bsp_display_lock(0);
        lv_timer_handler(); /* let the GUI do its work */
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

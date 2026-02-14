#include "actions.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "esp_log.h"
#include "lvgl.h"
#include "screens.h"
#include "styles.h"
#include "ui.h"
#include "vars.h"
#include "wifi_credentials.h"
#include "app_mqtt.h"
#include <stdint.h>
#include <string.h>

/* Check if WiFi is enabled (ESP-Hosted for ESP32-P4) */
#if defined(CONFIG_ESP_HOSTED_ENABLED)
#define WIFI_ENABLED 1
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#else
#define WIFI_ENABLED 0
#endif

/* WiFi scan results storage */
#define MAX_WIFI_NETWORKS 20
#if WIFI_ENABLED
static wifi_ap_record_t wifi_scan_results[MAX_WIFI_NETWORKS];
static uint16_t wifi_scan_count = 0;
static char wifi_networks_list[MAX_WIFI_NETWORKS * 33 + MAX_WIFI_NETWORKS]; /* SSIDs + newlines */
static int selected_wifi_index = -1;
static char wifi_connected_ip[20] = {0};
static int wifi_retry_count = 0;
#define WIFI_MAX_RETRIES 5

/**
 * @brief WiFi event handler to update UI on connection status changes
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI("WIFI_EVENT", "Connected to AP");
        wifi_retry_count = 0;
        bsp_display_lock(0);
        lv_label_set_text(objects.label_wifi_connection_status, "Status: Connected, getting IP...");
        bsp_display_unlock();
        break;
      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW("WIFI_EVENT", "Disconnected from AP, reason: %d", disconn->reason);
        if (wifi_retry_count < WIFI_MAX_RETRIES) {
          wifi_retry_count++;
          ESP_LOGI("WIFI_EVENT", "Retrying connection (%d/%d)...", wifi_retry_count, WIFI_MAX_RETRIES);
          esp_wifi_connect();
          char retry_msg[64];
          snprintf(retry_msg, sizeof(retry_msg), "Status: Retry %d/%d...", wifi_retry_count, WIFI_MAX_RETRIES);
          bsp_display_lock(0);
          lv_label_set_text(objects.label_wifi_connection_status, retry_msg);
          bsp_display_unlock();
        } else {
          ESP_LOGW("WIFI_EVENT", "Max retries reached, giving up");
          bsp_display_lock(0);
          lv_label_set_text(objects.label_wifi_connection_status, "Status: Connection failed");
          bsp_display_unlock();
        }
        break;
      }
      default:
        break;
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      snprintf(wifi_connected_ip, sizeof(wifi_connected_ip), IPSTR, IP2STR(&event->ip_info.ip));
      ESP_LOGI("WIFI_EVENT", "Got IP: %s", wifi_connected_ip);
      char status_msg[64];
      snprintf(status_msg, sizeof(status_msg), "Connected: %s", wifi_connected_ip);
      bsp_display_lock(0);
      lv_label_set_text(objects.label_wifi_connection_status, status_msg);
      bsp_display_unlock();
      mqtt_client_start();
    }
  }
}

/**
 * @brief Initialize WiFi event handlers for UI updates
 * Call this once after WiFi is initialized
 */
void wifi_event_handler_init(void) {
  esp_event_handler_instance_t wifi_handler;
  esp_event_handler_instance_t ip_handler;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &wifi_handler));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &ip_handler));
  ESP_LOGI("ACTIONS", "WiFi event handlers registered for UI updates");
}
#endif

static const char *TAG = "ACTIONS";

void action_change_screen_brightness(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  bsp_display_brightness_set(value);
}

void action_change_theme(lv_event_t *e) {
  set_var_user_settings_changed(true);
  int themeIndex = (int)lv_event_get_user_data(e);
  set_var_selected_theme(themeIndex);
}

void action_change_screen(lv_event_t *e) {
  int *to_screen = (int *)lv_event_get_user_data(e);
  /* Clear all toolbar button checked states for all home buttons */
  lv_obj_clear_state(
      objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_home_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_home_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all toolbar button checked states for the traile page buttons */
  lv_obj_clear_state(
      objects.widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all toolbar button checked states for the power page buttons */
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all the toolbar button checked states for the air quality page */
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all the toolbar button checked states for the water page */
  lv_obj_clear_state(
      objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_water_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_water_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all the toolbar button checked states for the water page */
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);
  /* Clear all the toolbar buttons checked states for the settings page */
  lv_obj_clear_state(
      objects.widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_home,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_power,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects.widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_water,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
      LV_STATE_CHECKED);
  lv_obj_clear_state(
      objects
          .widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_settings,
      LV_STATE_CHECKED);

  if (to_screen == 0) {
    /* Set the active state for the home page home icon */
    lv_obj_add_state(
        objects.widget_home_page_bottom_nav_bar__bottom_nav_bar_button_home,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_home);
  } else if (to_screen == 1) {
    /* Set the active state for the trailer page trailer icon */
    lv_obj_add_state(
        objects
            .widget_trailer_page_bottom_nav_bar__bottom_nav_bar_button_trailer,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_trailer);
  } else if (to_screen == 2) {
    /* Set the active state for the power page power icon */
    lv_obj_add_state(
        objects
            .widget_power_management_page_bottom_nav_bar__bottom_nav_bar_button_power,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_power_management);
  } else if (to_screen == 3) {
    /* Set the active state for the air quality page air icon */
    lv_obj_add_state(
        objects
            .widget_air_quality_page_bottom_nav_bar__bottom_nav_bar_button_air_quality,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_air_quality);
  } else if (to_screen == 4) {
    /* Set the active state for the water page water icon */
    lv_obj_add_state(
        objects.widget_water_page_bottom_nav_bar__bottom_nav_bar_button_water,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_water);
  } else if (to_screen == 5) {
    /* Set the active state for the entertainment page entertainment icon */
    lv_obj_add_state(
        objects
            .widget_entertainment_page_bottom_nav_bar__bottom_nav_bar_button_entertainment,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_entertainment);
  } else {
    /* Set the active state for the settings page settings icon */
    lv_obj_add_state(
        objects
            .widget_settings_page_bottom_nav_bar__bottom_nav_bar_button_settings,
        LV_STATE_CHECKED);
    /* Change to that screen */
    lv_scr_load(objects.page_settings);
  }
}

void action_rotate_screen(lv_event_t *e) {
  uint32_t currentDegrees = get_var_rotation_degrees();
  lv_disp_t *disp = lv_disp_get_default();
  int *btn_no_ptr = (int *)lv_event_get_user_data(e);
  if (btn_no_ptr == 0) {
    if (currentDegrees == 0) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90);
      set_var_rotation_degrees(90);
    } else if (currentDegrees == 90) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
      set_var_rotation_degrees(180);
    } else if (currentDegrees == 180) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_270);
      set_var_rotation_degrees(270);
    } else {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);
      set_var_rotation_degrees(0);
    }
  } else {
    if (currentDegrees == 270) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
      set_var_rotation_degrees(180);
    } else if (currentDegrees == 180) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90);
      set_var_rotation_degrees(90);
    } else if (currentDegrees == 90) {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);
      set_var_rotation_degrees(0);
    } else {
      bsp_display_rotate(disp, LV_DISPLAY_ROTATION_270);
      set_var_rotation_degrees(270);
    }
  }
}

void action_send_device_command(lv_event_t *e) {
  int device_id = (int)lv_event_get_user_data(e);
  char topic[64];
  snprintf(topic, sizeof(topic), "trailcurrent/lights/%d/set", device_id);
  const char *payload = "{\"action\":\"toggle\"}";
  if (mqtt_client_publish(topic, payload, strlen(payload)) < 0) {
    ESP_LOGW(TAG, "MQTT not connected, cannot send toggle for device %d", device_id);
  }
}

void action_change_desired_temperature(lv_event_t *e) {
  int new_desired_temperature = lv_arc_get_value(objects.arc_thermostat);
  set_var_desired_temperature(new_desired_temperature);
}

void action_change_fm_radio_station(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  float deisred_station = (value / 10.0);
  set_var_desired_fm_radio_station(deisred_station);
}

void action_go_to_preset(lv_event_t *e) { ESP_LOGE(TAG, "Got here prset"); }

/* NOTE: These have to match the order of the drop down items in the UI */
const char *timezoneItems[41] = {"ASKT9AKDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "HST11HDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00",
                                 "MST7"};
void action_timezone_change(lv_event_t *e) {
  uint8_t selectedIdex =
      lv_dropdown_get_selected(objects.drop_down_selected_time_zone);
  set_var_user_settings_changed(true);
  set_var_current_time_zone_string(timezoneItems[selectedIdex]);
}

void action_timeout_changed(lv_event_t *e) {
  set_var_user_settings_changed(true);
  int currentTimeoutValue = get_var_screen_timeout_value();
  int eventValue = (int)lv_event_get_user_data(e);
  if (eventValue == 0) {
    currentTimeoutValue--;
  } else {
    currentTimeoutValue++;
  }
  set_var_screen_timeout_value(currentTimeoutValue);
}

void action_show_device_brightness_dialog(lv_event_t *e) {
  lv_obj_clear_flag(objects.container_modal_background, LV_OBJ_FLAG_HIDDEN);
  int eventValue = (int)lv_event_get_user_data(e);
  set_var_current_device_brightness_identifier(eventValue);
}

void action_close_dialog(lv_event_t *e) {
  lv_obj_add_flag(objects.container_modal_background, LV_OBJ_FLAG_HIDDEN);
  set_var_current_device_brightness_identifier(-1);
}

void action_set_device_brightness_level(lv_event_t *e) {
  int32_t device_id = get_var_current_device_brightness_identifier();
  if (device_id < 0) return;
  lv_obj_t *slider = lv_event_get_target(e);
  int32_t slider_val = lv_slider_get_value(slider);
  int32_t brightness = slider_val * 255 / 100; /* scale 0-100 to 0-255 */
  char topic[64];
  snprintf(topic, sizeof(topic), "trailcurrent/lights/%d/set", (int)device_id);
  char payload[32];
  snprintf(payload, sizeof(payload), "{\"brightness\":%d}", (int)brightness);
  mqtt_client_publish(topic, payload, strlen(payload));
}

void action_selected_wifi_changed(lv_event_t *e) {
  // TODO: Implement action selected_wifi_changed here
}

void action_show_wi_fi_keyaboard_entry(lv_event_t *e) {
  (void)e; /* Password keyboard is shown when password field is focused */
  lv_obj_clear_flag(objects.keyboard_password, LV_OBJ_FLAG_HIDDEN);
}

void action_hide_wifi_keyboard(lv_event_t *e) {
  (void)e; /* Hide the password keyboard */
  lv_obj_add_flag(objects.keyboard_password, LV_OBJ_FLAG_HIDDEN);
}

#if WIFI_ENABLED
/**
 * @brief Build the roller options string from scan results
 */
static void build_wifi_networks_string(void) {
  wifi_networks_list[0] = '\0';

  if (wifi_scan_count == 0) {
    strcpy(wifi_networks_list, "No networks found");
    return;
  }

  char *ptr = wifi_networks_list;
  for (int i = 0; i < wifi_scan_count && i < MAX_WIFI_NETWORKS; i++) {
    /* Add SSID to the list */
    int len = strlen((char *)wifi_scan_results[i].ssid);
    if (len > 0) {
      memcpy(ptr, wifi_scan_results[i].ssid, len);
      ptr += len;

      /* Add signal strength indicator */
      int rssi = wifi_scan_results[i].rssi;
      if (rssi > -50) {
        strcpy(ptr, " ****");
      } else if (rssi > -60) {
        strcpy(ptr, " ***");
      } else if (rssi > -70) {
        strcpy(ptr, " **");
      } else {
        strcpy(ptr, " *");
      }
      ptr += strlen(ptr);

      /* Add newline separator for roller (except for last item) */
      if (i < wifi_scan_count - 1) {
        *ptr++ = '\n';
      }
    }
  }
  *ptr = '\0';
}

/**
 * @brief Scan for WiFi networks action handler
 * Called when "Scan Networks" button is clicked
 */
void action_scan_wifi_networks(lv_event_t *e) {
  (void)e;
  ESP_LOGI(TAG, "=== WiFi Scan Debug ===");

  /* Check WiFi mode first */
  wifi_mode_t mode;
  esp_err_t err = esp_wifi_get_mode(&mode);
  ESP_LOGI(TAG, "WiFi mode check: err=%s, mode=%d (1=STA)", esp_err_to_name(err), mode);

  /* Check if WiFi is started */
  uint8_t mac[6];
  err = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "WiFi STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    ESP_LOGE(TAG, "Cannot get MAC - WiFi may not be initialized: %s", esp_err_to_name(err));
  }

  /* Update status label */
  lv_label_set_text(objects.label_wifi_scan_status, "Scanning...");

  /* Configure and start the scan */
  wifi_scan_config_t scan_config = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = false,
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
      .scan_time.active.min = 120,
      .scan_time.active.max = 500,
  };

  ESP_LOGI(TAG, "Starting scan (blocking, all channels, active scan)...");
  err = esp_wifi_scan_start(&scan_config, true); /* blocking scan */
  ESP_LOGI(TAG, "esp_wifi_scan_start() returned: %s", esp_err_to_name(err));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi scan failed to start: %s", esp_err_to_name(err));
    lv_label_set_text(objects.label_wifi_scan_status, "Scan failed");
    return;
  }

  /* Get number of APs found */
  uint16_t ap_count = 0;
  err = esp_wifi_scan_get_ap_num(&ap_count);
  ESP_LOGI(TAG, "esp_wifi_scan_get_ap_num(): err=%s, count=%d", esp_err_to_name(err), ap_count);

  /* Get scan results */
  wifi_scan_count = MAX_WIFI_NETWORKS;
  err = esp_wifi_scan_get_ap_records(&wifi_scan_count, wifi_scan_results);
  ESP_LOGI(TAG, "esp_wifi_scan_get_ap_records(): err=%s, returned=%d records",
           esp_err_to_name(err), wifi_scan_count);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
    lv_label_set_text(objects.label_wifi_scan_status, "Scan failed");
    return;
  }

  /* Remove entries with empty SSIDs so roller indices match array indices */
  int write_idx = 0;
  for (int i = 0; i < wifi_scan_count; i++) {
    if (strlen((char *)wifi_scan_results[i].ssid) > 0) {
      if (write_idx != i) {
        wifi_scan_results[write_idx] = wifi_scan_results[i];
      }
      write_idx++;
    }
  }
  wifi_scan_count = write_idx;

  /* Log each network found */
  ESP_LOGI(TAG, "=== Scan Results (%d networks) ===", wifi_scan_count);
  for (int i = 0; i < wifi_scan_count; i++) {
    ESP_LOGI(TAG, "  [%d] SSID: %s, RSSI: %d, Channel: %d, Auth: %d",
             i, wifi_scan_results[i].ssid, wifi_scan_results[i].rssi,
             wifi_scan_results[i].primary, wifi_scan_results[i].authmode);
  }

  /* Build the roller options string */
  build_wifi_networks_string();

  /* Update the roller with network list */
  lv_roller_set_options(objects.roller_wifi_networks, wifi_networks_list, LV_ROLLER_MODE_NORMAL);

  /* Update status label */
  char status_msg[64];
  snprintf(status_msg, sizeof(status_msg), "Found %d network%s",
           wifi_scan_count, wifi_scan_count == 1 ? "" : "s");
  lv_label_set_text(objects.label_wifi_scan_status, status_msg);

  /* Reset selection */
  selected_wifi_index = -1;
}

/**
 * @brief WiFi network selected action handler
 * Called when user selects a network from the roller
 */
void action_wifi_network_selected(lv_event_t *e) {
  (void)e;
  uint16_t selected = lv_roller_get_selected(objects.roller_wifi_networks);

  if (selected < wifi_scan_count) {
    selected_wifi_index = selected;
    ESP_LOGI(TAG, "Selected WiFi network: %s (RSSI: %d)",
             wifi_scan_results[selected].ssid,
             wifi_scan_results[selected].rssi);

    /* Show the keyboard for password entry */
    lv_obj_clear_flag(objects.keyboard_password, LV_OBJ_FLAG_HIDDEN);

    /* Focus on password field */
    lv_obj_add_state(objects.textarea_wifi_password, LV_STATE_FOCUSED);
  }
}

/**
 * @brief Connect to WiFi action handler
 * Called when "Connect" button is clicked
 */
void action_connect_to_wifi(lv_event_t *e) {
  (void)e;

  /* Hide keyboard if visible */
  lv_obj_add_flag(objects.keyboard_password, LV_OBJ_FLAG_HIDDEN);

  if (selected_wifi_index < 0 || (uint16_t)selected_wifi_index >= wifi_scan_count) {
    ESP_LOGW(TAG, "No WiFi network selected");
    lv_label_set_text(objects.label_wifi_connection_status, "Status: Select a network first");
    return;
  }

  /* Get the selected SSID */
  const char *ssid = (const char *)wifi_scan_results[selected_wifi_index].ssid;

  /* Get the password from textarea */
  const char *password = lv_textarea_get_text(objects.textarea_wifi_password);

  ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
  lv_label_set_text(objects.label_wifi_connection_status, "Status: Connecting...");
  wifi_retry_count = 0;

  /* Save credentials to NVS */
  wifi_save_credentials(ssid, password);

  /* Configure WiFi station */
  wifi_config_t wifi_config = {0};
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
    lv_label_set_text(objects.label_wifi_connection_status, "Status: Config failed");
    return;
  }

  /* Start connection */
  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(err));
    lv_label_set_text(objects.label_wifi_connection_status, "Status: Connection failed");
    return;
  }

  /* Connection status will be updated by WiFi event handler */
  lv_label_set_text(objects.label_wifi_connection_status, "Status: Connecting...");
}

#else /* WIFI_ENABLED == 0 */

/**
 * WiFi not enabled - stub implementations
 * Enable ESP-Hosted WiFi in menuconfig to use WiFi features on ESP32-P4
 */
void action_scan_wifi_networks(lv_event_t *e) {
  (void)e;
  ESP_LOGW(TAG, "WiFi not enabled. Enable CONFIG_ESP_HOSTED_ENABLED in menuconfig.");
  lv_label_set_text(objects.label_wifi_scan_status, "WiFi not configured");
}

void action_wifi_network_selected(lv_event_t *e) {
  (void)e;
  ESP_LOGW(TAG, "WiFi not enabled");
}

void action_connect_to_wifi(lv_event_t *e) {
  (void)e;
  ESP_LOGW(TAG, "WiFi not enabled. Enable CONFIG_ESP_HOSTED_ENABLED in menuconfig.");
  lv_label_set_text(objects.label_wifi_connection_status, "WiFi not configured");
}

#endif /* WIFI_ENABLED */


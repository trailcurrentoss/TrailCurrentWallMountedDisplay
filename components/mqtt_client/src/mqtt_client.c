#include "app_mqtt.h"
#include "mqtt_client.h" /* ESP-IDF esp_mqtt library */
#include "esp_log.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        s_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        s_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        break;
    default:
        break;
    }
}

void mqtt_client_init(const mqtt_client_config_t *config) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,
        .credentials.client_id = config->client_id,
        .credentials.username = config->username,
        .credentials.authentication.password = config->password,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "MQTT client initialized (broker: %s)", config->broker_uri);
}

void mqtt_client_start(void) {
    if (s_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return;
    }
    ESP_LOGI(TAG, "Starting MQTT client...");
    esp_mqtt_client_start(s_client);
}

bool mqtt_client_is_connected(void) {
    return s_connected;
}

int mqtt_client_publish(const char *topic, const char *payload, int payload_len) {
    if (!s_connected || s_client == NULL) {
        ESP_LOGW(TAG, "Not connected, cannot publish to %s", topic);
        return -1;
    }
    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, payload_len, 0, 0);
    ESP_LOGI(TAG, "Published to %s (msg_id=%d)", topic, msg_id);
    return msg_id;
}

int mqtt_client_subscribe(const char *topic) {
    if (!s_connected || s_client == NULL) {
        ESP_LOGW(TAG, "Not connected, cannot subscribe to %s", topic);
        return -1;
    }
    int msg_id = esp_mqtt_client_subscribe(s_client, topic, 0);
    ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)", topic, msg_id);
    return msg_id;
}

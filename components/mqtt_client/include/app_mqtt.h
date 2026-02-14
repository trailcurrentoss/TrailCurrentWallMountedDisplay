#ifndef APP_MQTT_H
#define APP_MQTT_H

#include <stdbool.h>

typedef struct {
    const char* broker_uri;
    const char* client_id;
    const char* username;
    const char* password;
} mqtt_client_config_t;

void mqtt_client_init(const mqtt_client_config_t* config);

void mqtt_client_start(void);

bool mqtt_client_is_connected(void);

int mqtt_client_publish(const char* topic, const char* payload, int payload_len);

int mqtt_client_subscribe(const char* topic);

#endif // APP_MQTT_H

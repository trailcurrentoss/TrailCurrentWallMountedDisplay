#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#include <stdio.h>
#include <stdlib.h>

// Function to save SSID and Password to NVS
void wifi_save_credentials(const char* ssid, const char* password);

// Function to load SSID and Password from NVS
int wifi_load_credentials(char* ssid, size_t ssid_size, char* password, size_t password_size);

#endif // WIFI_CREDENTIALS_H
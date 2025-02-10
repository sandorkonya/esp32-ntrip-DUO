#ifndef ESP32_XBEE_NETWORK_H
#define ESP32_XBEE_NETWORK_H

#include <esp_netif.h>
#include <esp_netif_types.h>
#include <stdbool.h>


esp_netif_t *network_init();
bool network_is_ethernet();

#endif // ESP32_XBEE_NETWORK_H
// include/wifi_manager.h  

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"           // for esp_err_t
#include "freertos/FreeRTOS.h" // for TickType_t
#include "freertos/event_groups.h" // for EventGroupHandle_t, BIT0
#include "esp_netif.h"             // for esp_netif_ip_info_t


/** Bit mask for Wi-Fi “connected” event */
#define WIFI_CONNECTED_BIT BIT0

/**
 * @brief Initialize Wi-Fi in station mode (with auto-reconnect).
 * @return ESP_OK on success, or an esp_err_t code on failure.
 */
esp_err_t wifi_init(void);

/**
 * @brief Retrieve the event group handle used by the Wi-Fi manager.
 *        Tasks can block on WIFI_CONNECTED_BIT to wait for an IP.
 */
EventGroupHandle_t wifi_event_group(void);

/**
 * @brief Get a copy of the current IP information.
 */
esp_netif_ip_info_t wifi_get_ip_info(void);

/**
 * @brief Force a Wi-Fi disconnect so the manager will reconnect.
 */
void wifi_force_reconnect(void);

#endif // WIFI_MANAGER_H
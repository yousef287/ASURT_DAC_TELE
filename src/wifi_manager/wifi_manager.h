// include/wifi_manager.h  

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"           // for esp_err_t
#include "freertos/FreeRTOS.h" // for TickType_t
#include "freertos/event_groups.h" // for EventGroupHandle_t, BIT0
#include "esp_netif.h"             // for esp_netif_ip_info_t


/** Bit mask for Wi-Fi “connected” event */
#define WIFI_CONNECTED_BIT BIT0

/** Default reconnect back-off values (ms). Override before including to change */
#ifndef WIFI_RETRY_INITIAL_DELAY_MS
#define WIFI_RETRY_INITIAL_DELAY_MS 500
#endif

#ifndef WIFI_RETRY_MAX_DELAY_MS
#define WIFI_RETRY_MAX_DELAY_MS     10000
#endif

/**
 * @brief Initialise Wi-Fi in station mode with auto reconnect.
 *
 * @param ssid     Access point SSID
 * @param password Access point password
 *
 * This function sets up the Wi-Fi driver, registers event handlers and
 * attempts to connect using the supplied credentials.
 */
esp_err_t wifi_init(const char *ssid, const char *password);

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
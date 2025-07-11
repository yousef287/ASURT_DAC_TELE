
// src/wifi_manager.c

/*
 * WiFi Manager for ESP32
 * ----------------------
 * Handles Wi-Fi init in station mode, auto-reconnect, and
 * signals connected state via a FreeRTOS event group.
 * Standalone version without application specific dependencies.
 */

#include "wifi_manager.h"       // wifi_init(), wifi_event_group()
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <sys/param.h>  // MIN()
#include <string.h>

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group = NULL;
static SemaphoreHandle_t s_ip_mutex = NULL;
static esp_netif_ip_info_t s_ip_info;
static uint32_t s_retry_delay_ms = WIFI_RETRY_INITIAL_DELAY_MS;
static TimerHandle_t s_reconnect_timer = NULL;

static void wifi_reconnect_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Reconnecting to AP...");
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }
}

EventGroupHandle_t wifi_event_group(void) {
    return s_wifi_event_group;
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Station started, connecting to AP...");
        s_retry_delay_ms = WIFI_RETRY_INITIAL_DELAY_MS;
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Lost connection: clear bit and attempt reconnection
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi disconnected. Reconnecting in %lu ms", (unsigned long)s_retry_delay_ms);
        if (xTimerIsTimerActive(s_reconnect_timer)) {
            xTimerStop(s_reconnect_timer, 0);
        }
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_retry_delay_ms), 0);
        xTimerStart(s_reconnect_timer, 0);
        s_retry_delay_ms = MIN(s_retry_delay_ms * 2, WIFI_RETRY_MAX_DELAY_MS);

        if (s_ip_mutex) {
            xSemaphoreTake(s_ip_mutex, portMAX_DELAY);
            memset(&s_ip_info, 0, sizeof(s_ip_info));
            xSemaphoreGive(s_ip_mutex);
        }

    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *) event_data;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (s_ip_mutex) {
            xSemaphoreTake(s_ip_mutex, portMAX_DELAY);
            s_ip_info = evt->ip_info;
            xSemaphoreGive(s_ip_mutex);
        }
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        if (xTimerIsTimerActive(s_reconnect_timer)) {
            xTimerStop(s_reconnect_timer, 0);
        }
        s_retry_delay_ms = WIFI_RETRY_INITIAL_DELAY_MS;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Lost IP address, reconnecting in %lu ms", (unsigned long)s_retry_delay_ms);
        if (xTimerIsTimerActive(s_reconnect_timer)) {
            xTimerStop(s_reconnect_timer, 0);
        }
        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(s_retry_delay_ms), 0);
        xTimerStart(s_reconnect_timer, 0);
        s_retry_delay_ms = MIN(s_retry_delay_ms * 2, WIFI_RETRY_MAX_DELAY_MS);

        if (s_ip_mutex) {
            xSemaphoreTake(s_ip_mutex, portMAX_DELAY);
            memset(&s_ip_info, 0, sizeof(s_ip_info));
            xSemaphoreGive(s_ip_mutex);
        }
    }
}

esp_err_t wifi_init(const char *ssid, const char *password)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    s_ip_mutex = xSemaphoreCreateMutex();
    if (!s_ip_mutex) {
        ESP_LOGE(TAG, "Failed to create IP mutex");
        return ESP_FAIL;
    }
    memset(&s_ip_info, 0, sizeof(s_ip_info));

    s_reconnect_timer = xTimerCreate("reconn",
                                     pdMS_TO_TICKS(1000),
                                     pdFALSE,
                                     NULL,
                                     wifi_reconnect_cb);
    if (!s_reconnect_timer) {
        ESP_LOGE(TAG, "Failed to create reconnect timer");
        return ESP_FAIL;
    }

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t sta_cfg;
    memset(&sta_cfg, 0, sizeof(sta_cfg));
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization complete");
    return ESP_OK;
}

esp_netif_ip_info_t wifi_get_ip_info(void)
{
    esp_netif_ip_info_t copy;
    memset(&copy, 0, sizeof(copy));
    if (s_ip_mutex) {
        xSemaphoreTake(s_ip_mutex, portMAX_DELAY);
        copy = s_ip_info;
        xSemaphoreGive(s_ip_mutex);
    }
    return copy;
}

void wifi_force_reconnect(void)
{
    ESP_LOGW(TAG, "Forcing Wi-Fi reconnection");
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_disconnect failed: %s", esp_err_to_name(err));
    }
}

// src/udp_sender.c

#include "test.h"                  // dataQueue & DashboardData
#include "udp_sender.h"            // udp_sender_task, udp_socket_close
#include "wifi_manager.h"          // wifi_event_group()
#include "freertos/event_groups.h" // xEventGroupWaitBits, xEventGroupGetBits
#include "freertos/semphr.h"       // xSemaphoreCreateMutex
#include "lwip/sockets.h"          // socket(), sendto()
#include "lwip/inet.h"             // inet_addr()
#include "esp_log.h"               // ESP_LOG*
#include "esp_heap_caps.h"         // heap_caps_get_free_size()
#include "config.h"                // SERVER_IP, SERVER_PORT
#include <string.h>                // memset(), snprintf()
#include <errno.h>                 // errno
#include <unistd.h>                // close()

static const char *TAG = "udp_sender";
static int udp_sock = -1;
static struct sockaddr_in dest_addr;
static SemaphoreHandle_t udp_mutex = NULL;

/*
 * Some Wi-Fi chipsets report errno 118 when the connection drops.
 * Use this value to detect the condition before the event bit clears.
 */
#define WIFI_SEND_ERR 118

#define UDP_MAX_RETRIES    4
#define UDP_BASE_DELAY_MS 10

// Counter for periodic heap logging
static uint32_t heap_log_counter = 0;

/**
 * @brief (Re)create the UDP socket and configure dest_addr.
 */
static bool init_udp_socket(void) {
    if (!udp_mutex) {
        udp_mutex = xSemaphoreCreateMutex();
        if (!udp_mutex) {
            ESP_LOGE(TAG, "Failed to create UDP mutex");
            return false;
        }
    }

    xSemaphoreTake(udp_mutex, portMAX_DELAY);

    if (udp_sock >= 0) {
        close(udp_sock);
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0) {
        int err = errno;
        xSemaphoreGive(udp_mutex);
        ESP_LOGE(TAG, "socket() failed: errno %d", err);
        return false;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(SERVER_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    xSemaphoreGive(udp_mutex);
    return true;
}

/**
 * @brief Close the UDP socket (called from wifi_manager on disconnect).
 */
void udp_socket_close(void) {
    if (!udp_mutex) {
        udp_mutex = xSemaphoreCreateMutex();
        if (!udp_mutex) {
            ESP_LOGE(TAG, "Failed to create UDP mutex");
            if (udp_sock >= 0) {
                close(udp_sock);
                udp_sock = -1;
            }
            return;
        }
    }

    xSemaphoreTake(udp_mutex, portMAX_DELAY);

    if (udp_sock >= 0) {
        close(udp_sock);
        udp_sock = -1;
        ESP_LOGI(TAG, "UDP socket closed due to Wi-Fi drop");
    }

    xSemaphoreGive(udp_mutex);
}

void udp_sender_task(void *pvParameters)
{
    EventGroupHandle_t eg = wifi_event_group();

    // Wait for initial Wi-Fi connection
    xEventGroupWaitBits(eg,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi connected; starting UDP sender");

    if (!init_udp_socket()) {
        ESP_LOGE(TAG, "Initial UDP socket setup failed");
        vTaskDelete(NULL);
        return;
    }

    char message[150];
    DashboardData current, newer;
    int len;

    while (1) {
        // A) Receive the newest ADC sample
        if (xQueueReceive(dataQueue, &current, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Queue receive failed");
            continue;
        }

        // B) If Wi-Fi dropped, wait and re-init socket
        if ((xEventGroupGetBits(eg) & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGW(TAG, "Wi-Fi lost, waiting to reconnect...");
            xEventGroupWaitBits(eg,
                                WIFI_CONNECTED_BIT,
                                pdFALSE,
                                pdTRUE,
                                portMAX_DELAY);
            ESP_LOGI(TAG, "Wi-Fi reconnected; re-initializing socket");
            init_udp_socket();
        }

        // C) Serialize data into CSV
        len = snprintf(message, sizeof(message),
                       "%.1f,%d,%d,%d,%.1f,%.1f,%d,%.6f,%.6f,%d,%d,%d,%d,%.2f,%.2f",
                       current.speed,
                       current.rpm,
                       current.accPedal,
                       current.brakePedal,
                       current.encoder,
                       current.temperature,
                       current.batteryLevel,
                       current.gpsLongitude,
                       current.gpsLatitude,
                       current.frWheelSpeed,
                       current.flWheelSpeed,
                       current.brWheelSpeed,
                       current.blWheelSpeed,
                       current.lateralG,
                       current.longitudinalG);

        // D) Send with exponential back-off and fresh-data check
        bool sent = false;
        int last_err = 0;
        for (int attempt = 1; attempt <= UDP_MAX_RETRIES; ++attempt) {
            // Fresh-data check
            if (xQueueReceive(dataQueue, &newer, 0) == pdTRUE) {
                current = newer;
                len = snprintf(message, sizeof(message),
                               "%.1f,%d,%d,%d,%.1f,%.1f,%d,%.6f,%.6f,%d,%d,%d,%d,%.2f,%.2f",
                               current.speed,
                               current.rpm,
                               current.accPedal,
                               current.brakePedal,
                               current.encoder,
                               current.temperature,
                               current.batteryLevel,
                               current.gpsLongitude,
                               current.gpsLatitude,
                               current.frWheelSpeed,
                               current.flWheelSpeed,
                               current.brWheelSpeed,
                               current.blWheelSpeed,
                               current.lateralG,
                               current.longitudinalG);
                attempt = 0;
                continue;
            }

            xSemaphoreTake(udp_mutex, portMAX_DELAY);
            int ret = sendto(udp_sock,
                             message, len, 0,
                             (struct sockaddr *)&dest_addr,
                             sizeof(dest_addr));
            last_err = errno;
            xSemaphoreGive(udp_mutex);
            if (ret < 0) {
                if (attempt == 1) {
                    ESP_LOGW(TAG,
                             "sendto failed (errno %d), retrying", last_err);
                    init_udp_socket();
                }
                /*
                 * If Wi-Fi has dropped but the event bit hasn't been
                 * cleared yet we would keep retrying and spamming the
                 * log. Break out early when the connection bit is gone
                 * or the typical disconnect errno is observed.
                 */
                if ((xEventGroupGetBits(eg) & WIFI_CONNECTED_BIT) == 0 ||
                    last_err == WIFI_SEND_ERR) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(UDP_BASE_DELAY_MS << (attempt - 1)));
            } else {
                sent = true;
                break;
            }
        }
        if (!sent) {
            if ((xEventGroupGetBits(eg) & WIFI_CONNECTED_BIT) == 0 ||
                last_err == WIFI_SEND_ERR) {
                ESP_LOGW(TAG, "Waiting for Wi-Fi to reconnect...");
                xEventGroupWaitBits(eg,
                                    WIFI_CONNECTED_BIT,
                                    pdFALSE,
                                    pdTRUE,
                                    portMAX_DELAY);
                ESP_LOGI(TAG, "Wi-Fi reconnected; re-initializing socket");
                init_udp_socket();
            } else {
                ESP_LOGE(TAG,
                         "Dropping packet after %d retries (errno %d)",
                         UDP_MAX_RETRIES,
                         last_err);
            }
        }

        // E) Monitor heap every 1000 loops (~10 s at 100 Hz)
        if (++heap_log_counter >= 1000) {
            size_t free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
            ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)free);
            heap_log_counter = 0;
        }

        // F) Pace at ~100 Hz
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
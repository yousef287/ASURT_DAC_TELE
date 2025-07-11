#include "connectivity.h"
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "conn_check";

// Try to open a TCP socket to the test IP/port. Returns true on success.
static bool connectivity_test(void)
{
    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(CONNECTIVITY_TEST_PORT);
    dest.sin_addr.s_addr = inet_addr(CONNECTIVITY_TEST_IP);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed");
        return false;
    }

    int err = connect(sock, (struct sockaddr *)&dest, sizeof(dest));
    close(sock);
    return (err == 0);
}

void connectivity_monitor_task(void *pvParameters)
{
    EventGroupHandle_t eg = wifi_event_group();
    int fails = 0;

    while (1) {
        // Wait until Wi-Fi is connected
        xEventGroupWaitBits(eg, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        // Delay between connectivity checks
        vTaskDelay(pdMS_TO_TICKS(CONNECTIVITY_CHECK_INTERVAL_MS));

        if (connectivity_test()) {
            // Reset on success
            if (fails != 0) {
                ESP_LOGI(TAG, "Connectivity restored after %d failed attempt%s", fails, (fails == 1 ? "" : "s"));
            }
            fails = 0;

        } else {
            // Increment fail counter
            ++fails;
            int remaining = CONNECTIVITY_FAIL_THRESHOLD - fails;

            // Log remaining tries
            ESP_LOGW(TAG,
                     "Connectivity test failed (%d/%d). %d try%s left before forcing Wi-Fi reconnect",
                     fails,
                     CONNECTIVITY_FAIL_THRESHOLD,
                     remaining,
                     (remaining == 1 ? "" : "s"));

            // If we've hit the threshold, force a reconnect
            if (fails >= CONNECTIVITY_FAIL_THRESHOLD) {
                fails = 0;
                ESP_LOGW(TAG,
                         "No connectivity after %d attempts — forcing Wi-Fi reconnect now",
                         CONNECTIVITY_FAIL_THRESHOLD);
                wifi_force_reconnect();

                // Give reconnect a moment before next test
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}

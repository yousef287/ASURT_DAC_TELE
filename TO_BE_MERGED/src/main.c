#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "test.h"
#include "udp_sender.h"
#include "mqtt_sender.h"
#include "config.h"
#include "connectivity.h"
#include "nvs_flash.h"

static const char *TAG = "main";
QueueHandle_t dataQueue = NULL;
void app_main(void)
{
    // Initialize WiFi in station mode.
    if (wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return;
    }
    
    // Create 1 slot queue
    dataQueue = xQueueCreate(1, sizeof(DashboardData));
    if (dataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create dataQueue");
        return;
    }
    
    // Allow a short delay for WiFi connection to be established.
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Create the ADC task that reads and updates dashboard data.
    xTaskCreate(adc_task, "adcTask", 4096, NULL, 5, NULL);

    // Start sender task depending on selected transport
#if USE_MQTT
    xTaskCreate(mqtt_sender_task, "mqttSenderTask", 4096, NULL, 5, NULL);
#else
    xTaskCreate(udp_sender_task, "udpSenderTask", 4096, NULL, 5, NULL);
#endif

    // Start connectivity monitor to force reconnection when internet drops
    xTaskCreate(connectivity_monitor_task, "connMonitor", 4096, NULL, 5, NULL);
}
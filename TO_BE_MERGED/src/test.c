#include "test.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "adc_reader";
DashboardData dashboardData;
SemaphoreHandle_t dataMutex;

void adc_task(void *pvParameters)
{
    // Configure ADC1: set the resolution and attenuation for the channel.
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    while (1) {
        // Read raw value from ADC, then normalize it.
        int rawVal = adc1_get_raw(ADC_CHANNEL);
        float norm = rawVal / 4095.0f;

        // Map normalized value to dashboard parameters.
        DashboardData newData;
        newData.speed        = norm * 240.0f;                      // 0.0 - 240.0 km/h
        newData.rpm          = (int)(norm * 8000.0f);              // 0 - 8000 rpm
        newData.accPedal     = (int)(norm * 100);                  // 0 - 100 %
        newData.brakePedal   = (int)(norm * 100);                  // 0 - 100 %
        newData.encoder      = -450.0f + norm * 900.0f;            // -450° to +450°
        newData.temperature  = -40.0f + norm * 165.0f;             // -40 to +125 °C
        newData.batteryLevel = (int)(norm * 100);                  // 0 - 100 %

        // Map normalized value to GPS coordinates.
        newData.gpsLongitude = -180.0 + norm * 360.0;              // -180.0° to +180.0°
        newData.gpsLatitude  = -90.0 + norm * 180.0;               // -90.0° to +90.0°

        // Map normalized value to wheel speeds.
        newData.frWheelSpeed = (int)(norm * 200);
        newData.flWheelSpeed = (int)(norm * 200);
        newData.brWheelSpeed = (int)(norm * 200);
        newData.blWheelSpeed = (int)(norm * 200);
        newData.lateralG     = -3.5f + norm * 7.0f;                // -3.5 to +3.5 G
        newData.longitudinalG = -3.5f + norm * 5.5f;               // -3.5 to +2.0 G

        /* 3) Publish latest sample: overwrite the single-slot queue */
        if (dataQueue == NULL ||
            xQueueOverwrite(dataQueue, &newData) != pdPASS)
        {
            ESP_LOGW(TAG, "dataQueue overwrite failed");
        }

        /* 4) Maintain 100 Hz sampling rate */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#ifndef TEST_H
#define TEST_H
//Reads a value of a potentiometer and maps it to the struct, then the data is formatted in udp_sender to be send for testing
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    float speed;          // Speed in km/h (0.0 - 240.0)
    int   rpm;            // Engine RPM (0 - 8000)
    int   accPedal;       // Accelerator pedal position (0 - 100%)
    int   brakePedal;     // Brake pedal position (0 - 100%)
    float encoder;        // Steering encoder angle (-450° to +450°)
    float temperature;    // Temperature in °C (-40 to +125)
    int   batteryLevel;   // Battery level in percentage (0 - 100%)
    double gpsLongitude;  // GPS longitude (-180.0° to +180.0°)
    double gpsLatitude;   // GPS latitude (-90.0° to +90.0°)
    int   frWheelSpeed;   // Front right wheel speed (0 - 200)
    int   flWheelSpeed;   // Front left wheel speed (0 - 200)
    int   brWheelSpeed;   // Back right wheel speed (0 - 200)
    int   blWheelSpeed;   // Back left wheel speed (0 - 200)
    float lateralG;       // Lateral G-force (-3.5 to +3.5)
    float longitudinalG;  // Longitudinal G-force (-3.5 to +2.0)
} DashboardData;

// ADC task that reads the analog value
// and maps the normalized value to various dashboard parameters, and send it to queue.
void adc_task(void *pvParameters);

// Single-item queue carrying DashboardData
extern QueueHandle_t dataQueue;

#endif // ADC_READER_H
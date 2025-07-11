#ifndef CONFIG_H
#define CONFIG_H

// WiFi configuration
#define WIFI_SSID "androidhotspotq"
#define WIFI_PASS "123456788"

// GPIO number for the on-board status LED
#define WIFI_STATUS_LED_GPIO 2

// Back-off delays for Wi-Fi reconnection (in milliseconds)
#define WIFI_RETRY_INITIAL_DELAY_MS 500
#define WIFI_RETRY_MAX_DELAY_MS     10000

// ADC configuration: using ADC1 channel 6 (GPIO34) with 12-bit resolution and 11 dB attenuation.
#define ADC_CHANNEL ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN ADC_ATTEN_DB_11
#define ADC_WIDTH ADC_WIDTH_BIT_12

// UDP configuration: destination IP address and port.
#define SERVER_IP "41.238.164.247"
#define SERVER_PORT 19132

// Transport selection: set to 1 to publish data using MQTT instead of UDP.
#define USE_MQTT 1

#if USE_MQTT
// MQTT configuration
#define MQTT_URI       "mqtts://5aeaff002e7c423299c2d92361292d54.s1.eu.hivemq.cloud:8883"
#define MQTT_USER      "yousef"
#define MQTT_PASS      "Yousef123"
#define MQTT_PUB_TOPIC "com/yousef/esp32/data"

// The HiveMQ Cloud root certificate used for TLS connection
extern const char mqtt_root_ca_pem[];
#endif

// Connectivity check configuration
#define CONNECTIVITY_TEST_IP "8.8.8.8"   // Destination for periodic reachability test
#define CONNECTIVITY_TEST_PORT 53         // DNS port used for test connection
#define CONNECTIVITY_CHECK_INTERVAL_MS 1000
#define CONNECTIVITY_FAIL_THRESHOLD 3     // Force reconnect after this many failures


#endif // CONFIG_H
#ifndef MQTT_SENDER_H
#define MQTT_SENDER_H

#include "freertos/FreeRTOS.h"

void mqtt_sender_task(void *pvParameters);

#endif // MQTT_SENDER_H
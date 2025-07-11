// include/udp_sender.h

#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief UDP sender task.
 *
 * Socket operations are protected by an internal FreeRTOS mutex so this
 * module can be safely used from multiple tasks.
 */
void udp_sender_task(void *pvParameters);

/**
 * @brief Close the internal UDP socket (called on Wi-Fi drop).
 *
 * May be invoked from other tasks such as the Wi-Fi event handler.
 */
void udp_socket_close(void);

#endif // UDP_SENDER_H
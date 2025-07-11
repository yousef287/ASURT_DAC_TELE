#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

/** Task that periodically checks internet reachability and forces a Wi-Fi
 * reconnect when connectivity is lost. */
void connectivity_monitor_task(void *pvParameters);

#endif // CONNECTIVITY_H
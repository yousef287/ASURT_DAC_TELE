# ESP32 UDP Sender

This repository contains a small ESP-IDF / PlatformIO project which reads an analog input on an ESP32 board and periodically sends the values to a remote host using UDP.

The project was originally created for development on the `esp32doit-devkit-v1` board but should work on any ESP32 module.

## Features

- **ADC sampling** – `src/test.c` reads `ADC1_CHANNEL_6` (GPIO34) at 100&nbsp;Hz. The raw value is normalised and mapped to various fields of the `DashboardData` structure. The most recent sample is stored in a single-slot FreeRTOS queue.
- **Wi‑Fi management** – `src/wifi_manager.c` initialises the Wi‑Fi subsystem in station mode, automatically reconnects to the configured network using a back‑off delay and publishes connection status through an event group. The current IP address is stored for retrieval and if the connection or IP is lost the UDP socket is closed.

- **UDP transmission** – `src/udp_sender.c` serialises each `DashboardData` sample into a comma‑separated string and transmits it to a configurable IP address/port. The task waits for Wi‑Fi connectivity, uses exponential back‑off on errors and logs free heap space roughly every 10 seconds.
- **Configurable parameters** – see `include/config.h` for Wi‑Fi credentials, ADC settings and UDP destination.

## Project structure

```
esptest/
├── include/      # Project headers
├── lib/          # Private libraries (currently empty)
├── src/          # Application source files
├── test/         # PlatformIO unit tests (none provided)
├── CMakeLists.txt
├── platformio.ini
└── sdkconfig.esp32doit-devkit-v1
```

Key source files:

- `src/main.c` – application entry point creating the ADC reader and UDP sender tasks after Wi‑Fi is up.
- `src/test.c` – reads the ADC and updates `DashboardData`.
- `src/udp_sender.c` – handles socket creation, formatting the CSV string and sending it via UDP.
- `src/wifi_manager.c` – manages Wi‑Fi connection and notifies other tasks.
- `include/test.h` – definition of the `DashboardData` structure.

## Building and uploading

The project uses PlatformIO with the ESP‑IDF framework. To build and flash the firmware, install [PlatformIO](https://platformio.org/) and run:

```bash
$ pio run
$ pio run --target upload
```

The default serial monitor speed is 115200 baud. Adjust the board type or other settings in `platformio.ini` as needed.

## Configuration

Edit `include/config.h` to customise the Wi‑Fi credentials or UDP destination:

```c
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
#define SERVER_IP "192.0.2.1"       // Destination IP
#define SERVER_PORT 12345           // Destination UDP port
#define WIFI_RETRY_INITIAL_DELAY_MS 500   // Reconnect back-off start
#define WIFI_RETRY_MAX_DELAY_MS     10000 // Maximum back-off
```

## How it works

1. **Startup** – `app_main()` in `src/main.c` initialises non‑volatile storage and Wi‑Fi. Once an IP address is obtained it creates the queue and starts the ADC and UDP tasks.
2. **Sampling** – `adc_task()` continuously reads the configured ADC channel, maps the value to the dashboard fields and overwrites the queue slot. This happens every 10 ms (100 Hz).
3. **Sending data** – `udp_sender_task()` waits for new queue entries. When Wi‑Fi is connected it formats the sample as CSV and sends it with `sendto()`. If sending fails it retries up to four times with exponential delays before dropping the packet.
4. **Reconnection** – if Wi‑Fi disconnects or the IP address is lost, the UDP socket is closed. The Wi‑Fi event handler automatically reconnects with a back‑off delay and the UDP task re‑creates its socket once connectivity is restored. The current IP can be queried via `wifi_get_ip_info()`.


## Testing

The `test/` directory is provided for potential PlatformIO unit tests but no tests are currently implemented.

---

This README gives an overview of the code and build process. Refer to the comments within each source file for additional details.


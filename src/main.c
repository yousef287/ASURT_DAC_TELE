//==================================RTOS Libraries Includes==========================//
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_manager/wifi_manager.h"

#include "logging/logging.h"
#include "driver/twai.h"
#include "RTC_Time_Sync/rtc_time_sync.h"
#include "telemetry_config.h"
#include "connectivity/connectivity.h"
#include "udp_sender/udp_sender.h"
#include "mqtt_sender/mqtt_sender.h"

#define LED_GPIO 2 // GPIO pin for the LED
#define Queue_Size 10

/*
 * ================================================================
 * 							SDIO Config Variables
 * ================================================================
 *
 * */
SDIO_FileConfig SDIO_test;
SDIO_FileConfig SDIO_txt;
SDIO_TxBuffer buffer;

/*
 * ================================================================
 * 							CAN Config Variables
 * ================================================================
 *
 * */
// 1. Configure TWAI with pins from your ESP32 pinout
twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = GPIO_NUM_21, // CAN TX on GPIO21
    .rx_io = GPIO_NUM_22, // CAN RX on GPIO22
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 5,
    .rx_queue_len = 5,
    .alerts_enabled = TWAI_ALERT_ALL,
    .clkout_divider = 0};
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();

twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/*
 * ================================================================
 * 							RTOS Config Variables
 * ================================================================
 *
 * */
// Define Queue Handler
QueueHandle_t CAN_SDIO_queue_Handler;
QueueHandle_t telemetry_queue;

// Define Tasks Handler to hold task ID
TaskHandle_t CAN_Receive_TaskHandler;
TaskHandle_t SDIO_Log_TaskHandler;

// Declare Tasks Entery point
void CAN_Receive_Task_init(void *pvParameters);
void SDIO_Log_Task_init(void *pvParameters);

void app_main()
{
    //==========================================WIFI Implementation (DONE)===========================================
    // ESP_ERROR_CHECK(wifi_init("Mi A2", "min@fathy2004"));
    // ESP_ERROR_CHECK(wifi_init("Belal's A34", "password"));
    ESP_ERROR_CHECK(wifi_init("Fathy WIFI", "Min@F@thy.2004$$"));

    /* Wait until connected */
    xEventGroupWaitBits(wifi_event_group(), WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI("WIFI", "Connected to WiFi!");
    //==========================================RTC_Time_Sync Implementation (DONE)===========================================
    Time_Sync_obtain_time();
    //==========================================SDIO Implementation (DONE)===========================================

    static const char *TAG = "SDIO";
    esp_err_t ret;
    ret = SDIO_SD_Init();

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    
        SDIO_txt.name = "SDIO.TXT";
        SDIO_txt.type = TXT;
        buffer.string = "Hello World line 1\r\nHello World line 2\r\nHello World line 3\r";
        if (SDIO_SD_Create_Write_File(&SDIO_txt, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "SDIO.TXT Written Successfully!");
        }

        SDIO_test.name = "SDIO.CSV";
        SDIO_test.type = CSV;
        buffer.string = "LOG1";
        buffer.timestamp = "2023-10-01 12:00:00";
        buffer.adc.SUS_1 = 15;
        buffer.adc.SUS_2 = 20;
        buffer.adc.SUS_3 = 25;
        buffer.adc.SUS_4 = 30;
        buffer.adc.PRESSURE_1 = 10;
        buffer.adc.PRESSURE_2 = 15;
        buffer.prox_encoder.RPM_front_left = 1000;
        buffer.prox_encoder.RPM_front_right = 1100;
        buffer.prox_encoder.RPM_rear_left = 1200;
        buffer.prox_encoder.RPM_rear_right = 1300;
        buffer.prox_encoder.ENCODER_angle = 45;
        buffer.imu_accel.x = 100;
        buffer.imu_accel.y = 200;
        buffer.imu_accel.z = 300;

        if (SDIO_SD_Create_Write_File(&SDIO_test, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "SDIO.CSV Written Successfully!");
        }

        SDIO_SD_Read_Data(&SDIO_txt);
        SDIO_SD_Read_Data(&SDIO_test);
    /*    // Append data to the existing files
        buffer.string = "Hello World line 4\r\nHello World line 5\r\n";
        if (SDIO_SD_Add_Data(&SDIO_txt, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "SDIO.TXT Appended Successfully!");
        }

        buffer.string = "LOG2";
        buffer.timestamp = "2023-10-01 12:00:01"; // 1 second later

        buffer.adc.SUS_1 = 18;
        buffer.adc.SUS_2 = 22;
        buffer.adc.SUS_3 = 28;
        buffer.adc.SUS_4 = 33;

        buffer.adc.PRESSURE_1 = 12;
        buffer.adc.PRESSURE_2 = 17;

        buffer.prox_encoder.RPM_front_left = 1020;
        buffer.prox_encoder.RPM_front_right = 1120;
        buffer.prox_encoder.RPM_rear_left = 1220;
        buffer.prox_encoder.RPM_rear_right = 1320;
        buffer.prox_encoder.ENCODER_angle = 47;

        buffer.imu.x = 110;
        buffer.imu.y = 210;
        buffer.imu.z = 310;

        buffer.temp.Temp_front_left = 25;
        buffer.temp.Temp_front_right = 26;
        buffer.temp.Temp_rear_left = 27;
        buffer.temp.Temp_rear_right = 28;

        buffer.gps.longitude = 30.123456;
        buffer.gps.latitude = 31.123456;

        if (SDIO_SD_Create_Write_File(&SDIO_test, &buffer) == ESP_OK)
        {
            ESP_LOGI(TAG, "SDIO.CSV Appended Successfully!");
        }

        if (SDIO_SD_Close_file() == ESP_OK)
        {
            ESP_LOGI(TAG, "File Closed Successfully!");
        }

        // Read the files again to verify the appended data
        SDIO_SD_Read_Data(&SDIO_txt);
        SDIO_SD_Read_Data(&SDIO_test);

        twai_message_t rx_msg;
        SDIO_SD_LOG_CAN_Message(&rx_msg);

        // All done, unmount partition and disable SDMMC peripheral
        if(SDIO_SD_DeInit() == ESP_OK)
        ESP_LOGI(TAG, "Card unmounted successfully"); */

    //==========================================CAN Implementation (DONE)===========================================

    // Can Initialization
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully installed TWAI driver");
    }

    if (twai_start() == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully started TWAI driver");
    }

    /*
         twai_message_t tx_msg = {
            .flags = TWAI_MSG_FLAG_NONE,
            .identifier = 0x123,
            .data_length_code = 8,
            .data = {1,2,3,4,5,6,7,8}
        };
        // 3. Transmit a message
        twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));  */

    //==========================================RTOS Implementation (Semaphore can be added)===========================================

    //=======================Create Queue====================//

    telemetry_queue = xQueueCreate(Queue_Size, sizeof(twai_message_t));
    CAN_SDIO_queue_Handler = xQueueCreate(Queue_Size, sizeof(twai_message_t));

    if (telemetry_queue == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (CAN_SDIO_queue_Handler == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //=============Define Tasks=================//
    xTaskCreate((TaskFunction_t)SDIO_Log_Task_init, "SDIO_Log_Task", 4096, NULL, (UBaseType_t)4, &SDIO_Log_TaskHandler);
    xTaskCreate((TaskFunction_t)CAN_Receive_Task_init, "CAN_Receive_Task", 4096, NULL, (UBaseType_t)4, &CAN_Receive_TaskHandler);
#if USE_MQTT
    xTaskCreate(mqtt_sender_task, "mqtt_sender", 4096, telemetry_queue, 4, NULL);
#else
    xTaskCreate(udp_sender_task, "udp_sender", 4096, telemetry_queue, 4, NULL);
#endif
    xTaskCreate(connectivity_monitor_task, "conn_monitor", 4096, NULL, 4, NULL);

    while (1)
    {
        /*
                char time_buffer[32];
                if (Time_Sync_get_rtc_time_str(time_buffer, sizeof(time_buffer)))
                {
                    printf("Current time: %s\n", time_buffer);
                }
                else
                {
                    printf("Failed to get time.\n");
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
         */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CAN_Receive_Task_init(void *pvParameters) // DONE
{
    const char *TAG = "CAN_Receive_Task";
    twai_message_t rx_msg;
    esp_err_t ret;
    uint32_t alerts = 0;
    twai_status_info_t s;
    ESP_LOGI("CAN_Receive_Task", "CAN IS WORKING");
    while (1)
    {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1000)) == ESP_OK)
        {
            // Process rx_msg->identifier, rx_msg->data, etc.

            /* printf("ID = 0x%03lX\n",rx_msg.identifier);
            printf("Extended? %s\n", rx_msg.extd ? "Yes" : "No");
            printf("RTR? %s\n", rx_msg.rtr ? "Yes" : "No");
            printf("DLC = %d\n", rx_msg.data_length_code);
            for (int i = 0; i < rx_msg.data_length_code; i++) {
                printf("byte[%d] = 0x%02X\n", i, rx_msg.data[i]);
            }
*/
            // Format the message into the string buffer
            xQueueSend(telemetry_queue, &rx_msg, (TickType_t)10);

            if (xQueueSend(CAN_SDIO_queue_Handler, &rx_msg, (TickType_t)10) != pdPASS)
            {
                if (SDIO_Log_TaskHandler != NULL)
                {
                    xTaskNotifyGive(SDIO_Log_TaskHandler); // Notify SDIO task
                }
            }
        }
        else
        {
            ESP_LOGI(TAG, "No message received within the timeout period");
            ret = twai_read_alerts(&alerts, 0);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "TWAI alert: %08ld", alerts);
            }
            twai_get_status_info(&s);
            ESP_LOGI(TAG, "RX errors: %ld, bus errors: %ld, RX queue full: %ld",
                     s.rx_error_counter, s.bus_error_count, s.rx_missed_count);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void SDIO_Log_Task_init(void *pvParameters) // WORKS! but Need Integration with sensor format
{                                           // Add filter for ID

    const char *TAG = "SDIO_Log_Task";
    ESP_LOGI(TAG, "SDO_LOG IS WORKING");
    char name_buffer[30] = "LOG_Session.CSV"; //Temp Logging file name

    SDIO_FileConfig LOG_CSV;
    LOG_CSV.name = name_buffer;
    LOG_CSV.type = CSV;
    SDIO_TxBuffer SDIO_buffer;

    snprintf(LOG_CSV.path, sizeof(LOG_CSV.path), "%s/%s", MOUNT_POINT, LOG_CSV.name);

    // Check if the files exists and Modification Time more than 2 days
    //if it exists and last modified was more than 2 days
    //      Increment the name of the file and check again
    // if it exists and last modified in less than 2 days           |
    //      Don't change name and add to the already existing file  |   This logic is implemented
    // if it doesn't exist                                          |   SDIO_SD_Create_Write_File()
    //      Create file                                             |
    
    struct stat st;
    uint8_t Session_Num = 0;
    while((stat(LOG_CSV.path, &st) == 0) && (compare_file_time_days(LOG_CSV.path) > 2))
    {
        //it exists and last modified was more than 2 days
        Session_Num++;
        //Update Name and path
        snprintf(LOG_CSV.name, sizeof(name_buffer), "LOG_Session_%u.CSV", Session_Num);
        snprintf(LOG_CSV.path, sizeof(LOG_CSV.path), "%s/%s", MOUNT_POINT, LOG_CSV.name);
    }


    twai_message_t buffer;
    const uint8_t NUM_IDS = COMM_CAN_ID_COUNT;
    uint8_t id_received[NUM_IDS];
    TickType_t period = pdMS_TO_TICKS(50);

    while (1)
    {
        // Wait for notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 1. Clear buffer and flags
        memset(&SDIO_buffer, 0, sizeof(SDIO_buffer));
        memset(&buffer, 0, sizeof(buffer));

        TickType_t start = xTaskGetTickCount();
        TickType_t now = start;
        uint8_t received_count = 0;

        while ((now - start) < period && received_count < NUM_IDS)
        {
            TickType_t remaining = period - (now - start);
            if (xQueueReceive(CAN_SDIO_queue_Handler, &buffer, remaining))
            {
                // To print the Message Received
                /* printf("ID = 0x%03lX ", buffer.identifier);
                printf("Extended? %s ", buffer.extd ? "Yes" : "No");
                printf("RTR? %s ", buffer.rtr ? "Yes" : "No");
                printf("DLC = %d\n", buffer.data_length_code);
                for (int i = 0; i < buffer.data_length_code; i++)
                {
                    printf("byte[%d] = 0x%02X ", i, buffer.data[i]);
                }
                printf("\n");
                 */
                switch (buffer.identifier) // Check the ID of the message
                {
                case COMM_CAN_ID_IMU_ANGLE:
                    if (id_received[COMM_CAN_ID_IMU_ANGLE - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.imu_ang = *((COMM_message_IMU_t *)buffer.data);
                        id_received[COMM_CAN_ID_IMU_ANGLE - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_IMU_ACCEL:
                    if (id_received[COMM_CAN_ID_IMU_ACCEL - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.imu_accel = *((COMM_message_IMU_t *)buffer.data);
                        id_received[COMM_CAN_ID_IMU_ACCEL - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_ADC:
                    if (id_received[COMM_CAN_ID_ADC - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.adc = *((COMM_message_ADC_t *)buffer.data);
                        id_received[COMM_CAN_ID_ADC - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_PROX_ENCODER:
                    if (id_received[COMM_CAN_ID_PROX_ENCODER - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.prox_encoder = *((COMM_message_PROX_encoder_t *)buffer.data);
                        id_received[COMM_CAN_ID_PROX_ENCODER - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_GPS_LATLONG:
                    if (id_received[COMM_CAN_ID_GPS_LATLONG - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.gps = *((COMM_message_GPS_t *)buffer.data);
                        id_received[COMM_CAN_ID_GPS_LATLONG - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                case COMM_CAN_ID_TEMP:
                    if (id_received[COMM_CAN_ID_TEMP - COMM_CAN_ID_FISRT] == 0)
                    {
                        SDIO_buffer.temp = *((COMM_message_Temp_t *)buffer.data);
                        id_received[COMM_CAN_ID_TEMP - COMM_CAN_ID_FISRT] = 1;
                        received_count++;
                    }
                    break;

                default:
                    break;
                }
            }
            now = xTaskGetTickCount();
        }

        //Log the Buffer on SD card
        if(SDIO_SD_Create_Write_File(&LOG_CSV, &SDIO_buffer) != ESP_OK)
        {
            ESP_LOGI(TAG, "ERROR! : %s is not Created // Appedended", LOG_CSV.name);
        }
    }
}

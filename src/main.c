

//==================================RTOS Libraries Includes==========================//
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_manager/wifi_manager.h"

#include "logging/logging.h"
#include "driver/twai.h"
#include "RTC_Time_Sync/rtc_time_sync.h"

#define LED_GPIO    2 // GPIO pin for the LED
#define Queue_Size  10

int test = 0;
static const char *TAG = "SDIO";


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

twai_filter_config_t f_config = {
    .acceptance_code = (0x22 << 21),   // shift standard ID into appropriate bit position
    .acceptance_mask = ~(0x7FF << 21), // mask to match only exact 11-bit ID 0x20
    .single_filter = true};

/*
 * ================================================================
 * 							RTOS Config Variables
 * ================================================================
 *
 * */
// Define Queue Handler
QueueHandle_t CAN_SDIO_queue_Handler;
QueueHandle_t CAN_TELE_queue_Handler;

// Define Tasks Handler to hold task ID
TaskHandle_t CAN_Receive_TaskHandler;
TaskHandle_t SDIO_Log_TaskHandler;
TaskHandle_t TELE_Log_TaskHandler;

// Declare Tasks Entery point
void CAN_Receive_Task_init(void *pvParameters);
void SDIO_Log_Task_init(void *pvParameters);
void TELE_Log_Task_init(void *pvParameters);

void app_main()
{
    //==========================================WIFI Implementation (DONE)===========================================
    ESP_ERROR_CHECK(wifi_init("Fathy WIFI", "Min@F@thy.2004$$"));

    /* Wait until connected */
    xEventGroupWaitBits(wifi_event_group(), WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI("WIFI", "Connected to WiFi!");

    //==========================================CAN Implementation (DONE)===========================================

    // Can Initialization
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully installed TWAI driver");
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (twai_start() == ESP_OK)
    {
        ESP_LOGE("CAN", "successfully started TWAI driver");
    }

    esp_err_t ret;
    /*
         twai_message_t tx_msg = {
            .flags = TWAI_MSG_FLAG_NONE,
            .identifier = 0x123,
            .data_length_code = 8,
            .data = {1,2,3,4,5,6,7,8}
        };
        // 3. Transmit a message
        twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));  */

    //==========================================RTOS Implementation (Working)===========================================

    //=======================Create Queue====================//

    CAN_TELE_queue_Handler = xQueueCreate(Queue_Size, sizeof(twai_message_t));
    CAN_SDIO_queue_Handler = xQueueCreate(Queue_Size, sizeof(twai_message_t));

    if (CAN_TELE_queue_Handler == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else // If there is  queue created
    {
        ESP_LOGI("RTOS", "Structure Queue Created Successfully\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (CAN_SDIO_queue_Handler == NULL) // If there is no queue created
    {
        ESP_LOGE("RTOS", "Unable to Create Structure Queue");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else // If there is  queue created
    {
        ESP_LOGI("RTOS", "Structure Queue Created Successfully");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //=============Define Tasks=================//
    xTaskCreate((TaskFunction_t)CAN_Receive_Task_init, "CAN_Receive_Task", 4096, NULL, (UBaseType_t)4, &CAN_Receive_TaskHandler);
    //xTaskCreate((TaskFunction_t)SDIO_Log_Task_init, "SDIO_Log_Task", 1024, NULL, (UBaseType_t)4, &SDIO_Log_TaskHandler);
    //xTaskCreate((TaskFunction_t)TELE_Log_Task_init, "TELE_Log_Task", 1024, NULL, (UBaseType_t)4, &TELE_Log_TaskHandler);

    //==========================================SDIO Implementation (DONE)===========================================
    /*
       ret = SDIO_SD_Init();

        if (ret != ESP_OK) {
           if (ret == ESP_FAIL) {
               ESP_LOGE(TAG, "Failed to mount filesystem. "
                        "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
           } else {
               ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                        "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
           }
           return;
       }
       ESP_LOGI(TAG, "Filesystem mounted");


       SDIO_txt.name = "SDIO.TXT";
       SDIO_txt.type = TXT;
       buffer.string = "Hello World line 1\r\nHello World line 2\r\nHello World line 3\r";
       if(SDIO_SD_Create_Write_File(&SDIO_txt, &buffer) == ESP_OK)
       {
           ESP_LOGI(TAG, "SDIO.TXT Written Successfully!");
       }


       SDIO_test.name = "SDIO.CSV";
       SDIO_test.type = CSV;
       buffer.string = "LOG1";
       buffer.timestamp = "2023-10-01 12:00:00";
       buffer.adc.SUS_1= 15;
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
       buffer.imu.x = 100;
       buffer.imu.y = 200;
       buffer.imu.z = 300;

       if(SDIO_SD_Create_Write_File(&SDIO_test, &buffer) == ESP_OK)
       {
           ESP_LOGI(TAG, "SDIO.CSV Written Successfully!");
       }

       SDIO_SD_Read_Data(&SDIO_txt);
       SDIO_SD_Read_Data(&SDIO_test);
       //Append data to the existing files
       buffer.string = "Hello World line 4\r\nHello World line 5\r\n";
       if(SDIO_SD_Add_Data(&SDIO_txt, &buffer) == ESP_OK)
       {
           ESP_LOGI(TAG, "SDIO.TXT Appended Successfully!");
       }

       buffer.string = "LOG2";
       buffer.timestamp = "2023-10-01 12:00:01";  // 1 second later

       buffer.adc.SUS_1 = 18;
       buffer.adc.SUS_2 = 22;
       buffer.adc.SUS_3 = 28;
       buffer.adc.SUS_4 = 33;

       buffer.adc.PRESSURE_1 = 12;
       buffer.adc.PRESSURE_2 = 17;

       buffer.prox_encoder.RPM_front_left  = 1020;
       buffer.prox_encoder.RPM_front_right = 1120;
       buffer.prox_encoder.RPM_rear_left   = 1220;
       buffer.prox_encoder.RPM_rear_right  = 1320;
       buffer.prox_encoder.ENCODER_angle   = 47;

       buffer.imu.x = 110;
       buffer.imu.y = 210;
       buffer.imu.z = 310;

       if(SDIO_SD_Add_Data(&SDIO_test, &buffer) == ESP_OK)
       {
           ESP_LOGI(TAG, "SDIO.CSV Appended Successfully!");
       }

       if(SDIO_SD_Close_file() == ESP_OK)
       {
           ESP_LOGI(TAG, "File Closed Successfully!");
       }


       // Read the files again to verify the appended data
       SDIO_SD_Read_Data(&SDIO_txt);
       SDIO_SD_Read_Data(&SDIO_test);

       twai_message_t rx_msg;
       SDIO_SD_LOG_CAN_Message(&SDIO_test, &rx_msg);

       // All done, unmount partition and disable SDMMC peripheral
       if(SDIO_SD_DeInit() == ESP_OK)
        ESP_LOGI(TAG, "Card unmounted successfully");


    */
    //==========================================RTC_Time_Sync Implementation (IN Progress)===========================================
    Time_Sync_obtain_time();
    char time_buffer[32];
    while (1)
    {
        /* ESP_LOGI("ESP Check", "I am Alive :)");
        vTaskDelay(1000 / portTICK_PERIOD_MS);     */

        if (Time_Sync_get_rtc_time_str(time_buffer, sizeof(time_buffer)))
        {
            printf("Current time: %s\n", time_buffer);
        }
        else
        {
            printf("Failed to get time.\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

        /*   twai_message_t tx_msg = {
        .flags = TWAI_MSG_FLAG_NONE,
        .identifier = 0x123,
        .data_length_code = 8,
        .data = {1,2,3,4,5,6,7,8}
        };
        // 3. Transmit a message


        esp_err_t tx_ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));
         if (tx_ret != ESP_OK) {
        ESP_LOGE(TAG, "Transmit failed: %s", esp_err_to_name(tx_ret));
        }
        */
    }
}

void CAN_Receive_Task_init(void *pvParameters)
{
    const char *TAG = "CAN_Receive_Task";
    twai_message_t rx_msg;
    esp_err_t ret;
    uint32_t alerts = 0;
    twai_status_info_t s;
    twai_message_t buffer;
    ESP_LOGI("CAN_Receive_Task", "CAN IS WORKING");
    while (1)
    {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1000)) == ESP_OK)
        {
            // Process rx_msg->identifier, rx_msg->data, etc.

            /*  printf("ID = 0x%03lX\n",rx_msg->identifier);
             printf("Extended? %s\n", rx_msg->extd ? "Yes" : "No");
             printf("RTR? %s\n", rx_msg->rtr ? "Yes" : "No");
             printf("DLC = %d\n", rx_msg->data_length_code);
             for (int i = 0; i < rx_msg->data_length_code; i++) {
                 printf("byte[%d] = 0x%02X\n", i, rx_msg->data[i]);
             } */

            // Format the message into the string buffer
            if (xQueueSend(CAN_TELE_queue_Handler, &rx_msg, (TickType_t)10) != pdPASS)
            {
                ESP_LOGE(TAG, "Error ! - Queue IS FULL !!");
                for (uint8_t i = 0; i < Queue_Size; i++)
                {
                    if (xQueueReceive(CAN_TELE_queue_Handler, &buffer, (TickType_t)10))
                    {
                        ESP_LOGI(TAG, "Message No: %u", i);
                        printf("ID = 0x%03lX\n", buffer.identifier);
                        printf("Extended? %s\n", buffer.extd ? "Yes" : "No");
                        printf("RTR? %s\n", buffer.rtr ? "Yes" : "No");
                        printf("DLC = %d\n", buffer.data_length_code);
                        for (int i = 0; i < buffer.data_length_code; i++)
                        {
                            printf("byte[%d] = 0x%02X\n", i, buffer.data[i]);
                        }
                    }
                    
                }
            }
        }
        else
        {
            ESP_LOGI("CAN", "No message received within the timeout period");
            ret = twai_read_alerts(&alerts, 0);
            if (ret == ESP_OK)
            {
                ESP_LOGI("CAN", "TWAI alert: %08ld", alerts);
            }
            twai_get_status_info(&s);
            ESP_LOGI("CAN", "RX errors: %ld, bus errors: %ld, RX queue full: %ld",
                     s.rx_error_counter, s.bus_error_count, s.rx_missed_count);

        }
        vTaskDelay(1);
    }
}
void SDIO_Log_Task_init(void *pvParameters)
{
    const char *TAG = "SDIO_Log_Task";
    while (1)
    {
        ESP_LOGI(TAG, "SDO_LOG IS WORKING");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void TELE_Log_Task_init(void *pvParameters)
{
     const char *TAG = "TELE_Log_Task";
    while (1)
    {
        ESP_LOGI(TAG, "TELE_LOG IS WORKING");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

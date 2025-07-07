/*
 * Logging.h
 *
 *  Created on: Jun 22, 2025
 *      Author: Mina Fathy
 */
#ifndef LOGGING_H
#define LOGGING_H

//==================================Standard Libraries Includes=======================//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//==================================ESP32 Libraries Includes==========================//
#include <driver/gpio.h>
#include <esp_log.h>

#include <driver/sdmmc_host.h>
#include "sdmmc_cmd.h"

#include "driver/twai.h"

//==================================Status Libraries Includes==========================//
#include <sys/unistd.h>
#include <sys/stat.h>

//===================================FATFS Includes===================================//
#include "esp_vfs_fat.h"



//----------------------------
//SDIO Macros
//----------------------------
#define CMD     15
#define CLK     14
#define D0      2
#define D1      4
#define D2      12
#define D3      13

#define SDMMC_BUS_WIDTH_4

#define MAX_CHAR_SIZE  				64
#define EXAMPLE_MAX_CHAR_SIZE    	64
#define MOUNT_POINT 				"/sdcard"
#define EXAMPLE_IS_UHS1   			 (CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50 || CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50)


//===============================================
// User type definitions (structures)
//===============================================

//----------------------------
//Sensor Readings Structures
//----------------------------

//ADC Readings Structures
typedef struct
{
	uint64_t SUS_1 : 10;
	uint64_t SUS_2 : 10;
	uint64_t SUS_3 : 10;
	uint64_t SUS_4 : 10;

	uint64_t PRESSURE_1 : 10;
	uint64_t PRESSURE_2 : 10;

} COMM_message_ADC_t;

//----------------------------

//Proximity Encoder Readings Structures
typedef struct 
{
	uint64_t RPM_front_left  : 12;
	uint64_t RPM_front_right : 12;
	uint64_t RPM_rear_left 	 : 12;
	uint64_t RPM_rear_right  : 12;

	uint64_t ENCODER_angle	 : 10;

} COMM_message_PROX_encoder_t;

//----------------------------

//IMU Readings Structures
typedef struct 
{
	uint16_t x;		/* X-axis Value */
	uint16_t y;		/* Y-axis Value */
	uint16_t z;		/* Z-axis Value */
} COMM_message_IMU_t;


//----------------------------
//SDIO File Configuration Structures
//----------------------------
typedef struct
{
	char *name;			//Specifies the File name to be configured.

	char path[30];		//Specifies the path where the file will be created.

	uint8_t type;		//Specifies the file type to be configured.
						//This parameter must be based on @ref SDIO_File_Types

}SDIO_FileConfig;

//----------------------------
//SD Communication Buffer Structures
typedef struct
{
	const char *string;			//String to be Stored in File
								//Used mostly in .TXT files and First column in .CSV files

	const char *timestamp;		//Timestamp of the Readings to be stored

	COMM_message_ADC_t adc;		//ADC Readings to be stored
								//Used mostly in .CSV files

	COMM_message_PROX_encoder_t prox_encoder;	//Proximity Encoder Readings to be stored
												//Used mostly in .CSV files
	
	COMM_message_IMU_t imu;		//IMU Readings to be stored
								//Used mostly in .CSV files



}SDIO_TxBuffer;

//----------------------------
//Macros Configuration References
//---------------------------

//@ref SDIO_File_Types
#define CSV		0
#define TXT 	1

//===============================================
// APIs Supported by "LOGGING DRIVER"
//===============================================

esp_err_t SDIO_SD_Init(void);
esp_err_t SDIO_SD_DeInit(void);
esp_err_t SDIO_SD_Create_Write_File(SDIO_FileConfig *file, SDIO_TxBuffer *pTxBuffer);
esp_err_t SDIO_SD_Add_Data(SDIO_FileConfig *file, SDIO_TxBuffer *pTxBuffer);
esp_err_t SDIO_SD_Read_Data(SDIO_FileConfig *file);
esp_err_t SDIO_SD_Close_file(void);
esp_err_t SDIO_SD_LOG_CAN_Message(SDIO_FileConfig *file, twai_message_t *rx_msg);

#endif
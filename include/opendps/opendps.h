/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Lasse K. Mikkelsen (github.com/lkmikkel)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* 
 * This module defines the serial interface protocol for opendps.
 * Command  (TX):  <start of frame> <command> [<payload>]* <crc16 msb> <crc16 lsb> <end of frame>
 * Response (RX):  <start of frame> <command response> <success> [<response data>]* <crc16 msb> <crc16 lsb> <end of frame>
 */

#ifndef __LIB_OPENDPS_H__
#define __LIB_OPENDPS_H__

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <float.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define INPUT_BUFFER_SIZE 128
#define OUTPUT_BUFFER_SIZE 20
#define MAX_RETRY 3

// OPENDPS protocol

#define _SOF 0x7e
#define _DLE 0x7d
#define _XOR 0x20
#define _EOF 0x7f

// OpenDPS commands
static const __uint8_t CMD_PING				= 0x01;
//static const __uint8_t CMD_SET_VOUT 			= 0x02; // obsolete
//static const __uint8_t CMD_SET_ILIMIT 		= 0x03; // obsolete
static const __uint8_t CMD_QUERY			= 0x04;
//const __uint8_t CMD_POWER_ENABLE 			= 0x05; // obsolete
static const __uint8_t CMD_WIFI_STATUS 			= 0x06;
static const __uint8_t CMD_LOCK 			= 0x07;
static const __uint8_t CMD_OCP_EVENT 			= 0x08;
static const __uint8_t CMD_UPGRADE_START 		= 0x09;
static const __uint8_t CMD_UPGRADE_DATA 		= 0x0a;
static const __uint8_t CMD_SET_FUNCTION 		= 0x0b;
static const __uint8_t CMD_ENABLE_OUTPUT 		= 0x0c;
static const __uint8_t CMD_LIST_FUNCTIONS 		= 0x0d;
static const __uint8_t CMD_SET_PARAMETERS 		= 0x0e;
static const __uint8_t CMD_LIST_PARAMETERS 		= 0x0f;
static const __uint8_t CMD_TEMPERATURE_REPORT 		= 0x10;
static const __uint8_t CMD_VERSION 			= 0x11;
static const __uint8_t CMD_CAL_REPORT 			= 0x12;
static const __uint8_t CMD_SET_CALIBRATION 		= 0x13;
static const __uint8_t CMD_CLEAR_CALIBRATION 		= 0x14;
static const __uint8_t CMD_CHANGE_SCREEN 		= 0x15;
static const __uint8_t CMD_SET_BRIGHTNESS 		= 0x16;
static const __uint8_t CMD_RESPONSE 			= 0x80;

static const __uint8_t CMD_STATUS_SUCC			= 0x01;

// Upgrade status                                                                                                                                                                                              
static const __uint8_t UPGRADE_CONTINUE 		= 0;
static const __uint8_t UPGRADE_BOOTCOM_ERROR 		= 1;
static const __uint8_t UPGRADE_CRC_ERROR 		= 2;
static const __uint8_t UPGRADE_ERASE_ERROR 		= 3;
static const __uint8_t UPGRADE_FLASH_ERROR 		= 4;
static const __uint8_t UPGRADE_OVERFLOW_ERROR 		= 5;
static const __uint8_t UPGRADE_SUCCESS 			= 16;

// options for cmd_change_screen                                                                                                                                                                               
static const __uint8_t SCREEN_MAIN 			= 0;
static const __uint8_t SCREEN_SETTINGS 			= 1;

typedef void (*cb_upgrade_progress) (__uint8_t);

typedef struct query_t {
	bool temp_shutdown;
	bool output_enabled;
	__uint16_t v_in;
	__uint16_t v_out;
	__uint16_t i_out;
	double temp1;
	double temp2;
} dps_query_t;

typedef struct version_t {                                                                                                                                                                                     
        char *bootloader_ver;                                                                                                                                                                                  
        char *firmware_ver;
} dps_version_t;

static const unsigned short crc16tab[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

int dps_init(const char *serial_device, int baud_rate, bool pverbose);
int dps_ping();
int dps_lock(bool enable);
int dps_brightness(int brightness);
int dps_power(bool poweron);
int dps_voltage(int millivol);
int dps_current(int milliamp);
int dps_query(dps_query_t *result);
int dps_change_screen(__uint8_t screen);
int dps_version(dps_version_t *version);
int dps_upgrade(char *fw_file_name, cb_upgrade_progress progress);

#ifdef __cplusplus
}
#endif

#endif //__LIB_OPENDPS_H__

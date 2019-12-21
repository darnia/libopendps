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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "opendps/opendps.h"

// argument

void print_usage(char *program)
{
	fprintf(stderr, "Usage: %s [-v] [-i] [-d device] [-b baudrate] [-B brightness] [-c current] [-V voltage] <-l | -L | -o | -O -p>\n", program);
}

int main(int argc, char *argv[])
{
	char *serial_device = "/dev/ttyUSB0";
	int baudrate = B115200;
	int lcd_brightness = -1;
	bool verbose = false;
	bool c_lock = false;
	bool c_unlock = false;
	bool c_ping = false;
	bool c_power_on = false;
	bool c_power_off = false;
	bool c_query = false;
	bool c_help = false;
	bool c_upgrade = false;
	bool c_version = false;
	int voltage = -1;
	int current = -1;
	int opt;

	while ((opt = getopt(argc, argv, "B:b:c:d:hilLoOpqvV:U")) != -1) {
		switch(opt) {
			case 'B':
				lcd_brightness = atoi(optarg);
				break;
			case 'b':
				baudrate = atoi(optarg);
				break;
			case 'c':
				current = atoi(optarg);
				break;
			case 'd':
				serial_device = optarg;
				break;
			case 'h':
				c_help = true;
				break;
			case 'i':
				c_version = true;
				break;
			case 'l':
				c_unlock = true;
				break;
			case 'L':
				c_lock = true;
				break;
			case 'o':
				c_power_off = true;
				break;
			case 'O':
				c_power_on = true;
				break;
			case 'p':
				c_ping = true;
				break;
			case 'q':
				c_query = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'V':
				voltage = atoi(optarg);
				break;
			case 'U':
				c_upgrade = true;
				break;
			default: 
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	int rc = dps_init(serial_device, baudrate, verbose);
	if (rc < 0)
		return rc;

        if (c_version) {
                dps_version_t version;
                rc = dps_version(&version);
                if (rc == 0) {
                        printf("Boot version : %s\n", version.bootloader_ver);
                        printf("App version  : %s\n", version.firmware_ver);
                        free(version.bootloader_ver);
                        free(version.firmware_ver);
                } else {
                        printf("Failed to get versions from DPS\n");
                }
        }

	if (c_ping) {
		rc = dps_ping();
		if (rc == 0) {
			printf("Ping... OK\n");
		} else {
			printf("Ping... Failed (Error: %s)\n", "");
		}
	}

	if (c_unlock) {
		printf("DPS %s\n", dps_lock(false) == 0 ? "unlocked" : "failed to unlock");
	}

	if (lcd_brightness >= 0) {
		rc = dps_brightness(lcd_brightness);
		if (rc == 0)
			printf("Brightness set to %d\n", lcd_brightness);
		else
			printf("Setting brightness failed\n");
	}

	if (voltage >= 0) {
		if (dps_voltage(voltage) == 0)
			printf("Voltage set to: %d mV\n", voltage);
	}

	if (current >= 0) {
		if (dps_current(current) == 0)
			printf("Current set to: %d mA\n", current);
	}

	if (c_power_on) {
		if (dps_power(true) == 0)
			printf("Power output ON\n");
	}

	if (c_power_off) {
		if (dps_power(false) == 0)
			printf("Power output OFF\n");
	}

	if (c_lock) {
		printf("DPS %s\n", dps_lock(true) == 0 ? "locked" : "failed to lock");
	}

	if (c_query) {
		query_t status;	
		if (dps_query(&status) == 0) {
			printf("Status\n");
			printf("Input voltage : %.2f\n", (double) status.v_in / 1000);
			printf("Output voltage: %.2f\n", (double) status.v_out / 1000);
			printf("Output current: %.3f\n", (double) status.i_out / 1000);
			printf("Output        : %s\n"  , (status.output_enabled ? (status.temp_shutdown ? "temperature shutdown" : "ON") : "OFF"));
			if (status.temp1 != -DBL_MAX)
				printf("Temperature 1 : %.1f\n", status.temp1);
			if (status.temp2 != -DBL_MAX)
				printf("Temperature 2 : %.1f\n", status.temp2);
		}
	}

	if (c_upgrade) {
		//TODO: implement
	}

	if (c_help) {
		print_usage(argv[0]);
	}
}

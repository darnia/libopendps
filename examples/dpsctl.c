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
#include "opendps.h"

// argument

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
	bool c_version = false;
	int voltage = -1;
	int current = -1;
	int opt;

	while ((opt = getopt(argc, argv, "B:b:C:d:DhlLoOpqvV:")) != -1)
	{
		switch (opt)
		{
		case 'B':
			lcd_brightness = atoi(optarg);
			break;
		case 'b':
			baudrate = atoi(optarg);
			break;
		case 'C':
			current = atoi(optarg);
			break;
		case 'd':
			serial_device = optarg;
			break;
		case 'D':
			verbose = true;
			break;
		case 'h':
			c_help = true;
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
			c_version = true;
			break;
		case 'V':
			voltage = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage: %s [-v] [-d device] [-b baudrate] [-B brightness] <-l | -L | -p>\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	int rc = dps_init(serial_device, baudrate, verbose);
	if (rc < 0)
		return rc;

	if (c_help)
	{
		rc = dps_version();

		return rc;
	}

	if (c_ping)
	{
		rc = dps_ping();
		if (rc == 0)
		{
			printf("Ping... OK\n");
		}
		else
		{
			printf("Ping... Failed (Error: %s)\n", "");
		}
	}

	if (c_unlock)
	{
		printf("DPS %s\n", dps_lock(false) ? "unlocked" : "failed to unlock");
	}

	if (lcd_brightness >= 0)
	{
		rc = dps_brightness(lcd_brightness);
		if (rc)
			printf("Brightness set to %d\n", lcd_brightness);
	}

	if (voltage >= 0)
	{
		dps_voltage(voltage);
	}

	if (current >= 0)
	{
		dps_current(current);
	}

	if (c_power_on)
	{
		rc = dps_power(true);
		if (rc)
			printf("Power output ON\n");
	}

	if (c_power_off)
	{
		rc = dps_power(false);
		if (rc)
			printf("Power output OFF\n");
	}

	if (c_lock)
	{
		printf("DPS %s\n", dps_lock(true) ? "locked" : "failed to lock");
	}

	if (c_query)
	{
		rc = dps_query();
		if (rc)
		{
			printf("Status\n");
		}
	}

	if (c_version)
	{
		rc = dps_version();
		if (rc)
		{
			printf("Boot version: %s\n");
			printf("App version: %s\n");
		}
	}
}

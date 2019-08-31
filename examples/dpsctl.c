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
	int voltage = -1;
	int current = -1;
	int opt;

	while ((opt = getopt(argc, argv, "B:b:c:d:hlLoOpqvV:")) != -1) {
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
			default: 
				fprintf(stderr, "Usage: %s [-v] [-d device] [-b baudrate] [-B brightness] <-l | -L | -p>\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	int rc = dps_init(serial_device, baudrate, verbose);
	if (rc < 0)	
		return rc;
	

	if (c_help) {
		rc = dps_version();

		return rc;
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
		printf("DPS %s\n", dps_lock(false) ? "unlocked" : "failed to unlock");
	}

	if (lcd_brightness >= 0) {
		rc = dps_brightness(lcd_brightness);
		if (rc >= 0)
			printf("Brightness set to %d\n", lcd_brightness);
	}

	if (voltage >= 0) {
		dps_voltage(voltage);
	}

	if (current >= 0) {
		dps_current(current);
	}

	if (c_power_on) {
		rc = dps_power(true);
		if (rc)
			printf("Power output ON\n");
	}

	if (c_power_off) {
		rc = dps_power(false);
		if (rc)
			printf("Power output OFF\n");
	}

	if (c_lock) {
		printf("DPS %s\n", dps_lock(true) ? "locked" : "failed to lock");
	}

	if (c_query) {
		query_t status;
		rc = dps_query(&status);
		if (rc > 0) {
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
}

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

#include "opendps/opendps.h"

static bool verbose = false;
static int fd = -1;

int set_serial_attribs(int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("Error from tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;		 /* 8-bit characters */
	tty.c_cflag &= ~PARENB;  /* no parity bit */
	tty.c_cflag &= ~CSTOPB;  /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS; /* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1; // 0.1 sec.

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tcflush(fd, TCIOFLUSH); // flush input and output buffers before use

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		printf("Error from tcsetattr: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int get_baud(int baudrate)
{
	switch (baudrate)
	{
	case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	default:
		return B115200;
	}
}

int dps_init(const char *serial_device, int baud_rate, bool pverbose)
{
	verbose = pverbose;

	fd = open(serial_device, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0)
	{
		printf("Error opening %s: %s\n", serial_device, strerror(errno));
		return fd;
	}

	return set_serial_attribs(get_baud(baud_rate));
}

unsigned short crc16_ccitt(const void *buf, int len)
{
	register int counter;
	register unsigned short crc = 0;
	for (counter = 0; counter < len; counter++)
		crc = (crc << 8) ^ crc16tab[((crc >> 8) ^ *(char *)buf++) & 0x00FF];
	return crc;
}

unsigned short calc_crc_file(char *filename)
{
	__uint8_t buffer[65536];
	unsigned short result = 0;
	FILE *file = fopen(filename, "r");
	if (file != NULL) {
		size_t read = fread(&buffer, sizeof(__uint8_t), sizeof(buffer), file);
		if (read > 0)
			result = crc16_ccitt(&buffer, read);
		if (verbose)
			printf("File: %s, size: %d, CRC: %2.2x %2.2x\n", filename, read, (result >> 8), (result &  0xff));
		fclose(file);
	}
	return result;
}

void pack8(__uint8_t data, void *buf, int *idx)
{
	if (data == _SOF || data == _DLE || data == _EOF)
	{
		*(__uint8_t *)(buf + (*idx)++) = _DLE;
		*(__uint8_t *)(buf + (*idx)++) = data ^ _XOR;
	}
	else
	{
		*(__uint8_t *)(buf + (*idx)++) = data;
	}
}

void pack16(__uint16_t data, void *buf, int *idx)
{
        __uint16_t nbo = htons(data);
        pack8(*(__uint8_t *)(&nbo), buf, idx);
        pack8(*(__uint8_t *)(&nbo + 1), buf, idx);
}

__uint8_t unpack8(void *buf, int *idx)
{
	return *(__uint8_t *) (buf + (*idx)++);
}

__uint16_t unpack16(void *buf, int *idx)
{
	__uint8_t msb = *(__uint8_t *) (buf + (*idx)++);
	__uint8_t lsb = *(__uint8_t *) (buf + (*idx)++);
	return (msb << 8 | lsb);
}

char* unpack_cstr(void *buf, int *idx)
{
        char *char_buf = (char*) buf;
        char *buf_start = char_buf + *idx;
        while ( char_buf[(*idx)++] != '\0');
        return buf_start;
}

int send_cmd(int fd, const void *cmd, int len)
{
	__uint8_t output[OUTPUT_BUFFER_SIZE + len];
	int idx = 0;
	// calc CRC16
	unsigned short crc = crc16_ccitt(cmd, len);
	// Build request
	output[idx++] = _SOF;
	for (int i = 0; i < len; i++)
	{
		pack8(*(char *)(cmd + i), &output, &idx);
	}
	pack8((crc >> 8), &output, &idx);
	pack8((crc & 0xff), &output, &idx);
	output[idx++] = _EOF;
	int cmd_size = idx;
	if (verbose)
	{
		printf("TX %d bytes [", idx);
		for (__uint8_t *p = output; idx-- > 0; p++)
			printf(" %2.2x", (*p & 0xff));
		printf(" ]\n");
	}
	int rc = write(fd, output, cmd_size);
	tcdrain(fd);
	return rc;
}

//FIXME: please clean this shit up
int get_response(int fd, void *output_buffer, int buf_size)
{
	__uint8_t input_buf[INPUT_BUFFER_SIZE];
	int buf_idx = 0;
	int len = 0;
	int idx = 0;
	__uint8_t input;
	bool crc_ok = false;
	bool dle = false;
	bool eof = false;
	int max_fetches = 10;
	memset(input_buf, 0x00, sizeof(input_buf)); // clear buffer
	do
	{
		len = read(fd, &input_buf[buf_idx], sizeof(input_buf) - buf_idx);
		if (len > 0)
		{
			if (verbose)
				printf("Buffer: ");
			for (int i=buf_idx; i < (buf_idx + len); i++) {
				if (verbose)
					printf(" %2.2x", input_buf[i]);
				if (input_buf[i] == _EOF) {
					eof = true;
					break;
				}
			}
			if (verbose)
				printf("\n");
			buf_idx += len;
		}
		if (len == 0)
			max_fetches--;
		if (verbose)
			printf("Read input: %d, total: %d, fetches: %d, EOF: %d\n", len, buf_idx, max_fetches, eof);
	} while (!eof && max_fetches > 0);

	if (buf_idx > buf_size)
	{
		return -ENOBUFS;
	}
	else if (buf_idx > 0)
	{
		unsigned char *p;
		unsigned char *cmd_begin = NULL;
		int cmd_len = 0;
		if (verbose)
			printf("RX %d bytes [", buf_idx);
		for (p = input_buf; buf_idx-- > 0; p++)
		{
			if (verbose)
				printf(" %2.2x", *p);
			if (*p == _SOF)
			{
				cmd_begin = p + 1;
			}
			else if (cmd_begin != NULL)
			{
				if (*p == _EOF)
				{
					if ((p - 2) > cmd_begin)
					{
						// get crc16
						unsigned short crc16 = (*(p - 2) << 8) | (*(p - 1) & 0xff);
						cmd_len = (p - 2) - cmd_begin;
						// calc crc16 and compare
						crc_ok = crc16 == crc16_ccitt(cmd_begin, cmd_len);
					}
				}
				else if (*p == _DLE)
				{
					dle = true;
				}
				else
				{
					if (dle)
					{
						*(char *)(output_buffer + idx++) = *p ^ _XOR;
						dle = false;
					}
					else
					{
						*(char *)(output_buffer + idx++) = *p;
					}
				}
			}
		}
		if (verbose)
			printf(" ] %s\n\n", (crc_ok ? "CRC OK" : "CRC FAILED"));
		return (crc_ok ? (idx - 2) : -EPROTO);
	}
	else if (len < 0)
	{
		printf("Error from read: %d: %s\n", len, strerror(errno));
		return errno;
	}
	else
	{ /* len == 0 timeout */
		printf("Error from read: %d: %s\n", len, "timeout");
		return -ETIMEDOUT;
	}
}

int response_ok(__uint8_t cmd, const void *buf, __uint8_t succ)
{
	__uint8_t cmd_resp = *(__uint8_t *)buf;
	__uint8_t cmd_succ = *(__uint8_t *)(buf + 1);
	//printf("%2.2x : %2.2x\n", cmd_resp, cmd_succ);
	if ((cmd_resp & CMD_RESPONSE) && ( cmd_resp ^ CMD_RESPONSE ) == cmd && cmd_succ == succ)
		return 0;
	else
		return -EIO;
}

int dps_ping()
{
	__uint8_t cmd_buffer[] = {CMD_PING};
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
		if (rc < 0)
			return rc;

		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_PING, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_lock(bool enable)
{
	__uint8_t cmd_buffer[] = {CMD_LOCK, enable ? 1 : 0};
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
		if (rc < 0)
			return rc;

		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_LOCK, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_brightness(int brightness)
{
	__uint8_t cmd_buffer[] = {CMD_SET_BRIGHTNESS, brightness};
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
		if (rc < 0)
			return rc;

		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_SET_BRIGHTNESS, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_power(bool enable)
{
	__uint8_t cmd_buffer[] = {CMD_ENABLE_OUTPUT, enable ? 1 : 0};
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
		if (rc < 0)
			return rc;

		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_ENABLE_OUTPUT, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_voltage(int millivol)
{
	__uint8_t cmd_buffer[18];
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		int size = sprintf((char *)cmd_buffer, "%cu%c%d", CMD_SET_PARAMETERS, '\0', millivol);
		if (size < 0)
			return -EIO;
		rc = send_cmd(fd, cmd_buffer, size);
		if (rc < 0)
			return rc;
	
		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_SET_PARAMETERS, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while (retry >= 0);
	return rc;
}

int dps_current(int milliamp)
{
	__uint8_t cmd_buffer[18];
	__uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
		int size = sprintf((char *)cmd_buffer, "%ci%c%d", CMD_SET_PARAMETERS, '\0', milliamp);
		if (size < 0)
			return -EIO;
		rc = send_cmd(fd, cmd_buffer, size);
		if (rc < 0)
			return rc;

		rc = get_response(fd, &response_buffer, sizeof(response_buffer));
		if (rc > 0 && response_ok(CMD_SET_PARAMETERS, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_query(dps_query_t *result) {
        __uint8_t cmd_buffer[] = { CMD_QUERY };
        __uint8_t response_buffer[128];
	int retry = MAX_RETRY;
	int rc;
	do {
        	rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
        	if (rc < 0)
                	return rc;

        	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
        	if (rc > 0) {
                	if (response_ok(CMD_QUERY, &response_buffer, CMD_STATUS_SUCC) != 0) return -EIO;
                	int idx = 2;
                	result->v_in = unpack16(response_buffer, &idx);
                	result->v_out = unpack16(response_buffer, &idx);
                	result->i_out = unpack16(response_buffer, &idx);
                	result->output_enabled = (response_buffer[idx++] == 1);
                	__uint16_t temp1 = unpack16(response_buffer, &idx);
                	if (temp1 != 0xffff && temp1 & 0x8000) {
                        	temp1 -= 0x10000;
                        	result->temp1 = (double) temp1 / 10;
                	} else {
                        	result->temp1 = -DBL_MAX;
                	}
                	__uint16_t temp2 = unpack16(response_buffer, &idx);
                	if (temp2 != 0xffff && temp2 & 0x8000) {
                        	temp2 -= 0x10000;
                        	result->temp2 = (double) temp2 / 10;
                	} else {
                        	result->temp2 = -DBL_MAX;
                	}
                	result->temp_shutdown = (response_buffer[idx++] == 1);
                //while (idx < rc) {
                //      char *key = unpack_cstr(response_buffer, &idx);
                //      char *val = unpack_cstr(response_buffer, &idx);
                //}
                	return 0;
        	}
		rc = -EPROTO;
		retry--;
	} while (retry >= 0);
        return rc;
}

int dps_change_screen(__uint8_t screen)
{
        __uint8_t cmd_buffer[] = {CMD_CHANGE_SCREEN, screen};
        __uint8_t response_buffer[32];
	int retry = MAX_RETRY;
	int rc;
	do {
        	rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
        	if (rc < 0)
                	return rc;

        	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
        	if (rc > 0 && response_ok(CMD_CHANGE_SCREEN, &response_buffer, CMD_STATUS_SUCC) == 0)
			return 0;
		rc = -EPROTO;
		retry--;
	} while(retry >= 0);
	return rc;
}

int dps_version(dps_version_t *version)
{
        __uint8_t cmd_buffer[] = {CMD_VERSION};
        __uint8_t response_buffer[128];
	int retry = MAX_RETRY;
	int res;
	do {
        	res = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
        	if (res < 0)
                	return res;

        	int size = get_response(fd, &response_buffer, sizeof(response_buffer));
        	if (size >= 13 && response_ok(CMD_VERSION, &response_buffer, CMD_STATUS_SUCC) == 0)
        	{
                	int idx = 2;
                	version->bootloader_ver = strdup(unpack_cstr(response_buffer, &idx));
                	version->firmware_ver = strdup(unpack_cstr(response_buffer, &idx));
                	return 0;
        	}
		res = -EPROTO;
		retry--;
	} while(retry >= 0);
	return res;
}

int dps_upgrade(char *fw_file_name, cb_upgrade_progress progress)
{
        __uint8_t cmd_buffer[5] = { CMD_UPGRADE_START, 0, 0, 0, 0 };
        __uint8_t response_buffer[32];
        __uint16_t chunk_size = 1024;
        // Check if file is a valid firmware

        // Calc crc
        unsigned short crc = calc_crc_file(fw_file_name);

        int idx = 1;
        pack16(chunk_size, &cmd_buffer, &idx);
	pack8((crc >> 8), &cmd_buffer, &idx);
	pack8((crc & 0xff), &cmd_buffer, &idx);
        int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
        if (rc < 0)
                return rc;
        rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	idx = 2;
        if (rc > 0 && response_ok(CMD_UPGRADE_START, &response_buffer, UPGRADE_CONTINUE) == 0) {
                __uint16_t dps_chunk_size = unpack16(response_buffer, &idx);
                if (chunk_size != dps_chunk_size) {
			if (verbose)
				printf("DPS selected chunk size %d\n", dps_chunk_size);
                        chunk_size = dps_chunk_size;
                }
		FILE *file = fopen(fw_file_name, "r");
		if (file == NULL) {
			if (verbose)
				printf("Failed to open firmware file: %s\n", fw_file_name);
			return -EIO;
		}
		fseek(file, 0, SEEK_END); // seek to end of file
		long fw_size = ftell(file); // get current file pointer
		fseek(file, 0, SEEK_SET); // seek back to beginning of file
                __uint8_t input_buf[chunk_size + 1];
                input_buf[0] = CMD_UPGRADE_DATA;
		void *buf_ptr = &input_buf;
		long counter = 0;
		size_t read;
                while(read = fread(buf_ptr + 1, sizeof(__uint8_t), chunk_size, file)) {
			counter += read;
                        rc = send_cmd(fd, &input_buf, read + 1);
                        if (rc < 0)
                                break;
                        rc = get_response(fd, &response_buffer, sizeof(response_buffer));
                        if (rc < 0)
                                break;

			if (response_buffer[0] == (CMD_UPGRADE_DATA ^ CMD_RESPONSE)) {
				idx = 1;
				__uint8_t status = unpack8(response_buffer, &idx);
				if (status == UPGRADE_CONTINUE) {
					if (progress != NULL)
						progress((100 * counter) / fw_size);
					continue;
				} else if (status == UPGRADE_ERASE_ERROR) {
					if (verbose)
						printf("DPS reported erase failed.\n");
					rc = -EIO;
					break;
				} else if (status == UPGRADE_CRC_ERROR) {
					if (verbose)
						printf("DPS reported flash error.\n");
					rc = -EIO;
					break;
				} else if (status == UPGRADE_OVERFLOW_ERROR) {
					if (verbose)
						printf("DPS reported firmware overflow error.\n");
					rc = -EIO;
					break;
				} else if (status == UPGRADE_SUCCESS) {
					if (progress != NULL)
						progress(100);
					rc = 0;
					break;
				} else {
					if (verbose)
						printf("DPS reported unknown error code: %d\n", status);
				}
			}

                }
		fclose(file);
		return rc;
        } else {
                printf("Failed to start upgrade.\n");
        }

}


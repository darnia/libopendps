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
	tty.c_cc[VTIME] = 2; // 0.2 sec.

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

__uint16_t unpack16(void *buf) {
	__uint8_t msb = *(__uint8_t *) buf;
	__uint8_t lsb = *(__uint8_t *) (buf + 1);
	return (msb << 8 | lsb);
}

int send_cmd(int fd, const void *cmd, int len) {
	__uint8_t output[OUTPUT_BUFFER_SIZE];
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
	memset(input_buf, 0x00, sizeof(input_buf)); // clear buffer

	do
	{
		len = read(fd, &input_buf[buf_idx], sizeof(input_buf) - buf_idx);
		// printf("read: %d\n", len);
		if (len > 0)
		{
			buf_idx += len;
		}
	} while (len > 0);

	if (buf_idx > buf_size)
	{
		return -EIO;
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
		return (crc_ok ? (idx - 2) : -EIO);
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

int response_ok(__uint8_t cmd, const void *buf)
{
	__uint8_t cmd_resp = *(char *)buf;
	__uint8_t cmd_succ = *(char *)(buf + 1);
	//printf("%2.2x : %2.2x\n", cmd_resp, cmd_succ);
	if ((cmd_resp & CMD_RESPONSE) && ( cmd_resp ^ CMD_RESPONSE ) == cmd && cmd_succ == 1)
		return 0;
	else
		return -EIO;
}

int dps_ping()
{
	__uint8_t cmd_buffer[] = {CMD_PING};
	__uint8_t response_buffer[32];
	int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (rc < 0)
		return rc;

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_PING, &response_buffer) : rc;
}

int dps_lock(bool enable)
{
	__uint8_t cmd_buffer[] = {CMD_LOCK, enable ? 1 : 0};
	__uint8_t response_buffer[32];
	int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (rc < 0)
		return rc;

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_LOCK, &response_buffer) : rc;
}

int dps_brightness(int brightness)
{
	__uint8_t cmd_buffer[] = {CMD_SET_BRIGHTNESS, brightness};
	__uint8_t response_buffer[32];
	int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (rc < 0)
		return rc;

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_SET_BRIGHTNESS, &response_buffer) : rc;
}

int dps_power(bool enable)
{
	__uint8_t cmd_buffer[] = {CMD_ENABLE_OUTPUT, enable ? 1 : 0};
	__uint8_t response_buffer[32];
	int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (rc < 0)
		return rc;

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_ENABLE_OUTPUT, &response_buffer) : rc;
}

int dps_voltage(int millivol)
{
	__uint8_t cmd_buffer[18];
	__uint8_t response_buffer[32];
	int size = sprintf((char *)cmd_buffer, "%cu%c%d", CMD_SET_PARAMETERS, '\0', millivol);
	if (size < 0)
		return -EIO;
	int rc = send_cmd(fd, cmd_buffer, size);
	if (rc < 0)
	{
		return rc;
	}

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_SET_PARAMETERS, &response_buffer) : rc;
}

int dps_current(int milliamp)
{
	__uint8_t cmd_buffer[18];
	__uint8_t response_buffer[32];
	int size = sprintf((char *)cmd_buffer, "%ci%c%d", CMD_SET_PARAMETERS, '\0', milliamp);
	if (size < 0)
		return -EIO;
	int rc = send_cmd(fd, cmd_buffer, size);
	if (rc < 0)
	{
		return rc;
	}

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	return (rc > 0) ? response_ok(CMD_SET_PARAMETERS, &response_buffer) : rc;
}

int dps_query(query_t *result) {
	__uint8_t cmd_buffer[] = { CMD_QUERY };
	__uint8_t response_buffer[64];
	int rc = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (rc < 0)
		return rc;

	rc = get_response(fd, &response_buffer, sizeof(response_buffer));
	if (rc > 0) {
		if (!response_ok(CMD_QUERY, &response_buffer)) return -EIO;
		result->v_in = unpack16(response_buffer + 2);
		result->v_out = unpack16(response_buffer + 4);
		result->i_out = unpack16(response_buffer + 6);
		result->output_enabled = *(__uint8_t *) (response_buffer + 8) == 1;
		__uint16_t temp1 = unpack16(response_buffer + 9);
		if (temp1 != 0xffff && temp1 & 0x8000) {
			temp1 -= 0x10000;
			result->temp1 = (double) temp1 / 10;
		} else {
			result->temp1 = -DBL_MAX;
		}
		__uint16_t temp2 = unpack16(response_buffer + 11);
		if (temp2 != 0xffff && temp2 & 0x8000) {
			temp2 -= 0x10000;
			result->temp2 = (double) temp2 / 10;
		} else {
			result->temp2 = -DBL_MAX;
		}
		result->temp_shutdown = *(__uint8_t *) (response_buffer + 13) == 1;
		return 0;
	}
	return rc;
}

int dps_version()
{
	__uint8_t cmd_buffer[] = {CMD_VERSION};
	__uint8_t response_buffer[32];
	int res = send_cmd(fd, cmd_buffer, sizeof(cmd_buffer));
	if (res < 0)
	{
		return res;
	}

	int size = get_response(fd, &response_buffer, sizeof(response_buffer));
	if (size > 0 && response_ok(CMD_VERSION, &response_buffer))
	{
		for (int i = 0; i < size; i++)
		{
			printf(" 0x%2.2x", response_buffer[i]);
		}
		printf("\n");
	}
	return 0;
}

int dps_upgrade(char *fw_file_name) {
	int chunk_size = 1024;
	//calc
	__uint8_t cmd_buffer[] = { CMD_UPGRADE_START };
	__uint8_t response_buffer[32];
}

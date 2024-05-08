/*! \file serial.c
 * Utility functions to deal with serial ports */
/*
 * Copyright (C) 2011  Sylvain Munaut <tnt@246tNt.com>
 *
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*! \addtogroup serial
 *  @{
 *  Osmocom serial port helpers
 *
 * \file serial.c */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/serial.h>
#endif

#include <osmocom/core/serial.h>


#if 0
# define dbg_perror(x) perror(x)
#else
# define dbg_perror(x) do { } while (0)
#endif

/*! Open serial device and does base init
 *  \param[in] dev Path to the device node to open
 *  \param[in] baudrate Baudrate constant (speed_t: B9600, B...)
 *  \returns >=0 file descriptor in case of success or negative errno.
 */
int
osmo_serial_init(const char *dev, speed_t baudrate)
{
	int rc, fd=-1, v24, flags;
	struct termios tio;

	/* Use nonblock as the device might block otherwise */
	fd = open(dev, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (fd < 0) {
		dbg_perror("open");
		return -errno;
	}

	/* now put it into blocking mode */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		dbg_perror("fcntl get flags");
		rc = -errno;
		goto error;
	}

	flags &= ~O_NONBLOCK;
	rc = fcntl(fd, F_SETFL, flags);
	if (rc != 0) {
		dbg_perror("fcntl set flags");
		rc = -errno;
		goto error;
	}

	/* Configure serial interface */
	rc = tcgetattr(fd, &tio);
	if (rc < 0) {
		dbg_perror("tcgetattr()");
		rc = -errno;
		goto error;
	}

	if (cfsetispeed(&tio, baudrate) < 0)
		dbg_perror("cfsetispeed()");
	if (cfsetospeed(&tio, baudrate) < 0)
		dbg_perror("cfsetospeed()");

	tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
	tio.c_cflag |=  (CREAD | CLOCAL | CS8);
	tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tio.c_iflag |=  (INPCK);
	tio.c_iflag &= ~(ISTRIP | IXON | IXOFF | IGNBRK | INLCR | ICRNL | IGNCR);
	tio.c_oflag &= ~(OPOST | ONLCR);

	rc = tcsetattr(fd, TCSANOW, &tio);
	if (rc < 0) {
		dbg_perror("tcsetattr()");
		rc = -errno;
		goto error;
	}

	/* Set ready to read/write */
	v24 = TIOCM_DTR | TIOCM_RTS;
	rc = ioctl(fd, TIOCMBIS, &v24);
	if (rc < 0) {
		dbg_perror("ioctl(TIOCMBIS)");
		/* some serial porst don't support this, so let's not
		 * return an error here */
	}

	return fd;

error:
	if (fd >= 0)
		close(fd);
	return rc;
}

static int
_osmo_serial_set_baudrate(int fd, speed_t baudrate)
{
	int rc;
	struct termios tio;

	rc = tcgetattr(fd, &tio);
	if (rc < 0) {
		dbg_perror("tcgetattr()");
		return -errno;
	}

	if (cfsetispeed(&tio, baudrate) < 0)
		dbg_perror("cfsetispeed()");
	if (cfsetospeed(&tio, baudrate) < 0)
		dbg_perror("cfsetospeed()");

	rc = tcsetattr(fd, TCSANOW, &tio);
	if (rc < 0) {
		dbg_perror("tcsetattr()");
		return -errno;
	}

	return 0;
}

/*! Change current baudrate
 *  \param[in] fd File descriptor of the open device
 *  \param[in] baudrate Baudrate constant (speed_t: B9600, B...)
 *  \returns 0 for success or negative errno.
 */
int
osmo_serial_set_baudrate(int fd, speed_t baudrate)
{
	osmo_serial_clear_custom_baudrate(fd);
	return _osmo_serial_set_baudrate(fd, baudrate);
}

/*! Change current baudrate to a custom one using OS specific method
 *  \param[in] fd File descriptor of the open device
 *  \param[in] baudrate Baudrate as integer
 *  \returns 0 for success or negative errno.
 *
 *  This function might not work on all OS or with all type of serial adapters
 */
int
osmo_serial_set_custom_baudrate(int fd, int baudrate)
{
#ifdef __linux__
	int rc;
	struct serial_struct ser_info;

	rc = ioctl(fd, TIOCGSERIAL, &ser_info);
	if (rc < 0) {
		dbg_perror("ioctl(TIOCGSERIAL)");
		return -errno;
	}

	ser_info.flags = ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
	ser_info.custom_divisor = ser_info.baud_base / baudrate;

	rc = ioctl(fd, TIOCSSERIAL, &ser_info);
	if (rc < 0) {
		dbg_perror("ioctl(TIOCSSERIAL)");
		return -errno;
	}

	return _osmo_serial_set_baudrate(fd, B38400); /* 38400 is a kind of magic ... */
#elif defined(__APPLE__)
#ifndef IOSSIOSPEED
#define IOSSIOSPEED    _IOW('T', 2, speed_t)
#endif
	int rc;

	unsigned int speed = baudrate;
	rc = ioctl(fd, IOSSIOSPEED, &speed);
	if (rc < 0) {
		dbg_perror("ioctl(IOSSIOSPEED)");
		return -errno;
	}
	return 0;
#else
#pragma message ("osmo_serial_set_custom_baudrate: unsupported platform")
	return 0;
#endif
}

/*! Clear any custom baudrate
 *  \param[in] fd File descriptor of the open device
 *  \returns 0 for success or negative errno.
 *
 *  This function might not work on all OS or with all type of serial adapters
 */
int
osmo_serial_clear_custom_baudrate(int fd)
{
#ifdef __linux__
	int rc;
	struct serial_struct ser_info;

	rc = ioctl(fd, TIOCGSERIAL, &ser_info);
	if (rc < 0) {
		dbg_perror("ioctl(TIOCGSERIAL)");
		return -errno;
	}

	ser_info.flags = ASYNC_LOW_LATENCY;
	ser_info.custom_divisor = 0;

	rc = ioctl(fd, TIOCSSERIAL, &ser_info);
	if (rc < 0) {
		dbg_perror("ioctl(TIOCSSERIAL)");
		return -errno;
	}
#endif
	return 0;
}

/*! Convert unsigned integer value to speed_t
 *  \param[in] baudrate integer value containing the desired standard baudrate
 *  \param[out] speed the standrd baudrate requested in speed_t format
 *  \returns 0 for success or negative errno.
 */
int
osmo_serial_speed_t(unsigned int baudrate, speed_t *speed)
{
	switch(baudrate) {
	case 0: *speed = B0; break;
	case 50: *speed = B50; break;
	case 75: *speed = B75; break;
	case 110: *speed = B110; break;
	case 134: *speed = B134; break;
	case 150: *speed = B150; break;
	case 200: *speed = B200; break;
	case 300: *speed = B300; break;
	case 600: *speed = B600; break;
	case 1200: *speed = B1200; break;
	case 1800: *speed = B1800; break;
	case 2400: *speed = B2400; break;
	case 4800: *speed = B4800; break;
	case 9600: *speed = B9600; break;
	case 19200: *speed = B19200; break;
	case 38400: *speed = B38400; break;
	case 57600: *speed = B57600; break;
	case 115200: *speed = B115200; break;
	case 230400: *speed = B230400; break;
	default:
		*speed = B0;
		return -EINVAL;
	}
	return 0;
}

/*! @} */

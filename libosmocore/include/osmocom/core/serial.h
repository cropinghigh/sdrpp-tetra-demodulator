/*! \file serial.h
 * Osmocom serial port helpers. */
/*
 * Copyright (C) 2011  Sylvain Munaut <tnt@246tNt.com>
 *
 * All Rights Reserved
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

/*! \defgroup serial Utility functions to deal with serial ports
 *  @{
 * \file serial.h */

#pragma once

#include <termios.h>

int osmo_serial_init(const char *dev, speed_t baudrate);
int osmo_serial_set_baudrate(int fd, speed_t baudrate);
int osmo_serial_set_custom_baudrate(int fd, int baudrate);
int osmo_serial_clear_custom_baudrate(int fd);
int osmo_serial_speed_t(unsigned int baudrate, speed_t *speed);

/*! @} */

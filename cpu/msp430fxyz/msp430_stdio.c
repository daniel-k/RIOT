/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_msp430fxyz
 * @{
 *
 * @file
 * @brief       Implementation of getchar and putchar for MSP430 CPUs
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <sys/types.h>
#include <unistd.h>

#include "uart_stdio.h"


_ssize_t _read_r(struct _reent *r, int fd, void *buffer, size_t count)
{
	(void)r;
	(void)fd;
	return uart_stdio_read(buffer, count);
}


/**
 * @brief   Write nbyte characters to the STDIO UART interface
 */
ssize_t write(int fildes, const void *buf, size_t nbyte)
{
	if (fildes == STDOUT_FILENO) {
		return uart_stdio_write(buf, nbyte);
	}
	else {
		return -1;
	}
}

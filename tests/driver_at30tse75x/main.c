/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for AT30TSE75x temperature sensor
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 *
 * @}
 */

#include <stdio.h>

#include "shell.h"
#include "shell_commands.h"
#ifdef MODULE_NEWLIB
#   include "uart_stdio.h"
#else
#   include "posix_io.h"
#   include "board_uart0.h"
#endif

/**
 * @brief   Buffer size used by the shell
 */
#define SHELL_BUFSIZE           (64U)

int main(void)
{
    shell_t shell;

    puts("AT30TSE75x device driver test");

#ifndef MODULE_NEWLIB
    (void) posix_open(uart0_handler_pid, 0);
    shell_init(&shell, NULL, SHELL_BUFSIZE, uart0_readc, uart0_putc);
#else
    shell_init(&shell, NULL, SHELL_BUFSIZE, getchar, putchar);
#endif
    shell_run(&shell);

    return 0;
}

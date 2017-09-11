/*
 * Copyright (C) 2008, 2009, 2010  Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2013 Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Default application that shows a lot of functionality of RIOT
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "thread.h"
#include "shell.h"
#include "shell_commands.h"

#if FEATURE_PERIPH_RTC
#include "periph/rtc.h"
#endif

#ifdef MODULE_NETIF
#include "net/gnrc/pktdump.h"
#include "net/gnrc.h"
#endif

#include "board.h"

char stack[128];

void* thread(void* arg)
{
	(void) arg;

	int i = 0;
	int pressed = 0;

	USER_BTN_INIT();
	LED2_INIT();

	while(1)
	{
		if(USER_BTN_PRESSED && !pressed) {
			i++;
			puts("pressed");
			LED2_TOGGLE();
			pressed = 1;
		}

		if(USER_BTN_RELEASED) {
			pressed = 0;
		}

		thread_yield();
	}
}

int main(void)
{
#ifdef FEATURE_PERIPH_RTC
    rtc_init();
#endif

#ifdef MODULE_NETIF
    gnrc_netreg_entry_t dump = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                          gnrc_pktdump_pid);
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &dump);
#endif

    (void) puts("Welcome to RIOT!");

//	int i =0;
//	while(i++ < 100) {
//		LED1_TOGGLE();
//		__delay_cycles(1000000);
//	}


	thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN + 1,
		THREAD_CREATE_WOUT_YIELD | THREAD_CREATE_STACKTEST,
		thread, NULL, "user-thr");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

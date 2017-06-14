/*
 * Copyright (C) 2017 INSA Lyon
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <msp430.h>
#include "cpu.h"
#include "board.h"
#include "uart_stdio.h"

/*---------------------------------------------------------------------------*/
void _init(void);

void board_init(void)
{
	/* disable watchdog timer */
	WDTCTL = WDTPW | WDTHOLD;

    /* init CPU core */
	msp430_cpu_init();

//	_init();

	/* release GPIOs (this actually depends on the CPU, not the board) */
	PM5CTL0 &= ~(LOCKLPM5);

	USER_BTN_INIT();

	LED1_INIT();
	LED2_INIT();

	LED1_OFF();
	LED2_OFF();

    /* initialize STDIO over UART */
	uart_stdio_init();

	/* enable interrupts */
	__enable_irq();
}

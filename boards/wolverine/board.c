/*
 * board.c - Board initialization for the Zolertia Z1
 * Copyright (C) 2014 INRIA
 *
 * Author : Kévin Roussel <Kevin.Roussel@inria.fr>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_z1
 * @{
 *
 * @file
 * @brief       Board specific implementations for the Zolertia Z1
 *
 * @author      Kévin Roussel <Kevin.Roussel@inria.fr>
 *
 * @}
 */


#include <msp430.h>
#include "cpu.h"
#include "board.h"
#include "uart_stdio.h"

/*---------------------------------------------------------------------------*/

void board_init(void)
{
	/* disable watchdog timer */
	WDTCTL = WDTPW | WDTHOLD;

	/* release GPIOs */
	PM5CTL0 &= ~(LOCKLPM5);

    /* init CPU core */
	msp430_cpu_init();

	LED1_INIT();
	LED2_INIT();

	LED2_ON();

    /* initialize STDIO over UART */
	uart_stdio_init();

	/* enable interrupts */
	__enable_irq();
}

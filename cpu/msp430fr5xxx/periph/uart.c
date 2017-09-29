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
 * @brief       Low-level UART driver implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <msp430.h>
#include "cpu.h"
#include "periph_cpu.h"
#include "periph_conf.h"
#include "periph/uart.h"

#include "sytare.h"

/**
 * @brief   Keep track of the interrupt context
 * @{
 */
static uart_rx_cb_t ctx_rx_cb;
static void *ctx_isr_arg;
/** @} */

static int init_base(uart_t uart, uint32_t baudrate);

// we have to prevent gcc from inlining this function, otherwise we get into trouble
// with returns before switching back the stack
__attribute__((used, noinline))
static int _uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
	if (init_base(uart, baudrate) < 0) {
		return -1;
	}

	/* save interrupt context */
	ctx_rx_cb = rx_cb;
	ctx_isr_arg = arg;

	return 0;
}

int uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
	syt_syscall_enter();

	__asm__ __volatile__("call #_uart_init");

	syt_syscall_exit();

	return 0;
}


static int init_base(uart_t uart, uint32_t baudrate)
{
	if ((uart != 0) || baudrate != (115200U)) {
        return -1;
    }

	// hardcode to primary uart
	P2SEL0 &= ~(BIT0 | BIT1);
	P2SEL1 |=  (BIT0 | BIT1);

	UCA0CTLW0 |= UCSWRST;

	// use SMCLK
	UCA0CTLW0 |= UCSSEL_2;

	// Datasheet:
	// BRCLK	"Baud Rate"		UCOS16	UCBRx	UCBRFx	UCBRSx
	// 1000000	115200			0		8		-		0xD6

	UCA0BRW = 8;
	UCA0MCTLW = 0xd6 << 8;

	UCA0CTLW0 &= ~(UCSWRST);

	// only enable RX interrupt
	UCA0IE = UCRXIE;

	return 0;
}

void uart_write(uart_t uart, const uint8_t *data, size_t len)
{
    (void)uart;

    for (size_t i = 0; i < len; i++) {
		while (!(UCA0IFG & UCTXIFG));
		UCA0TXBUF = data[i];
    }
}

void uart_poweron(uart_t uart)
{
    (void)uart;
    /* n/a */
}

void uart_poweroff(uart_t uart)
{
    (void)uart;
    /* n/a */
}

ISR(UART_RX_ISR, isr_uart_0_rx)
{
    __enter_isr();

	LED1_TOGGLE();

	uint8_t stat = UCA0IFG;


	if (stat & UCRXIFG) {
		ctx_rx_cb(ctx_isr_arg, (uint8_t)UCA0RXBUF);
    }
    else {
		/* some error which we do not handle, just do a pseudo read to reset the
		 * status register */
//		(void)data;
    }

    __exit_isr();
}

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

/**
 * @brief   Keep track of the interrupt context
 * @{
 */
static uart_rx_cb_t ctx_rx_cb;
static void *ctx_isr_arg;
/** @} */

static int init_base(uart_t uart, uint32_t baudrate);

#define UART_USE_USCI


/* per default, we use the legacy MSP430 USART module for UART functionality */
#ifndef UART_USE_USCI

int uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
    int res = init_base(uart, baudrate);
    if (res != UART_OK) {
        return res;
    }

    /* save interrupt context */
    ctx_rx_cb = rx_cb;
    ctx_isr_arg = arg;
    /* reset interrupt flags and enable RX interrupt */
    UART_IE &= ~(UART_IE_TX_BIT);
    UART_IF &= ~(UART_IE_RX_BIT);
    UART_IF |=  (UART_IE_TX_BIT);
    UART_IE |=  (UART_IE_RX_BIT);
    return UART_OK;
}

static int init_base(uart_t uart, uint32_t baudrate)
{
    if (uart != 0) {
        return UART_NODEV;
    }

    /* get the default UART for now -> TODO: enable for multiple devices */
    msp_usart_t *dev = UART_BASE;

    /* power off and reset device */
    uart_poweroff(uart);
    dev->CTL = USART_CTL_SWRST;
    /* configure to 8N1 and using the SMCLK*/
    dev->CTL |= USART_CTL_CHAR;
    dev->TCTL = (USART_TCTL_TXEPT | USART_TCTL_SSEL_SMCLK);
    dev->RCTL = 0x00;
    /* baudrate configuration */
    uint16_t br = (uint16_t)(CLOCK_CMCLK / baudrate);
    dev->BR0 = (uint8_t)br;
    dev->BR1 = (uint8_t)(br >> 8);
    /* TODO: calculate value for modulation register */
    dev->MCTL = 0;
    /* configure pins -> TODO: move into GPIO driver (once implemented) */
    UART_PORT->SEL |= (UART_RX_PIN | UART_TX_PIN);
    UART_PORT->OD |= UART_RX_PIN;
    UART_PORT->OD &= ~(UART_TX_PIN);
    UART_PORT->DIR |= UART_TX_PIN;
    UART_PORT->DIR &= ~(UART_RX_PIN);
    /* enable receiver and transmitter */
    uart_poweron(uart);
    /* and finally release the software reset bit */
    dev->CTL &= ~(USART_CTL_SWRST);
    return UART_OK;
}

void uart_write(uart_t uart, const uint8_t *data, size_t len)
{
    (void)uart;
    msp_usart_t *dev = UART_BASE;

    for (size_t i = 0; i < len; i++) {
        while (!(dev->TCTL & USART_TCTL_TXEPT)) {}
        dev->TXBUF = data[i];
    }
}

void uart_poweron(uart_t uart)
{
    UART_ME |= UART_ME_BITS;
}

void uart_poweroff(uart_t uart)
{
    UART_ME &= ~(UART_ME_BITS);
}

ISR(UART_RX_ISR, isr_uart_0_rx)
{
    __enter_isr();

    /* read character (resets interrupt flag) */
    char c = UART_BASE->RXBUF;

    /* only call callback if there was no receive error */
    if(! (UART_BASE->RCTL & RXERR)) {
        ctx_rx_cb(ctx_isr_arg, c);
    }

    __exit_isr();
}

/* we use alternative UART code in case the board used the USCI module for UART
 * in case of the (older) USART module */
#else

int uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
    if (init_base(uart, baudrate) < 0) {
        return -1;
    }

    /* save interrupt context */
    ctx_rx_cb = rx_cb;
    ctx_isr_arg = arg;
//    /* reset interrupt flags and enable RX interrupt */
//    UART_IF &= ~(UART_IE_RX_BIT);
//    UART_IF |=  (UART_IE_TX_BIT);
//    UART_IE |=  (UART_IE_RX_BIT);
//    UART_IE &= ~(UART_IE_TX_BIT);
    return 0;
}

static int init_base(uart_t uart, uint32_t baudrate)
{
    if (uart != 0) {
        return -1;
    }

	P2SEL0 &= ~(BIT0 | BIT1);
	P2SEL1 |=  (BIT0 | BIT1);

	UCA0CTLW0 |= UCSWRST;

	// use SMCLK
	UCA0CTLW0 |= UCSSEL_2;

	// BRCLK	"Baud Rate"		UCOS16	UCBRx	UCBRFx	UCBRSx
	// 1000000	115200			0		8		-		0xD6

	UCA0BRW = 8;
	UCA0MCTLW = 0xd6 << 8;

	UCA0CTLW0 &= ~(UCSWRST);

	// only enable RX interrupt
	UCA0IE = UCRXIE;

//	UCA0IFG = 1;

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

#endif

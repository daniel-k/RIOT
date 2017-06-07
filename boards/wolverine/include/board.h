/*
 * Copyright (C) 2014 INRIA
 *               2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef Z1_BOARD_H
#define Z1_BOARD_H

/**
 * @defgroup    boards_z1 Zolertia Z1
 * @ingroup     boards
 * @brief       Support for the Zolertia Z1 board.
 *
<h2>Components</h2>
\li MSP430F2617
\li CC2420

* @{
*
 * @file
 * @brief       Zolertia Z1 board configuration
 *
 * @author      Kévin Roussel <Kevin.Roussel@inria.fr>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 */

#include <stdint.h>

#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Define the CPU model for the <msp430.h>
 */
#ifndef __MSP430FR5969__
#define __MSP430FR5969__
#endif

/**
 * @name    Xtimer configuration
 * @{
 */
#define XTIMER_DEV                  (0)
#define XTIMER_CHAN                 (0)
#define XTIMER_WIDTH                (16)
#define XTIMER_BACKOFF              (40)
/** @} */

/**
 * @name    CPU core configuration
 * @{
 */
/** @todo   Move this to the periph_conf.h */
#define MSP430_INITIAL_CPU_SPEED    8000000uL
#ifndef F_CPU
#define F_CPU                       MSP430_INITIAL_CPU_SPEED
#endif
#define F_RC_OSCILLATOR             32768
#define MSP430_HAS_DCOR             0
#define MSP430_HAS_EXTERNAL_CRYSTAL 1
/** @} */

/**
 * @name    LED pin definitions and handlers
 * @{
 */
#define LED1_PIN                    GPIO_PIN(4, 6)
#define LED2_PIN                    GPIO_PIN(1, 0)

#define LED1_OUT_REG				P4OUT
#define LED1_DIR_REG				P4DIR
#define LED2_OUT_REG				P1OUT
#define LED2_DIR_REG				P1DIR

#define LED1_MASK                   (BIT6)
#define LED2_MASK                   (BIT0)

#define LED1_INIT()					do { (LED1_DIR_REG |= LED1_MASK); } while(0)
#define LED1_ON()                   do { (LED1_OUT_REG |= LED1_MASK); } while(0)
#define LED1_OFF()                  do { (LED1_OUT_REG &=~LED1_MASK); } while(0)
#define LED1_TOGGLE()               do { (LED1_OUT_REG ^= LED1_MASK); } while(0)

#define LED2_INIT()					do { (LED2_DIR_REG |= LED2_MASK); } while(0)
#define LED2_ON()                   do { (LED2_OUT_REG |= LED2_MASK); } while(0)
#define LED2_OFF()                  do { (LED2_OUT_REG &=~LED2_MASK); } while(0)
#define LED2_TOGGLE()               do { (LED2_OUT_REG ^= LED2_MASK); } while(0)
/** @} */

/**
 * @name    User button configuration
 * @{
 */
#define USER_BTN_PxIN      P2IN
#define USER_BTN_MASK      0x20

#define USER_BTN_PRESSED   ((USER_BTN_PxIN & USER_BTN_MASK) == 0)
#define USER_BTN_RELEASED  ((USER_BTN_PxIN & USER_BTN_MASK) != 0)
/** @} */


#ifdef __cplusplus
}
#endif

/** @} */
#endif /*  Z1_BOARD_H */

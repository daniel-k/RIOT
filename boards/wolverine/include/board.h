/*
 * Copyright (C) 2017 INSA Lyon
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef WOLVERINE_BOARD_H
#define WOLVERINE_BOARD_H

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
#define USER_BTN_PxIN      P4IN
#define USER_BTN_MASK      (1U << 5)

#define USER_BTN_INIT()		do { P4DIR &= ~(USER_BTN_MASK); P4REN |= USER_BTN_MASK; P4OUT |= USER_BTN_MASK; } while(0)
#define USER_BTN_PRESSED   ((USER_BTN_PxIN & USER_BTN_MASK) == 0)
#define USER_BTN_RELEASED  ((USER_BTN_PxIN & USER_BTN_MASK) != 0)
/** @} */


#ifdef __cplusplus
}
#endif

/** @} */
#endif /*  WOLVERINE_BOARD_H */

/*
 * Copyright (C) 2014 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    driver_periph_cpuid CPUID
 * @ingroup     driver_periph
 * @brief       Low-level CPU ID peripheral driver
 *
 * Provides access the CPU's serial number
 *
 * @{
 * @file
 * @brief       Low-level CPUID peripheral driver interface definitions
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#ifndef PERIPH_NVM_H_
#define PERIPH_NVM_H_

#include <stdbool.h>
#include "cpu_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in bytes */
#define PAGE_COUNT      (4096U)
#define PAGE_SIZE       (64U)
#define PAGES_IN_ROW    (4U)
#define ROW_SIZE        (PAGE_SIZE * PAGES_IN_ROW)

#define NVM_MEMORY ((volatile uint16_t *)FLASH_ADDR)


void* row_start(void* addr);

int nvm_write_erase(void* to, void* from, size_t len);

int nvm_page_write(void* to, void* from, size_t len);

int erase_row(unsigned num);

void enable_automatic_write(bool enabled);

void* row_start(void* addr);

unsigned next_row(void* addr);
void* row_to_addr(unsigned row);


#ifdef __cplusplus
}
#endif

#endif /* PERIPH_NVM_H_ */
/**
 * @}
 */

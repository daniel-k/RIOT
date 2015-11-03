/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @addtogroup  driver_periph
 * @{
 *
 * @file
 * @brief       Low-level CPUID driver implementation
 *
 * @author      Troels Hoffmeyer <troels.d.hoffmeyer@gmail.com>
 */

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include <cpu.h>
#include <periph/nvm.h>

#define ENABLE_DEBUG    (1)
#include "debug.h"


typedef enum {
    CMD_ER = 0x02,
    CMD_WP = 0x04,
    CMD_EAR = 0x05,
    CMD_WAP = 0x06,
    CMD_SF = 0x0a,
    CMD_WL = 0x0f,
    CMD_LR = 0x40,
    CMD_UR = 0x41,
    CMD_SPRM = 0x42,
    CMD_CPRM = 0x43,
    CMD_PBC = 0x44,
    CMD_SSB = 0x45,
    CMD_INVALL = 0x46
} nvm_cmd_t;


static inline unsigned page_num(void* addr)
{
    return ( ((uint32_t)addr) / PAGE_SIZE);
}

static inline unsigned row_num(void* addr)
{
    return (page_num(addr)) / PAGES_IN_ROW;
}

void* row_start(void* addr)
{
    return (void*) (row_num(addr) * PAGES_IN_ROW * PAGE_SIZE);
}

void* row_end(void* addr)
{
    return (void*)((uint32_t)row_start(addr) + PAGES_IN_ROW * PAGE_SIZE);
}

void enable_automatic_write(bool enabled)
{
    Nvmctrl* nvm = NVMCTRL;

    nvm->CTRLB.bit.MANW = !enabled;
}

unsigned next_row(void* addr)
{
    unsigned row = row_num(addr);

    return (row_start(addr) == addr) ? row : (row + 1);
}

void* row_to_addr(unsigned row)
{
    return (void*)(ROW_SIZE * row);
}

static bool _nvm_addr_valid(uint32_t addr)
{
    return ( (addr >= (uint32_t)NVM_MEMORY) && (addr < ((uint32_t)NVM_MEMORY + PAGE_SIZE * PAGE_COUNT)) );
}

static int nvm_cmd(nvm_cmd_t cmd, uint32_t addr)
{
    if(!_nvm_addr_valid(addr)) {
        DEBUG("nvm: address %p not valid\n", (void*)addr);
        return -1;
    }

    /* clear error flags */
    NVMCTRL->STATUS.reg |= 0x1e;

    /* disable cache, save state to restore later */
    uint32_t ctrlb = NVMCTRL->CTRLB.reg;
    NVMCTRL->CTRLB.bit.CACHEDIS = 1;

    switch(cmd)
    {
    case CMD_WP:
    case CMD_ER:
        NVMCTRL->ADDR.reg = addr / 2;
        break;
    case CMD_PBC:
        /* Nothing special be done here */
        break;
    default:
        DEBUG("nvm: command %d not implemented\n", cmd);
        return -2;
    }

    /* issue command and wait for completion */
    NVMCTRL->CTRLA.reg = cmd | NVMCTRL_CTRLA_CMDEX_KEY;
    while(!NVMCTRL->INTFLAG.bit.READY);

    /* restore cache state */
    NVMCTRL->CTRLB.reg = ctrlb;

    if(NVMCTRL->STATUS.bit.NVME) {
        DEBUG("nvm: error ");
        if(NVMCTRL->STATUS.bit.LOCKE)
            DEBUG("LOCKE ");
        else if(NVMCTRL->STATUS.bit.PROGE)
            DEBUG("PROGE ");
        DEBUG("occured for cmd %02x\n", cmd);
        return -3;
    }

    return 0;
}

int erase_row(unsigned num)
{
    uint16_t* start = row_to_addr(num);
    uint16_t* end = row_to_addr(num+1) - 1;
    DEBUG("nvm: erase row %i (%p to %p)\n", num, start, end);

    return nvm_cmd(CMD_ER, (uint32_t)start);
}

/* Assumes row has been erased */
int nvm_page_write(void* to, void* from, size_t len)
{
    if(page_num(to) != page_num(to + len - 1)) {
        DEBUG("nvm: writing accross pages is forbidden\n");
        return -1;
    }


    if( ((uint32_t)from & 0x1) || ((uint32_t)to & 0x1) ) {
        DEBUG("nvm: addresses must be 2-byte aligned\n");
        return -2;
    }

    DEBUG("nvm: clear page buffer\n");
    nvm_cmd(CMD_PBC, (uint32_t)to);

    unsigned bytes_written = 0;
    uint16_t* dest = to;
    uint16_t* src = from;

    DEBUG("nvm: copy from %p to %p src-len: %d bytes\n", from, to, len);
    while((void*)dest < (to + len))
    {
        *(dest++) = *(src++);
        bytes_written += 2;
    }

    if(!NVMCTRL->STATUS.bit.LOAD) {
        DEBUG("nvm: something went wrong, page buffer not loaded\n");
        return -3;
    }

    nvm_cmd(CMD_WP, (uint32_t)to);

    if(NVMCTRL->STATUS.bit.NVME) {
        return -4;
    }

    DEBUG("nvm: %u bytes written to %p\n", bytes_written, to);
    return bytes_written;
}


int nvm_write_erase(void* to, void* from, size_t len)
{
    unsigned row_first = row_num(to);
    unsigned row_last = row_num(to + len);

    for(int row = row_first; row <= row_last; row++)
    {
        erase_row(row);

        unsigned pages = PAGES_IN_ROW;
        size_t row_length = ROW_SIZE;
        if( (row == row_last) && (len % ROW_SIZE != 0)) {
            row_length = len % ROW_SIZE;
            pages = row_length / PAGE_SIZE + 1;
        }

        for(int page = 0; page < pages; page++)
        {
            size_t offset = page * PAGE_SIZE;

            size_t page_length = PAGE_SIZE;
            /* The last page may not be filled completely */
            if(page == (pages - 1)) {
                page_length = row_length % PAGE_SIZE;
            }
            DEBUG("nvm: write %u bytes to page %i in row %i\n", page_length, page, row);
            nvm_page_write(to + offset, from + offset, page_length);
        }
    }

    return 0;
}

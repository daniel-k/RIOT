/*
 * Copyright (C) Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       Driver for the EEPROM in AT30TSE75x temperature sensor
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 */


#include "periph/i2c.h"
#include "hwtimer.h"
#include "at30tse75x.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

static uint8_t calculate_i2c_addr(at30tse75x_t* dev, uint16_t data_addr)
{
    /* Upper 4 bit are fixed address */
    uint8_t i2c_addr = dev->addr_eeprom;

    /* Depending on EEPROM size*/
    switch(dev->eeprom_size) {
    case AT30TSE75X_EEPROM_2KB:
        /* No manipulation needed*/
        break;
    case AT30TSE75X_EEPROM_4KB:
        i2c_addr &= ~(1 << 0);
        i2c_addr |= (data_addr & 0x100) >> 8;
        break;
    case AT30TSE75X_EEPROM_8KB:
        i2c_addr &= ~((1 << 0) | (1 << 1));
        i2c_addr |= (data_addr & 0x300) >> 8;
        break;
    }
    return i2c_addr;
}

int at30tse75x_eeprom_write(at30tse75x_t* dev, uint16_t addr, uint8_t data)
{
    i2c_acquire(dev->i2c);
    if(i2c_write_reg(dev->i2c, calculate_i2c_addr(dev, addr), addr & 0xff, data) != 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    i2c_release(dev->i2c);
    /* Wait for write to finish */
    hwtimer_spin(HWTIMER_TICKS(3000));
    return 0;
}

int at30tse75x_eeprom_read(at30tse75x_t* dev, uint16_t addr, uint8_t* data)
{
    i2c_acquire(dev->i2c);
    /* Dummy write the address */
    if(i2c_write_byte(dev->i2c, calculate_i2c_addr(dev, addr), addr & 0xff) != 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    /* Read the byte */
    if(i2c_read_byte(dev->i2c, calculate_i2c_addr(dev, addr), (char*)data) != 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    i2c_release(dev->i2c);
    return 0;
}

int at30tse75x_eeprom_reads(at30tse75x_t* dev, uint16_t start_addr, uint8_t* data, uint16_t length)
{
    if((start_addr + length) > at30tse75x_eeprom_size(dev)) {
        /* Read out of bounds */
        return -1;
    }

    i2c_acquire(dev->i2c);
    /* Dummy write the address */
    if(i2c_write_byte(dev->i2c, calculate_i2c_addr(dev, start_addr), start_addr & 0xff) != 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    /* Read the bytes */
    if(i2c_read_bytes(dev->i2c, calculate_i2c_addr(dev, start_addr), (char*)data, length) != length) {
        i2c_release(dev->i2c);
        return -1;
    }
    i2c_release(dev->i2c);
    return 0;
}

uint16_t at30tse75x_eeprom_size(at30tse75x_t* dev)
{
    return dev->eeprom_size;
}


int at30tse75x_eeprom_init(at30tse75x_t* dev, uint8_t addr, at30tse75x_eeprom_size_t size)
{
    /* Only lowest 3 bit of I2C address are dynamic */
    if(addr > 7) {
        return -1;
    }
    dev->addr_eeprom = addr | AT30TSE75X_ADDR__EEPROM;

    switch(size) {
    case AT30TSE75X_EEPROM_2KB:
    case AT30TSE75X_EEPROM_4KB:
    case AT30TSE75X_EEPROM_8KB:
        break;
    default:
        return -1;
    }
    dev->eeprom_size = size;

    return 0;
}

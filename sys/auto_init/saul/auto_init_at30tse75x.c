/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/*
 * @ingroup     auto_init_saul
 * @{
 *
 * @file
 * @brief       Auto initialization of AT30TSE75X temperature sensors
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 *
 * @}
 */
#ifdef MODULE_AT30TSE75X

#include "periph/i2c.h"
#include "saul_reg.h"
#include "at30tse75x.h"
#include <at30tse75x_params.h>

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   Define the number of configured sensors
 */
#define AT30TSE75X_NUM    (sizeof(at30tse75x_params)/sizeof(at30tse75x_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static at30tse75x_t at30tse75x_devs[AT30TSE75X_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[AT30TSE75X_NUM];

/**
 * @brief   Reference the driver struct
 */
extern saul_driver_t at30tse75x_saul_driver;


void auto_init_at30tse75x(void)
{
    for (int i = 0; i < AT30TSE75X_NUM; i++) {
        const at30tse75x_t *p = &at30tse75x_params[i];

        DEBUG("[auto_init_saul] initializing at30tse75x temperature sensor\n");
        int res = at30tse75x_init(&at30tse75x_devs[i], p->i2c, I2C_SPEED_NORMAL, p->addr);
        DEBUG("not done\n");
        if (res < 0) {
            DEBUG("[auto_init_saul] error during initialization\n");
        }
        else {
            saul_entries[i].dev = &(at30tse75x_devs[i]);
            saul_entries[i].name = at30tse75x_saul_info[i].name;
            saul_entries[i].driver = &at30tse75x_saul_driver;
            saul_reg_add(&(saul_entries[i]));
        }
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_AT30TSE75X */

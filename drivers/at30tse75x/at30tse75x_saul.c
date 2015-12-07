/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_at30tse75x
 * @{
 *
 * @file
 * @brief       AT30TSE75x adaption to the RIOT actuator/sensor interface
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "at30tse75x.h"

static int read(void *dev, phydat_t *res)
{
    at30tse75x_t *d = (at30tse75x_t *)dev;
    float temp;
    at30tse75x_get_temperature(d, &temp);
    res->val[0] = (temp * 100);
    res->unit = UNIT_TEMP_C;
    res->scale = -2;
    return 1;
}

static int write(void *dev, phydat_t *state)
{
    return -ENOTSUP;
}


const saul_driver_t at30tse75x_saul_driver = {
    .read = read,
    .write = write,
    .type = SAUL_SENSE_TEMP,
};

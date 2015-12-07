/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup   boards_samr21-xpro
 * @{
 *
 * @file
 * @brief     AT30TSE75x attachable temperature sensor
 *
 * @author    Daniel Krebs <github@daniel-krebs.net>
 */

#ifndef AT30TSE75X_PARAMS_H
#define AT30TSE75X_PARAMS_H

#include "board.h"
#include "at30tse75x.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief    AT30TSE75x configuration
 */
static const  at30tse75x_t at30tse75x_params[] =
{
    {
        .i2c = 0,
        .addr = 0x48,
    },
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t at30tse75x_saul_info[] =
{
    {
        .name = "at30tse75x",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* AT30TSE75X_PARAMS_H */
/** @} */

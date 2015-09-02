/*
 * Copyright (C) Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    at30tse75x AT30TSE75x temperature sensor with EEPROM
 * @ingroup     drivers
 *
 * The connection between the MCU and the AT30TSE75x is based on the
 * I2C-interface. There are 3 versions of this IC, with either 2/4/8 Kb of
 * EEPROM.
 *
 * @{
 *
 * @file
 * @brief       Driver for the AT30TSE75x temperature sensor with serial EEPROM
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 */

#ifndef AT30TSE75X_H_
#define AT30TSE75X_H_

#include <stdint.h>
#include "periph/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name AT30TSE75x I2C addresses
 * @{
 */
#define AT30TSE75X_ADDR__TEMP           (0x48)
#define AT30TSE75X_ADDR__EEPROM         (0x50)
/** @} */

/**
 * @name AT30TSE75x register addresses
 * @{
 */
#define AT30TSE75X_REG__TEMPERATURE     (0x00)
#define AT30TSE75X_REG__CONFIG          (0x01)
#define AT30TSE75X_REG__LIMIT_LOW       (0x02)
#define AT30TSE75X_REG__LIMIT_HIGH      (0x03)
#define AT30TSE75X_REG__NV_CONFIG       (0x11)
#define AT30TSE75X_REG__NV_LIMIT_LOW    (0x12)
#define AT30TSE75X_REG__NV_LIMIR_HIGH   (0x13)
/** @} */

/**
 * @name AT30TSE75x number formatting
 * @{
 */
#define AT30TSE75X_INTEGER_MASK         (0x00ff)
#define AT30TSE75X_INTEGER_SHIFT        (0)
#define AT30TSE75X_FRACTIONAL_MASK      (0xf000)
#define AT30TSE75X_FRACTIONAL_SHIFT     (12)
#define AT30TSE75X_FRACTIONAL_BASE      (0.0625f)
/** @} */


/**
 * @name AT30TSE75x configuration register
 *
 * Only upper byte can be read/written, so treat as 8-bit register.
 * @{
 */
#define AT30TSE75X_CONFIG__OS_BIT               (1 << 7)
#define AT30TSE75X_CONFIG__RESOLUTION_MASK      (0x60)
#define AT30TSE75X_CONFIG__RESOLUTION_SHIFT     (5)
#define AT30TSE75X_CONFIG__FTQ_MASK             (0x18)
#define AT30TSE75X_CONFIG__FTQ_SHIFT            (3)
#define AT30TSE75X_CONFIG__ALERT_POL_BIT        (1 << 2)
#define AT30TSE75X_CONFIG__ALARM_MODE_BIT       (1 << 1)
#define AT30TSE75X_CONFIG__SHUTDOWN_BIT         (1 << 0)
/** @} */

/**
 * @name AT30TSE75x commands
 *
 * @{
 */
#define AT30TSE75X_CMD__SAVE_TO_NVRAM           (0x48)
#define AT30TSE75X_CMD__RESTORE_FROM_NVRAM      (0xb8)
#define AT30TSE75X_CMD__GENERAL_CALL_RESET      (0x06)
#define AT30TSE75X_CMD__GENERAL_CALL_RELATCH    (0x04)
/** @} */

/**
  * @name AT30TSE75x configuration types
  *
  * @brief   Temperature resolution
  *
  * @{
  */
typedef enum {
    AT30TSE75X_RESOLUTION_9BIT = 0,
    AT30TSE75X_RESOLUTION_10BIT = 1,
    AT30TSE75X_RESOLUTION_11BIT = 2,
    AT30TSE75X_RESOLUTION_12BIT = 3
} at30tse75x_resolution_t;
/** @} */

/**
  * @brief   Operation mode
  *
  * The device can run in continous or one-shot mode. While in one-shot mode it
  * is effectively shutdown and only wakes up to perform a single measurement.
  * When in comparator or interrupt mode, the device samples contiously the
  * temperature and sets the ALERT pin according to the chosen mode.
  * @{
  */
typedef enum {
    AT30TSE75X_MODE_COMPARATOR,
    AT30TSE75X_MODE_INTERRUPT,
    AT30TSE75X_MODE_ONE_SHOT
} at30tse75x_mode_t;
/** @} */

/**
  * @brief   After how many limit exceeding measurements the ALERT pin is set
  * @{
  */
typedef enum {
    AT30TSE75X_ALARM_AFTER_1 = 0,
    AT30TSE75X_ALARM_AFTER_2 = 1,
    AT30TSE75X_ALARM_AFTER_4 = 2,
    AT30TSE75X_ALARM_AFTER_6 = 3
} at30tse75x_fault_tolerance_t;
/** @} */

/**
  * @brief   Polarity of the ALERT pin
  * @{
  */
typedef enum {
    AT30TSE75X_ALARM_ACTIVE_LOW,
    AT30TSE75X_ALARM_ACTIVE_HIGH
} at30tse75x_alarm_polatity_t;
/** @} */

/**
 * @name EEPROM size in kilobit
 * @{
 */
typedef enum {
    AT30TSE75X_EEPROM_2KB = 256,
    AT30TSE75X_EEPROM_4KB = 512,
    AT30TSE75X_EEPROM_8KB = 1024
} at30tse75x_eeprom_size_t;
/** @} */

/**
  * @brief   Device descriptor for a AT30TSE75x device
  * @{
  */
typedef struct {
    i2c_t i2c;
    uint8_t addr_temp;
    uint8_t addr_eeprom;
    at30tse75x_eeprom_size_t eeprom_size;
} at30tse75x_t;
/** @} */

/**
 * @brief   Initialize a AT30TSE75x device
 *
 * The address given here should only be the lowest 3 bit of the I2C address,
 * i.e. the configurable address pins A2 A1 A0.
 *
 * Example:
 * A2=1 A1=1 A0=0 => addr = 0x06
 *
 * @param[out] dev          device descriptor
 * @param[in] i2c           I2C bus the device is connected to
 * @param[in] speed         I2C speed to use
 * @param[in] addr          I2C address of the device
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_init(at30tse75x_t* dev, i2c_t i2c, i2c_speed_t speed, uint8_t addr);

/**
 * @brief   Save configuration register to non-volatile backup register
 *
 * @param[in] dev           device descriptor
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_save_config(at30tse75x_t* dev);

/**
 * @brief   Restore configuration register from non-volatile backup register
 *
 * @param[in] dev           device descriptor
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_restore_config(at30tse75x_t* dev);

/**
 * @brief   Get content of configuration register
 *
 * @param[in] dev           device descriptor
 * @param[out] data         buffer where config register will be written to
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_get_config(at30tse75x_t* dev, uint8_t* data);

/**
 * @brief   Set content of configuration register
 *
 * @param[in] dev           device descriptor
 * @param[in] data          new value for configuration register
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_config(at30tse75x_t* dev, uint8_t data);

/**
 * @brief   Set temperature resolution
 *
 * @param[in] dev           device descriptor
 * @param[in] resolution    temperature resolution
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_resolution(at30tse75x_t* dev, at30tse75x_resolution_t resolution);

/**
 * @brief   Set operation mode
 *
 * @param[in] dev           device descriptor
 * @param[in] mode          operation mode
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_mode(at30tse75x_t* dev, at30tse75x_mode_t mode);

/**
 * @brief   Set polarity of ALERT pin
 *
 * @param[in] dev           device descriptor
 * @param[in] polarity      polarity of ALERT pin
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_alarm_polarity(at30tse75x_t* dev, at30tse75x_alarm_polatity_t polarity);

/**
 * @brief   Set tolerance to outlying measurements
 *
 * @param[in] dev           device descriptor
 * @param[in] tolerance     tolerance
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_fault_tolerance(at30tse75x_t* dev, at30tse75x_fault_tolerance_t tolerance);

/**
 * @brief   Set T_Low limit
 *
 * @param[in] dev           device descriptor
 * @param[in] t_low         lower temperature limit
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_limit_low(at30tse75x_t* dev, int8_t t_low);

/**
 * @brief   Set T_High limit
 *
 * @param[in] dev           device descriptor
 * @param[in] t_high        upper temperature limit
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_set_limit_high(at30tse75x_t* dev, int8_t t_high);

/**
 * @brief   Get measured temperature
 *
 * @param[in] dev           device descriptor
 * @param[out] temperature  float buffer where temperature will be written to
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int at30tse75x_get_temperature(at30tse75x_t* dev, float* temperature);


int at30tse75x_eeprom_init(at30tse75x_t* dev, uint8_t addr, at30tse75x_eeprom_size_t size);
int at30tse75x_eeprom_read(at30tse75x_t* dev, uint16_t addr, uint8_t* data);
int at30tse75x_eeprom_reads(at30tse75x_t* dev, uint16_t start_addr, uint8_t* data, uint16_t length);
int at30tse75x_eeprom_write(at30tse75x_t* dev, uint16_t addr, uint8_t data);
uint16_t at30tse75x_eeprom_size(at30tse75x_t* dev);


#ifdef __cplusplus
}
#endif

/** @} */
#endif /* AT30TSE75X_H_ */

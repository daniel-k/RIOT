/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_shell_commands
 * @{
 *
 * @file
 * @brief       Provides shell commands to test AT30TSE75x temperature sensor
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "at30tse75x.h"

//#ifdef MODULE_AT30TSE75X

static bool initialized = false;
static bool initialized_eeprom = false;
static at30tse75x_t dev;

static int16_t parse_hex(char* s)
{
    int16_t ret = -1;
    char* hex = strstr(s, "0x");
    if(hex) {
        ret = strtoul(hex+2, NULL, 16);
    }
    return ret;
}

int _at30tse75x_handler(int argc, char **argv)
{
    int error;
    int16_t hex;
    unsigned addr = 0;

    if(argc <= 1) {
        printf("Usage: %s init|read|mode|resolution|save|restore|config\n", argv[0]);
        return -1;
    }

    if(strcmp(argv[1], "init") == 0) {
        if(argc == 2) {
            printf("Usage: %s init #I2C [addr]\n"
                   "  e.g. %s init 0\n", argv[0], argv[0]);
            return -1;
        }

        /* Try to parse i2c dev */
        i2c_t i2c_dev = (i2c_t) strtol(argv[2], &argv[1] + 1, 10);

        /* Address given */
        if(argc == 4) {
            hex = parse_hex(argv[3]);
            if(hex >= 0) {
                addr = hex & 0xff;
            }
        }

        error = at30tse75x_init(&dev, i2c_dev, I2C_SPEED_NORMAL, addr);
        if (error) {
            printf("Error initializing AT30TSE75x sensor on I2C #%u @ 0x%x\n", i2c_dev, addr);
            initialized = false;
            initialized_eeprom = false;
            return 1;
        }
        else {
            printf("Initialized AT30TSE75x sensor on I2C #%u @ 0x%x\n", i2c_dev, addr);
            initialized = true;
        }
    } else {
        if(!initialized) {
            puts("Please initialize first");
            return -1;
        }
        if(strcmp(argv[1], "read") == 0) {
            float temperature;
            if(at30tse75x_get_temperature(&dev, &temperature) != 0) {
                puts("Reading temperature failed");
                return -1;
            }
            printf("Temperature: %i.%03u °C\n",
                    (int)temperature,
                    (unsigned)((temperature - (int)temperature) * 1000));
        } else if(strcmp(argv[1], "mode") == 0) {
            if(argc == 2) {
                printf("Usage: %s mode one-shot|comparator|interrupt\n", argv[0]);
                return -1;
            }
            at30tse75x_mode_t mode;
            if(strcmp(argv[2], "one-shot") == 0) {
                mode = AT30TSE75X_MODE_ONE_SHOT;
            } else if(strcmp(argv[2], "comparator") == 0) {
                mode = AT30TSE75X_MODE_COMPARATOR;
            } else if(strcmp(argv[2], "interrupt") == 0) {
                mode = AT30TSE75X_MODE_INTERRUPT;
            } else {
                puts("Invalid mode");
                return -1;
            }
            if(at30tse75x_set_mode(&dev, mode) != 0) {
                return -1;
            }
            printf("Mode set to %s\n", argv[2]);
        } else if(strcmp(argv[1], "resolution") == 0) {
            if(argc == 2) {
                printf("Usage: %s resolution 9|10|11|12\n", argv[0]);
                return -1;
            }
            unsigned resolution = strtoul(argv[2], NULL, 10);
            if(at30tse75x_set_resolution(&dev, resolution - 9) != 0) {
                return -1;
            }
            printf("Resolution set to %u bits\n", resolution);
        } else if(strcmp(argv[1], "save") == 0) {
            uint8_t config;
            if(at30tse75x_get_config(&dev, &config) != 0) {
                return -1;
            }
            if(at30tse75x_save_config(&dev) != 0) {
                return -1;
            }
            printf("Config (0x%x) saved\n", config);
        } else if(strcmp(argv[1], "restore") == 0) {
            uint8_t config;
            if(at30tse75x_restore_config(&dev) != 0) {
                return -1;
            }
            if(at30tse75x_get_config(&dev, &config) != 0) {
                return -1;
            }
            printf("Config restored to 0x%x\n", config);

        } else if(strcmp(argv[1], "config") == 0) {
            if(argc == 2) {
                uint8_t config;
                if(at30tse75x_get_config(&dev, &config) != 0) {
                    return -1;
                }
                printf("Config: 0x%x\n", config);
            } else {
                /* Try to parse config in hex format */
                uint8_t config;

                hex = parse_hex(argv[3]);
                if(hex < 0) {
                    printf("Usage: %s config 0x__"
                           "  to set config\n", argv[0]);
                    return -1;
                }
                config = hex & 0xff;
                if(at30tse75x_set_config(&dev, config) != 0) {
                    return -1;
                }
                printf("Config set to: 0x%x\n", config);
            }
        }
    }
    return 0;
}

int _at30tse75x_eeprom_handler(int argc, char **argv)
{
    int error;
    int16_t hex;
    unsigned addr = 0;

    if(argc <= 1) {
        printf("Usage: %s init|read|write\n", argv[0]);
        return -1;
    }

    if(!initialized ) {
        puts("Please initialize temperature sensor first");
        return -1;
    }

    if(strcmp(argv[1], "init") == 0) {
        if(argc == 2) {
            printf("Usage: %s init size_in_kb [addr]\n"
                   "  e.g. %s init 8 0x06\n", argv[0], argv[0]);
            return -1;
        }

        /* Try to parse size */
        int size_in_kb = strtol(argv[2], NULL, 10);

        switch(size_in_kb) {
        case 2:
            dev.eeprom_size = AT30TSE75X_EEPROM_2KB;
            break;
        case 4:
            dev.eeprom_size = AT30TSE75X_EEPROM_4KB;
            break;
        case 8:
            dev.eeprom_size = AT30TSE75X_EEPROM_8KB;
            break;
        default:
            return -1;
        }

        /* Address given */
        if(argc == 4) {
            hex = parse_hex(argv[3]);
            if(hex >= 0) {
                addr = hex & 0xff;
            }
        }

        error = at30tse75x_eeprom_init(&dev, addr, dev.eeprom_size);
        if (error) {
            printf("Error initializing AT30TSE75x EEPROM @ 0x%x\n", addr);
            initialized_eeprom = false;
            return 1;
        }
        else {
            printf("Initialized AT30TSE75x EEPROM @ 0x%x\n", addr);
            initialized_eeprom = true;
        }
    } else {
        if(!initialized_eeprom) {
            puts("Please initialize first");
            return -1;
        }

        if(strcmp(argv[1], "read") == 0) {
            if(argc == 2) {
                printf("Usage: %s read addr\n"
                       "  e.g. %s read 0x123\n", argv[0], argv[0]);
                return -1;
            }

            hex = parse_hex(argv[2]);
            if( (hex < 0) || (hex >= at30tse75x_eeprom_size(&dev)) ) {
                puts("Address out of range");
                return -1;
            }
            addr = hex;

            uint8_t data;
            if(at30tse75x_eeprom_read(&dev, addr, &data) != 0) {
                puts("Read failed");
                return -1;
            }

            printf("0x%04x: 0x%02x\n", addr, data);

        } else if(strcmp(argv[1], "write") == 0) {
            if(argc == 2 || argc == 3) {
                printf("Usage: %s write addr data\n"
                       "  e.g. %s write 0x123 0xef\n", argv[0], argv[0]);
                return -1;
            }

            hex = parse_hex(argv[2]);
            if( (hex < 0) || (hex >= at30tse75x_eeprom_size(&dev)) ) {
                puts("Address out of range");
                return -1;
            }
            addr = hex;

            hex = parse_hex(argv[3]);
            if(hex < 0) {
                return -1;
            }

            if(at30tse75x_eeprom_write(&dev, addr, (uint8_t)hex) != 0) {
                puts("Write failed");
                return -1;
            }
            puts("Write successful");
        } else if(strcmp(argv[1], "dump") == 0) {
            uint8_t buffer[16];
            puts("");
            for(int i = 0; i < at30tse75x_eeprom_size(&dev); i += 16) {
                if(at30tse75x_eeprom_reads(&dev, i, buffer, 16) != 0) {
                    printf("Reading 0x%04x - 0x%04x failed\n", i, i+15);
                    continue;
                }
                printf("0x%04x:  ", i);
                for(int k = 0; k < 16; k++) {
                    printf("0x%02x ", buffer[k]);
                }
                puts("");
            }
        }
    }
    return 0;
}

//#endif /* MODULE_AT30TSE75X */

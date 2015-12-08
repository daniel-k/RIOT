/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Test for low-level non-volatile memory driver
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 *
 * @}
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <periph/nvm.h>
#include "shell.h"
#include "msg.h"

extern char _sota;
extern char _eota;

char test_string[] = "Hello World!";

int _nvm_handler(int argc, char **argv)
{
    if(argc == 1) {
        puts("Usage: nvm read|write|erase");
        return -1;
    }

    if(&_sota != row_to_addr(next_row(&_sota))) {
        puts("Start of section '.ota' is not row-aligned");
        return -1;
    }

    if(strcmp(argv[1], "read") == 0) {
        uint32_t len = 16;
        unsigned char cksum = 0;

        /* Get read length */
        if(argc == 3) {
            int new_len = atoi(argv[2]);
            if(new_len > 0) {
                len = new_len;
            }
        }

        puts("NVM read");
        puts("--------");
        printf("Reading first %"PRIu32" bytes at %p:", len, &_sota);

        for(int offset = 0; offset < len; offset++)
        {
            if(offset % 16 == 0) {
                printf("\n%p: ", &((&_sota)[offset]));
            }
            printf("0x%02x ", (&_sota)[offset]);
            cksum += (&_sota)[offset];
        }
        printf("\nDone, checksum: 0x%02x\n", cksum);

    } else if(strcmp(argv[1], "erase") == 0) {
        uint32_t len = 16;

        /* Get erase length */
        if(argc == 3) {
            int new_len = atoi(argv[2]);
            if(new_len > 0) {
                len = new_len;
            }
        }

        puts("NVM erase");
        puts("---------");
        printf("Erasing first %"PRIu32" bytes at %p:\n", len, &_sota);

        puts("TODO: nothing is done here right now");
//        if((char*)row < &_sota) {
//            puts("Would erase row of .text section, abort!");
//            printf("&_sota:    %p\n", &_sota);
//            printf("row start: %p\n", row);
//            return 1;
//        }

    } else if(strcmp(argv[1], "write") == 0) {

        size_t len = sizeof(test_string);
        puts("NVM write");
        puts("---------");
        printf("Writing %d bytes to %p\n", len, &_sota);
        nvm_write_erase(&_sota, test_string, len);
    } else {
        printf("unknown command '%s'\n", argv[1]);
        return -1;
    }
    return 0;
}

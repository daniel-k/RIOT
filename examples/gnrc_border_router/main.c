/*
 * Copyright (C) 2015 Freie Universität Berlin
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
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "shell.h"
#include "msg.h"
#include "net/ipv6.h"
#include "net/gnrc/ipv6/netif.h"
#include "net/gnrc/ipv6/nc.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT border router example application");

    ipv6_addr_t ip_addr;

    /* Set IP for SLIP interface */
    ipv6_addr_from_str(&ip_addr, "affe::2");
    gnrc_ipv6_netif_add_addr(6, &ip_addr, 0, 0);

    /* Set IP for 802.15.4 interface */
//    ipv6_addr_from_str(&ip_addr, "affe::3");
//    gnrc_ipv6_netif_add_addr(5, &ip_addr, 0, 0);

    /* Set static neighbour cache for SLIP tunnel */
    ipv6_addr_from_str(&ip_addr, "affe::1");
    gnrc_ipv6_nc_add(6, &ip_addr, 0, 0, 0);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

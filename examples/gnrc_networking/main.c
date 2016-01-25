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
#include <string.h>

#include "shell.h"
#include "msg.h"
#include "net/ipv6.h"
#include "net/gnrc/ipv6/netif.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

extern int udp_cmd(int argc, char **argv);

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { NULL, NULL, NULL }
};

int main(void)
{
    ipv6_addr_t ip_addr;

	/* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    // Board discovery
    if(strcmp("ATML2127031800001673", SERIAL) == 0) {
		puts("I'm board 1");
		ipv6_addr_from_str(&ip_addr, "affe::11");

    } else if(strcmp("ATML2127031800004622", SERIAL) == 0) {
		puts("I'm board 2");
		ipv6_addr_from_str(&ip_addr, "affe::12");
    } else {
		printf("Unknown SERIAL = '%s'. Abort!\n", SERIAL);
    }

    // Sleep because neighbour discovery is not ready yet and IP would not be
    // broadcasted
    xtimer_sleep(1);

    // Register new address
	gnrc_ipv6_netif_add_addr(7, &ip_addr, 64, GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST);


    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

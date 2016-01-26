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
#include <thread.h>
#include <net/af.h>
#include <net/conn/udp.h>
#include <stdint.h>

#include "shell.h"
#include "msg.h"
#include "net/ipv6.h"
#include "net/gnrc/ipv6/netif.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static msg_t _msg_q[MAIN_QUEUE_SIZE];

static ipv6_addr_t ip_addr;

extern int udp_cmd(int argc, char **argv);

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { NULL, NULL, NULL }
};

#define LISTEN_PORT (1234)

static char thread_stack[1024];
static char ip_str[IPV6_ADDR_MAX_STR_LEN];

void* _thread(void* arg)
{
	conn_udp_t conn;
	char buf[128];


    msg_init_queue(_msg_q, MAIN_QUEUE_SIZE);


	uint8_t laddr[16] = { 0 };


	uint8_t src_addr[16] = { 0 };
	size_t src_addr_len;
	uint16_t src_port = LISTEN_PORT;

	conn_udp_create(&conn, laddr, sizeof(laddr), AF_INET6, LISTEN_PORT);



	while(1) {
		puts("Thread running, waiting for UDP packets ...");

		int res = conn_udp_recvfrom (&conn, buf, sizeof(buf), src_addr, &src_addr_len, &src_port);

		if(res < 0) {
			puts("Error: something went wrong");
		} else {
			ipv6_addr_to_str(ip_str, (ipv6_addr_t*) src_addr, sizeof(ip_str));

			printf("Received %d bytes on port %d from %s\n", res, src_port, ip_str);

			// Answer back with same payload
			conn_udp_sendto(buf, res, NULL, 0, src_addr, src_addr_len, AF_INET6, 42, LISTEN_PORT);
		}

//		xtimer_sleep(1);
	}


	return NULL;
}


int main(void)
{


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


	thread_create(thread_stack, sizeof(thread_stack), 8, THREAD_CREATE_STACKTEST, _thread, NULL, "thread-test");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

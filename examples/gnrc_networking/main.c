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

#include "net/gnrc/tftp.h"
#include "net/gnrc/netreg.h"
#include "thread.h"

extern int udp_cmd(int argc, char **argv);

static int tftp_handle(int argc, char **argv)
{
    (void) argv;

    if (argc == 1) {
        ipv6_addr_t ip;
        //const char *dst = "fdcb:61::1";
        const char *dst = "fdcb:172:31::1:254";
        //const char *dst = "fe80::407b:92ff:fe7b:dd88";
        uint16_t port = GNRC_TFTP_DEFAULT_SRC_PORT;
        ipv6_addr_from_str(&ip, dst);
        gnrc_tftp_test_connect(&ip, port);
    } else if (argc == 2) {
        gnrc_netreg_entry_t entry;
        entry.next = NULL;
        entry.pid = thread_getpid();
        entry.demux_ctx = GNRC_TFTP_DEFAULT_SRC_PORT;

        /* register our DNS response listener */
        if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
            puts("error: tftp reg failed");
        }
    }

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { "tftp", "TFTP test function", tftp_handle },
    { NULL, NULL, NULL }
};

int main(void)
{
    puts("RIOT network stack example application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

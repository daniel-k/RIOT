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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define TFTP_QUEUE_SIZE     (4)
static msg_t _tftp_msg_queue[TFTP_QUEUE_SIZE];

#include "net/gnrc/tftp.h"
#include "net/gnrc/netreg.h"
#include "thread.h"

//const char *addr = "fe80::44:d0ff:fe21:661b";
const char *addr = "affe::1";

extern ssize_t write(int fildes, const void *buf, size_t nbyte);
extern int udp_cmd(int argc, char **argv);

static int _tftp_data_cb(uint32_t offset, void *data, uint32_t data_len) {
    char *c = (char*) data;

    (void) offset;

    return write(STDOUT_FILENO, c, data_len);
}

static bool _tftp_start_cb(tftp_action_t action, const char *file_name, size_t len) {
    printf("tftp: %s %s:%u\n", action == TFTP_READ ? "read" : "write", file_name, len);

    return true;
}

static int tftp_handle(int argc, char **argv)
{
    (void) argv;

    if (argc == 2) {
        ipv6_addr_t ip;
        ipv6_addr_from_str(&ip, addr);
        gnrc_tftp_client_read(&ip, "welcome.txt", _tftp_data_cb, _tftp_start_cb);
    } else if (argc == 1) {
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

const char* tftp_name = "tftp";
char tftp_stack[2048];

void* tftp_server_wrapper(void* arg)
{
    /* A message queue is needed to register for incoming packets */
    msg_init_queue(_tftp_msg_queue, TFTP_QUEUE_SIZE);

    puts("Starting TFTP server at port 69");
    gnrc_tftp_server(_tftp_data_cb, _tftp_start_cb);
    puts("TFTP server terminated");
    return NULL;
}

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];

//    ipv6_addr_t ip;
//    ipv6_addr_from_str(&ip, addr);
//    gnrc_tftp_client_read(&ip, "welcome.txt", _tftp_data_cb, _tftp_start_cb);

    thread_create(tftp_stack, sizeof(tftp_stack),
            8,
            CREATE_WOUT_YIELD | CREATE_STACKTEST,
            tftp_server_wrapper, NULL, tftp_name);

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

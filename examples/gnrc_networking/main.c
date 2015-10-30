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
#include <string.h>

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define TFTP_QUEUE_SIZE     (8)
static msg_t _tftp_msg_queue[TFTP_QUEUE_SIZE];

#include "net/gnrc/tftp.h"
#include "net/gnrc/netreg.h"
#include "thread.h"

const char hello_world[] = "Hallo World,\
\
Welcome writing shit like a mofo to this TFTP server with loads of features";

const char *addr = "affe::2";

extern ssize_t write(int fildes, const void *buf, size_t nbyte);
extern int udp_cmd(int argc, char **argv);

tftp_action_t currentAction;

static int _tftp_data_client_cb(uint32_t offset, void *data, size_t data_len) {
    char *c = (char*) data;

    if (currentAction == TFTP_WRITE)
    {
        if (offset + data_len > sizeof(hello_world))
            data_len -= (offset + data_len) - sizeof(hello_world);

        memcpy(data, hello_world + offset, data_len);

        return data_len;
    }
    else
        return write(STDOUT_FILENO, c, data_len);
}

static int _tftp_data_server_cb(uint32_t offset, void *data, size_t data_len) {
    char *c = (char*) data;

    if (currentAction == TFTP_READ)
    {
        if (offset + data_len > sizeof(hello_world))
            data_len -= (offset + data_len) - sizeof(hello_world);

        memcpy(data, hello_world + offset, data_len);

        return data_len;
    }
    else
        return write(STDOUT_FILENO, c, data_len);
}

static bool _tftp_start_cb(tftp_action_t action, const char *file_name, size_t len) {
    printf("tftp: %s %s:%u\n", action == TFTP_READ ? "read" : "write", file_name, len);

    currentAction = action;

    return true;
}

static int _tftp_handler(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    ipv6_addr_t ip;
    ipv6_addr_from_str(&ip, addr);
    puts("Trying to read with options enabled");
    gnrc_tftp_client_read(&ip, "welcome.txt", _tftp_data_client_cb, _tftp_start_cb, true);

    puts("Trying to read with options disabled");
    gnrc_tftp_client_read(&ip, "welcome.txt", _tftp_data_client_cb, _tftp_start_cb, false);

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { "tftp", "test TFTP read", _tftp_handler },
    { NULL, NULL, NULL }
};

const char* tftp_name = "tftp";
char tftp_stack[THREAD_STACKSIZE_MAIN + THREAD_EXTRA_STACKSIZE_PRINTF];

void* tftp_server_wrapper(void* arg)
{
    (void)arg;

    /* A message queue is needed to register for incoming packets */
    msg_init_queue(_tftp_msg_queue, TFTP_QUEUE_SIZE);

    puts("Starting TFTP server at port 69");
    gnrc_tftp_server(_tftp_data_server_cb, _tftp_start_cb);
    puts("TFTP server terminated");
    return NULL;
}

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    thread_create(tftp_stack, sizeof(tftp_stack),
            1,
            CREATE_WOUT_YIELD | CREATE_STACKTEST,
            tftp_server_wrapper, NULL, tftp_name);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

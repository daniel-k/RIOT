/*
 * Copyright (C) 2015 Nick van IJzendoorn <nijzendoorn@engineering-spirit.nl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for
 * more details.
 */

/**
 * @ingroup net_gnrc_tftp
 * @{
 *
 * @file
 *
 * @author      Nick van IJzendoorn <nijzendoorn@engineering-spirit.nl>
 */

#include <errno.h>
#include <string.h>
#include <time.h>

#include "net/gnrc/tftp/internal.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/udp.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

int gnrc_tftp_client_read(ipv6_addr_t *addr, const char *file_name,
                          tftp_data_callback cb) {
    tftp_context_t ctxt;

    /* prepare the context */
    if (_tftp_init_ctxt(addr, file_name, cb, &ctxt, TO_RRQ) != FINISHED) {
        return -EINVAL;
    }

    /* set the transfer options */
    if (_tftp_set_opts(&ctxt, 10, 1, 0) != FINISHED) {
        return -EINVAL;
    }

    /* start the process */
    return _tftp_do_client_transfer(&ctxt);
}

int gnrc_tftp_client_write(ipv6_addr_t *addr, const char *file_name,
                           tftp_data_callback cb, uint32_t total_size) {
    tftp_context_t ctxt;

    /* prepare the context */
    if (_tftp_init_ctxt(addr, file_name, cb, &ctxt, TO_RWQ) < 0) {
        return -EINVAL;
    }

    /* set the transfer options */
    if (_tftp_set_opts(&ctxt, 10, 1, total_size) != FINISHED) {
        return -EINVAL;
    }

    /* start the process */
    return _tftp_do_client_transfer(&ctxt);
}

/**
 * @}
 */

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

void gnrc_tftp_test_connect(ipv6_addr_t *addr, uint16_t port) {

    uint8_t packet[1028];

    gnrc_pktsnip_t *payload, *udp, *ip;

    int length = _tftp_create_start(packet, TO_RRQ, "welcome.txt", OCTET);

    /* allocate payload */
    payload = gnrc_pktbuf_add(NULL, packet, length, GNRC_NETTYPE_UNDEF);
    if (payload == NULL) {
        DEBUG("dns: error unable to copy data to packet buffer");
        return;
    }

    /* allocate UDP header, set source port := destination port */
    network_uint16_t src_port, dst_port;
    src_port.u16 = port;
    dst_port.u16 = GNRC_TFTP_DEFAULT_DST_PORT;
    udp = gnrc_udp_hdr_build(payload, src_port.u8, sizeof(src_port),
            dst_port.u8, sizeof(dst_port));
    if (udp == NULL) {
        DEBUG("dns: error unable to allocate UDP header");
        gnrc_pktbuf_release(payload);
        return;
    }

    /* allocate IPv6 header */
    ip = gnrc_ipv6_hdr_build(udp, NULL, 0, addr->u8, sizeof(ipv6_addr_t));
    if (ip == NULL) {
        DEBUG("dns: error unable to allocate IPv6 header");
        gnrc_pktbuf_release(udp);
        return;
    }

    /* send packet */
#if 1
    if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip)) {
        DEBUG("dns: error unable to locate UDP thread");
        gnrc_pktbuf_release(ip);
        return;
    }
#else
    if (!gnrc_netapi_send(7, ip)) {
        DEBUG("dns: error unable to locate UDP thread");
        gnrc_pktbuf_release(ip);
        return;
    }
#endif

}


/**
 * @}
 */

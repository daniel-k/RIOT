/*
 * Copyright (C) 2015 Daniel Krebs
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net_lwmac
 * @file
 * @brief       Implementation of RX state machine
 *
 * @author      Daniel Krebs <github@daniel-krebs.net>
 * @}
 */

#include <net/gnrc.h>
#include <net/gnrc/lwmac/lwmac.h>
#include <net/gnrc/lwmac/packet_queue.h>

#include "include/rx_state_machine.h"
#include "include/timeout.h"
#include "include/lwmac_internal.h"
#include "include/lwmac_types.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#define LOG_LEVEL LOG_INFO
#include "log.h"

#undef LOG_ERROR
#undef LOG_WARNING
#undef LOG_INFO
#undef LOG_DEBUG

#define LOG_ERROR(...) LOG(LOG_ERROR, "ERROR: [lwmac-rx] " __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOG_WARNING, "WARNING: [lwmac-rx] " __VA_ARGS__)
#define LOG_INFO(...) LOG(LOG_INFO, "[lwmac-rx] " __VA_ARGS__)
#define LOG_DEBUG(...) LOG(LOG_DEBUG, "[lwmac-rx] " __VA_ARGS__)

/* Break out of switch and mark the need for rescheduling */
#define GOTO_RX_STATE(rx_state, do_resched) lwmac->rx.state = rx_state; \
                                reschedule = do_resched; \
                                break

/******************************************************************************/

void lwmac_rx_start(lwmac_t* lwmac)
{
    if(lwmac == NULL)
        return;

    /* RX address should have been reset, probably not stopped then */
    assert(lwmac->rx.l2_addr.len == 0);

    lwmac->rx.state = RX_STATE_INIT;
}

/******************************************************************************/

void lwmac_rx_stop(lwmac_t* lwmac)
{
    if(!lwmac)
        return;

    lwmac_reset_timeouts(lwmac);
    lwmac->rx.state = RX_STATE_STOPPED;
    lwmac->rx.l2_addr.len = 0;
}

/******************************************************************************/

/* Returns whether rescheduling is needed or not */
static bool _lwmac_rx_update(lwmac_t* lwmac)
{
    bool reschedule = false;

    if(!lwmac)
        return reschedule;

    switch(lwmac->rx.state)
    {
    case RX_STATE_INIT:
        lwmac_reset_timeouts(lwmac);
        GOTO_RX_STATE(RX_STATE_WAIT_FOR_WR, true);

    case RX_STATE_WAIT_FOR_WR:
    {
        LOG_DEBUG("RX_STATE_WAIT_FOR_WR\n");

        gnrc_pktsnip_t* pkt;
        bool found_wr = false;

        while( (pkt = packet_queue_pop(&lwmac->rx.queue)) != NULL ) {
            LOG_DEBUG("Inspecting pkt @ %p\n", pkt);

            /* Dissect lwMAC header */
            gnrc_pktbuf_mark(pkt, sizeof(lwmac_hdr_t), GNRC_NETTYPE_LWMAC);

            /* Parse packet */
            lwmac_packet_info_t info;
            int ret = _parse_packet(pkt, &info);

            /* All info needed is parsed, so release pkt already */
            gnrc_pktbuf_release(pkt);

            if(ret != 0) {
                LOG_DEBUG("Packet could not be parsed: %i\n", ret);
                continue;
            }

            if(info.header.type != FRAMETYPE_WR) {
                LOG_DEBUG("Packet is not WR: 0x%02x\n", info.header.type);
                continue;
            }

            if(!_addr_match(&lwmac->l2_addr, &info.dst_addr)) {
                LOG_DEBUG("Packet is WR but not for us\n");
                continue;
            }

            /* Save source address for later addressing */
            lwmac->rx.l2_addr = info.src_addr;

            found_wr = true;
            break;
        }

        if(!found_wr) {
            LOG_DEBUG("No WR found, stop RX\n");
            GOTO_RX_STATE(RX_STATE_FAILED, true);
        }

        // TODO: don't flush queue
        packet_queue_flush(&lwmac->rx.queue);

        GOTO_RX_STATE(RX_STATE_SEND_WA, true);
    }
    case RX_STATE_SEND_WA:
    {
        LOG_DEBUG("RX_STATE_SEND_WA\n");

        gnrc_pktsnip_t* pkt;
        gnrc_netif_hdr_t* nethdr_wa;

        assert(lwmac->rx.l2_addr.len != 0);

        /* Assemble WA packet */
        lwmac_hdr_t lwmac_hdr = {FRAMETYPE_WA};

        pkt = gnrc_pktbuf_add(NULL, &lwmac_hdr, sizeof(lwmac_hdr), GNRC_NETTYPE_LWMAC);
        if(pkt == NULL) {
            LOG_ERROR("Cannot allocate pktbuf of type GNRC_NETTYPE_LWMAC\n");
            GOTO_RX_STATE(RX_STATE_FAILED, true);
        }

        pkt = gnrc_pktbuf_add(pkt, NULL, sizeof(gnrc_netif_hdr_t) + lwmac->rx.l2_addr.len, GNRC_NETTYPE_NETIF);
        if(pkt == NULL) {
            LOG_ERROR("Cannot allocate pktbuf of type GNRC_NETTYPE_NETIF\n");
            GOTO_RX_STATE(RX_STATE_FAILED, true);
        }

        /* We wouldn't get here if add the NETIF header had failed, so no
           sanity checks needed */
        nethdr_wa = (gnrc_netif_hdr_t*) _gnrc_pktbuf_find(pkt, GNRC_NETTYPE_NETIF);

        /* Construct NETIF header and insert address for WA packet */
        gnrc_netif_hdr_init(nethdr_wa, 0, lwmac->rx.l2_addr.len);
        gnrc_netif_hdr_set_dst_addr(nethdr_wa, lwmac->rx.l2_addr.addr, lwmac->rx.l2_addr.len);

        /* Disable Auto ACK */
        netopt_enable_t autoack = NETOPT_DISABLE;
        lwmac->netdev->driver->set(lwmac->netdev, NETOPT_AUTOACK, &autoack, sizeof(autoack));

        if(_get_netdev_state(lwmac) == NETOPT_STATE_RX) {
            LOG_WARNING("Receiving now, so cancel sending WA\n");
            gnrc_pktbuf_release(pkt);
            GOTO_RX_STATE(RX_STATE_WAIT_FOR_DATA, false);
            break;
        }

        /* Send WA */
        lwmac->netdev->driver->send_data(lwmac->netdev, pkt);
        _set_netdev_state(lwmac, NETOPT_STATE_TX);

        /* Enable Auto ACK again for data reception */
        autoack = NETOPT_ENABLE;
        lwmac->netdev->driver->set(lwmac->netdev, NETOPT_AUTOACK, &autoack, sizeof(autoack));

        GOTO_RX_STATE(RX_STATE_WAIT_WA_SENT, false);
    }
    case RX_STATE_WAIT_WA_SENT:
    {
        LOG_DEBUG("RX_STATE_WAIT_WA_SENT\n");

        if(lwmac->tx_feedback == TX_FEEDBACK_UNDEF) {
            LOG_DEBUG("WA not yet completely sent\n");
            break;
        }

        /* WA wasn't sent, so restart state machine */
        if(lwmac->tx_feedback == TX_FEEDBACK_BUSY) {
            LOG_WARNING("WA could not be sent. Wait for next WR\n");
            GOTO_RX_STATE(RX_STATE_FAILED, true);
        }

        /* Set timeout for expected data arrival */
        timex_t interval = {0, LWMAC_DATA_DELAY_US};
        lwmac_set_timeout(lwmac, TIMEOUT_DATA, &interval);

        GOTO_RX_STATE(RX_STATE_WAIT_FOR_DATA, false);
    }
    case RX_STATE_WAIT_FOR_DATA:
    {
        LOG_DEBUG("RX_STATE_WAIT_FOR_DATA\n");

        gnrc_pktsnip_t* pkt;
        bool found_data = false;
        bool found_wr = false;

        while( (pkt = packet_queue_pop(&lwmac->rx.queue)) != NULL ) {
            LOG_DEBUG("Inspecting pkt @ %p\n", pkt);

            /* Dissect lwMAC header */
            gnrc_pktbuf_mark(pkt, sizeof(lwmac_hdr_t), GNRC_NETTYPE_LWMAC);

            /* Parse packet */
            lwmac_packet_info_t info;
            int ret = _parse_packet(pkt, &info);

            if(ret != 0) {
                LOG_DEBUG("Packet could not be parsed: %i\n", ret);
                gnrc_pktbuf_release(pkt);
                continue;
            }

            if(!_addr_match(&lwmac->rx.l2_addr, &info.src_addr)) {
                LOG_DEBUG("Packet is not from destination\n");
                gnrc_pktbuf_release(pkt);
                continue;
            }

            if(!_addr_match(&lwmac->l2_addr, &info.dst_addr)) {
                LOG_DEBUG("Packet is not for us\n");
                gnrc_pktbuf_release(pkt);
                continue;
            }

            /* Sender maybe didn't get the WA */
            if(info.header.type == FRAMETYPE_WR) {
                LOG_INFO("Found a WR while waiting for DATA\n");
                lwmac_clear_timeout(lwmac, TIMEOUT_DATA);
                found_wr = true;
                break;
            }

            if(info.header.type == FRAMETYPE_DATA) {
                LOG_DEBUG("Found DATA!\n");
                lwmac_clear_timeout(lwmac, TIMEOUT_DATA);
                found_data = true;
                break;
            }
        }

        /* If WA got lost we wait for data but we will be hammered with WR
         * packets. So a WR indicates a lost WA => reset RX state machine
         */
        if(found_wr) {
            LOG_INFO("WA probably got lost, reset RX state machine\n");
            /* Push WR back to rx queue */
            packet_queue_push(&lwmac->rx.queue, pkt, 0);
            /* Start over again */
            GOTO_RX_STATE(RX_STATE_INIT, true);
        }

        /* Only timeout if no packet (presumably the expected data) is being
         * received. This won't be blocked by WRs as they restart the state
         * machine (see above). */
        if( (lwmac_timeout_is_expired(lwmac, TIMEOUT_DATA)) &&
            (!lwmac->rx_started) ) {
            LOG_ERROR("DATA timed out\n");
            GOTO_RX_STATE(RX_STATE_FAILED, true);
        }

        if(!found_data) {
            LOG_DEBUG("No DATA yet\n");
            break;
        }

        if (!gnrc_netapi_dispatch_receive(pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
            LOG_INFO("Unable to forward packet of type %i\n", pkt->type);
            gnrc_pktbuf_release(pkt);
        }

        GOTO_RX_STATE(RX_STATE_SUCCESSFUL, true);
    }
    case RX_STATE_SUCCESSFUL:
    case RX_STATE_FAILED:
        lwmac_reset_timeouts(lwmac);
        break;
    case RX_STATE_STOPPED:
        LOG_DEBUG("Reception state machine is stopped\n");
    }

    return reschedule;
}

/******************************************************************************/

void lwmac_rx_update(lwmac_t* lwmac)
{
    /* Update until no rescheduling needed */
    while(_lwmac_rx_update(lwmac));
}

/******************************************************************************/

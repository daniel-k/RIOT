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
#include <stdlib.h>
#include <time.h>

#include "net/gnrc/tftp/internal.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/ipv6.h"
#include "random.h"

#define TFTP_TIMEOUT_MSG                    0x4000
#define TFTP_DEFAULT_DATA_SIZE              (GNRC_TFTP_MAX_TRANSFER_UNIT + sizeof(tftp_packet_data_t))
#define MSG(s)                              #s, sizeof(#s)

/**
 * @brief TFTP mode help support
 */
#define MODE(mode)                  { #mode, sizeof(#mode) }

typedef struct {
    char *name;
    uint8_t len;
} tftp_opt_t;

/* ordered as @see tftp_mode_t */
tftp_opt_t _tftp_modes[] = {
        MODE(netascii),
        MODE(octet),
        MODE(mail),
};

/* ordered as @see tftp_options_t */
tftp_opt_t _tftp_options[] = {
        MODE(blksize),
        MODE(tsize),
        MODE(timeout),
};

int _tftp_init_ctxt(ipv6_addr_t *addr, const char *file_name,
                    tftp_data_callback cb, tftp_context_t *ctxt,
                    tftp_opcodes_t op) {

    if (!addr || !file_name || !cb) {
        return -EINVAL;
    }

    memset(ctxt, 0, sizeof(tftp_context_t));

    /* set the default context parameters */
    ctxt->op = op;
    ctxt->cb = cb;
    ctxt->peer = addr;
    ctxt->mode = OCTET;
    ctxt->file_name = file_name;
    ctxt->dst_port = GNRC_TFTP_DEFAULT_DST_PORT;

    /* transport layer parameters */
    ctxt->block_size = 10;
    ctxt->block_timeout = 10;

    /* generate a random source UDP source port */
    do {
        ctxt->src_port = (genrand_uint32() & 0xff) + GNRC_TFTP_DEFAULT_SRC_PORT;
    } while (gnrc_netreg_num(GNRC_NETTYPE_UDP, ctxt->src_port));

    return 0;
}

int _tftp_do_client_transfer(tftp_context_t *ctxt) {
    msg_t msg;
    int ret = 0;

    /* register our DNS response listener */
    gnrc_netreg_entry_t entry = { NULL, ctxt->src_port, thread_getpid() };
    if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
        DEBUG("tftp: error starting server.");
        return -1;
    }

    /* try to start the TFTP transfer */
    ret = _tftp_state_processes(ctxt, NULL);
    if (ret < 0) {
        /* if the start failed return */
        return ret;
    }

    /* main processing loop */
    while (ret > 0) {
        /* wait for a message */
        msg_receive(&msg);
        ret = _tftp_state_processes(ctxt, &msg);
    }

    /* unregister our UDP listener on this thread */
    gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &entry);

    return 0;
}

int _tftp_send(gnrc_pktsnip_t *buf, tftp_context_t *ctxt, size_t len) {
    network_uint16_t src_port, dst_port;
    gnrc_pktsnip_t *udp, *ip;

    if (len > TFTP_DEFAULT_DATA_SIZE) {
        DEBUG("tftp: can't reallocate to bigger packet, buffer overflow occurred");
        return -1;
    }
    else if (gnrc_pktbuf_realloc_data(buf, len) != 0) {
        DEBUG("tftp: failed to reallocate data snippet");
        return -1;
    }

    /* allocate UDP header, set source port := destination port */
    src_port.u16 = ctxt->src_port;
    dst_port.u16 = ctxt->dst_port;
    udp = gnrc_udp_hdr_build(buf, src_port.u8, sizeof(src_port),
            dst_port.u8, sizeof(dst_port));
    if (udp == NULL) {
        DEBUG("dns: error unable to allocate UDP header");
        gnrc_pktbuf_release(buf);
        return -1;
    }

    /* allocate IPv6 header */
    ip = gnrc_ipv6_hdr_build(udp, NULL, 0, ctxt->peer->u8, sizeof(ipv6_addr_t));
    if (ip == NULL) {
        DEBUG("dns: error unable to allocate IPv6 header");
        gnrc_pktbuf_release(udp);
        return -1;
    }

    /* send packet */
    if (gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip) == 0) {
        /* if send failed inform the user */
        DEBUG("dns: error unable to locate UDP thread");
        gnrc_pktbuf_release(ip);
        return -1;
    }

    //vtimer_set_msg(&(ctxt->timer), timex_set(ctxt->timeout, 0), thread_getpid(), TFTP_TIMEOUT_MSG, NULL);

    return len;
}

int _tftp_state_processes(tftp_context_t *ctxt, msg_t *m) {
    gnrc_pktsnip_t *outbuf = gnrc_pktbuf_add(NULL, NULL, TFTP_DEFAULT_DATA_SIZE, GNRC_NETTYPE_UNDEF);

    /* check if this is an client start */
    if (!m) {
        return _tftp_send_start(ctxt, outbuf);
    }
    else if (m->type == TFTP_TIMEOUT_MSG) {
        /* the send message timed out, re-sending */
        if (ctxt->dst_port == GNRC_TFTP_DEFAULT_DST_PORT) {
            /* we are still negotiating resent, start */
            return _tftp_send_start(ctxt, outbuf);
        }
        else {
            /* we are sending / receiving data */
            if (ctxt->op == TO_RRQ) {
                /* we are reading so send the ACK again */
                return _tftp_send_ack(ctxt, outbuf);
            }
            else {
                /* we are writing so send the data again */
                return _tftp_send_data(ctxt, outbuf);
            }
        }
    }
    else if (m->type != GNRC_NETAPI_MSG_TYPE_RCV) {
        DEBUG("tftp: unknown message");
        return 1;
    }

    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t*)(m->content.ptr);
    uint8_t *data = (uint8_t*)pkt->data;
    udp_hdr_t *hdr = (udp_hdr_t*)pkt->next->data;

    switch (_tftp_parse_type(data)) {
    case TO_RRQ:
    case TO_RWQ: {
        /* TODO when in server mode this indicates that a clients requests a transfer start */
#if 0
        ctxt->op = _tftp_parse_type(data);
        _tftp_decode_start(data, &(ctxt->file_name), &(ctxt->mode));

        /* TODO send ACK if valid, ERROR if not */
        _tftp_send_ack(data, 0);
        _tftp_send_error(data, TE_UN_DEF, "");
#endif
    } break;

    case TO_DATA: {
        /* try to process the data */
        int proc = _tftp_process_data(ctxt, pkt);
        if (proc < 0) {
            /* the data is not accepted return */
            return 1;
        }

        /* check if this is the first block */
        if (!ctxt->block_nr) {
            /* TODO data if no opt ack received set default values */
            ctxt->dst_port = byteorder_ntohs(hdr->src_port);
        }

        /* wait for the next data block */
        ++(ctxt->block_nr);
        _tftp_send_ack(ctxt, outbuf);

        /* check if the data transfer has finished */
        if (proc < ctxt->block_size) {
            return 0;
        }

        return 1;
    } break;

    case TO_ACK: {
        /* validate if this is the ACK we are waiting for */
        if (!_tftp_validate_ack(ctxt, data)) {
            /* invalid packet ACK, drop */
            return -1;
        }

        /* check if this is the first received data and switch port numbers if that is the case */
        if (!ctxt->block_nr) {
            /* TODO data if no opt ack received set default values */
           ctxt->dst_port = byteorder_ntohs(hdr->src_port);
        }

        /* send the next data block */
        ++(ctxt->block_nr);
        return _tftp_send_data(ctxt, outbuf);
    } break;

    case TO_ERROR: {
        tftp_err_codes_t err;
        const char *err_msg;

        _tftp_decode_error(data, &err, &err_msg);

        /* TODO inform the user that there is an error */
    } break;

    case TO_OACK: {
        /* decode the options */
        _tftp_decode_options(ctxt, pkt);

        /* take the new source port */
        ctxt->dst_port = byteorder_ntohs(hdr->src_port);

        /* send and ACK that we accept the options */
        _tftp_send_ack(ctxt, outbuf);

        return 1;
    } break;
    }

    return -EINVAL;
}

int _tftp_send_start(tftp_context_t *ctxt, gnrc_pktsnip_t *buf)
{
    /* get required values */
    int len = strlen(ctxt->file_name) + 1;          /* we also want the \0 char */
    tftp_opt_t *m = _tftp_modes + ctxt->mode;

    /* start filling the header */
    tftp_header_t *hdr = (tftp_header_t*)(buf->data);
    hdr->opc = ctxt->op;
    memcpy(hdr->data, ctxt->file_name, len);
    memcpy(hdr->data + len, m->name, m->len);

    /* fill the options */
    uint32_t offset = (len + m->len);
    for (int idx = 0; idx < 1 /*ARRAY_LEN(_tftp_options)*/; ++idx) {
        tftp_opt_t *opt = _tftp_options + idx;

        /* set the option name */
        memcpy(hdr->data + offset, opt->name, opt->len);
        offset += opt->len;

        /* set the option value */
        offset += sprintf((char*)(hdr->data + offset), "%d", 10);

        /* finish option value */
        *(hdr->data + offset) = 0;
        ++offset;
    }

    /* send the data */
    return _tftp_send(buf, ctxt, offset + sizeof(tftp_header_t));
}

int _tftp_send_ack(tftp_context_t *ctxt, gnrc_pktsnip_t *buf)
{
    /* fill the packet */
    tftp_packet_data_t *pkt = (tftp_packet_data_t*)(buf->data);
    pkt->opc = TO_ACK;
    pkt->block_nr = HTONS(ctxt->block_nr);

    /* send the data */
    return _tftp_send(buf, ctxt, sizeof(tftp_packet_data_t));
}

int _tftp_send_data(tftp_context_t *ctxt, gnrc_pktsnip_t *buf)
{
    size_t len = 0;

    /* fill the packet */
    tftp_packet_data_t *pkt = (tftp_packet_data_t*)(buf->data);
    pkt->opc = TO_DATA;
    pkt->block_nr = HTONS(ctxt->block_nr);

    /* get the required data from the user */
    len = ctxt->cb(ctxt->block_size * ctxt->block_nr, pkt->data, ctxt->block_size);

    /* send the data */
    return _tftp_send(buf, ctxt, sizeof(tftp_packet_data_t) + len);
}

int _tftp_send_error(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_err_codes_t err, const char *err_msg)
{
    int strl = err_msg
            ? strlen(err_msg)
            : 0;

    (void) ctxt;

    /* fill the packet */
    tftp_packet_error_t *pkt = (tftp_packet_error_t*)(buf->data);
    pkt->opc = err;
    pkt->err_code = err;
    memcpy(pkt->err_msg, err_msg, strl);
    pkt->err_msg[strl] = 0;

    /* return the size of the packet */
    return sizeof(tftp_packet_error_t) + strl + 1;
}

int _tftp_decode_options(tftp_context_t *ctxt, gnrc_pktsnip_t *buf) {
    tftp_header_t *pkt = (tftp_header_t*)buf->data;
    size_t offset = 0;

    while (offset < (buf->size - sizeof(tftp_header_t))) {
        /* get the option name */
        const char *name = (const char*)(pkt->data + offset);
        offset += strlen(name) + 1;
        /* get the value name */
        const char *value = (const char*)(pkt->data + offset);
        offset += strlen(value) + 1;

        /* check what option we are parsing */
        for (uint32_t idx = 0; idx < ARRAY_LEN(_tftp_options); ++idx) {
            if (memcmp(name, _tftp_options[idx].name, _tftp_options[idx].len) == 0) {
                /* set the option value of the known options */
                if (idx == TOPT_BLKSIZE) {
                    ctxt->block_size = atoi(value);
                }
                else if (idx == TOPT_TSIZE) {
                    ctxt->transfer_size = atoi(value);
                }
                else if (idx == TOPT_TIMEOUT) {
                    ctxt->timeout = atoi(value);
                }

                break;
            }
        }
    }

    return sizeof(tftp_header_t) + buf->size;
}

int _tftp_decode_start(uint8_t *buf, char **file_name, tftp_mode_t *mode) {
    /* decode the packet */
    tftp_header_t *hdr = (tftp_header_t*)buf;
    *file_name = (char*)(hdr->data);

    /* decode the TFTP transfer mode */
    char *str_mode = ((char*) memchr(hdr->data, 0, TFTP_DEFAULT_DATA_SIZE)); /* TODO get from context */
    if (!str_mode)
        return -EINVAL;

    for (uint32_t idx = 0; idx < ARRAY_LEN(_tftp_modes); ++idx) {
        if (memcmp(_tftp_modes[idx].name, str_mode, _tftp_modes[idx].len) == 0) {
            *mode = (tftp_mode_t)idx;
            return ((str_mode + _tftp_modes[idx].len) - (char*)buf);
        }
    }

    return -EINVAL;
}

bool _tftp_validate_ack(tftp_context_t *ctxt, uint8_t *buf) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) buf;

    return (ctxt->block_nr == NTOHS(pkt->block_nr));
}

int _tftp_process_data(tftp_context_t *ctxt, gnrc_pktsnip_t *buf) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) buf->data;

    uint16_t block_nr = NTOHS(pkt->block_nr);

    /* check if this is the packet we are waiting for */
    if (block_nr != (ctxt->block_nr + 1)) {
        return -1;
    }

    /* send the user data trough to the user application */
    if (ctxt->cb(ctxt->block_nr * ctxt->block_size, pkt->data, buf->size - sizeof(tftp_packet_data_t)) < 0) {
        return -1;
    }

    /* return the number of data bytes received */
    return (buf->size - sizeof(tftp_packet_data_t));
}

int _tftp_decode_error(uint8_t *buf, tftp_err_codes_t *err, const char **err_msg) {
    tftp_packet_error_t *pkt = (tftp_packet_error_t*) buf;

    *err = pkt->err_code;
    *err_msg = pkt->err_msg;

    return (sizeof(tftp_packet_error_t) + strlen(pkt->err_msg) + 1);
}

/**
 * @}
 */

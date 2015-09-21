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

#define TFTP_TIMEOUT_MSG            0x4000
#define TFTP_DEFAULT_DATA_SIZE      (GNRC_TFTP_MAX_TRANSFER_UNIT    \
                                        + sizeof(tftp_packet_data_t))
#define MSG(s)                      #s, sizeof(#s)

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
        MODE(timeout),
        MODE(tsize),
};

#define INT_DIGITS 6       /* enough for 64 bit integer */

char *itoa(int i, size_t *len)
{
  /* Room for INT_DIGITS digits, - and '\0' */
  static char buf[INT_DIGITS + 2];
  char *p = buf + INT_DIGITS + 1;   /* points to terminating '\0' */
  if (i >= 0) {
    do {
      *--p = '0' + (i % 10);
      i /= 10;
    } while (i != 0);
    *len = (INT_DIGITS + 1) - (p - buf);
    return p;
  }

  return NULL;
}

int _tftp_init_ctxt(ipv6_addr_t *addr, const char *file_name,
                    tftp_data_callback cb, tftp_context_t *ctxt,
                    tftp_opcodes_t op) {

    if (!addr || !file_name || !cb) {
        return FAILED;
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

    return FINISHED;
}

int _tftp_set_opts(tftp_context_t *ctxt, size_t blksize, uint16_t timeout, size_t total_size) {
    if (blksize > GNRC_TFTP_MAX_TRANSFER_UNIT || !timeout) {
        return FAILED;
    }

    ctxt->block_size = blksize;
    ctxt->timeout = timeout;
    ctxt->transfer_size = total_size;
    ctxt->use_options = true;

    return FINISHED;
}

int _tftp_start_server_transfer(tftp_context_t *ctxt) {
    msg_t msg;
    int ret = 0;

    /* register our TFTP response listener */
    gnrc_netreg_entry_t entry = { NULL, GNRC_TFTP_DEFAULT_DST_PORT,
                                  thread_getpid() };
    if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
        DEBUG("tftp: error starting server.");
        return FAILED;
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

int _tftp_do_client_transfer(tftp_context_t *ctxt) {
    msg_t msg;
    tftp_state ret = BUSY;

    /* register our DNS response listener */
    gnrc_netreg_entry_t entry = { NULL, ctxt->src_port, thread_getpid() };
    if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
        DEBUG("tftp: error starting server.");
        return FAILED;
    }

    /* try to start the TFTP transfer */
    ret = _tftp_state_processes(ctxt, NULL);
    if (ret != BUSY) {
        /* if the start failed return */
        return ret;
    }

    /* main processing loop */
    while (ret == BUSY) {
        /* wait for a message */
        msg_receive(&msg);
        ret = _tftp_state_processes(ctxt, &msg);
    }

    /* unregister our UDP listener on this thread */
    gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &entry);

    return ret;
}

tftp_state _tftp_send(gnrc_pktsnip_t *buf, tftp_context_t *ctxt, size_t len) {
    network_uint16_t src_port, dst_port;
    gnrc_pktsnip_t *udp, *ip;

    if (len > TFTP_DEFAULT_DATA_SIZE) {
        DEBUG("tftp: can't reallocate to bigger packet, buffer overflowed");
        return FAILED;
    }
    else if (gnrc_pktbuf_realloc_data(buf, len) != 0) {
        DEBUG("tftp: failed to reallocate data snippet");
        return FAILED;
    }

    /* allocate UDP header, set source port := destination port */
    src_port.u16 = ctxt->src_port;
    dst_port.u16 = ctxt->dst_port;
    udp = gnrc_udp_hdr_build(buf, src_port.u8, sizeof(src_port),
            dst_port.u8, sizeof(dst_port));
    if (udp == NULL) {
        DEBUG("dns: error unable to allocate UDP header");
        gnrc_pktbuf_release(buf);
        return FAILED;
    }

    /* allocate IPv6 header */
    ip = gnrc_ipv6_hdr_build(udp, NULL, 0, ctxt->peer->u8, sizeof(ipv6_addr_t));
    if (ip == NULL) {
        DEBUG("dns: error unable to allocate IPv6 header");
        gnrc_pktbuf_release(udp);
        return FAILED;
    }

    /* send packet */
    if (gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL,
                                  ip) == 0) {
        /* if send failed inform the user */
        DEBUG("dns: error unable to locate UDP thread");
        gnrc_pktbuf_release(ip);
        return FAILED;
    }

#if 0
    vtimer_set_msg(&(ctxt->timer), timex_set(ctxt->timeout, 0), thread_getpid(),
                    TFTP_TIMEOUT_MSG, NULL);
#endif

    return BUSY;
}

void _tftp_set_default_options(tftp_context_t *ctxt) {
    ctxt->block_size = GNRC_TFTP_MAX_TRANSFER_UNIT;
    ctxt->timeout = 1;
    ctxt->transfer_size = 0;
}

tftp_state _tftp_state_processes(tftp_context_t *ctxt, msg_t *m) {
    gnrc_pktsnip_t *outbuf = gnrc_pktbuf_add(NULL, NULL, TFTP_DEFAULT_DATA_SIZE,
                                             GNRC_NETTYPE_UNDEF);

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
            /* if we are reading resent the ACK, if writing the DATA */
            return _tftp_send_dack(ctxt, outbuf, (ctxt->op == TO_RRQ) ? TO_ACK : TO_DATA);
        }
    }
    else if (m->type != GNRC_NETAPI_MSG_TYPE_RCV) {
        DEBUG("tftp: unknown message");
        return BUSY;
    }

    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t*)(m->content.ptr);
    uint8_t *data = (uint8_t*)pkt->data;
    udp_hdr_t *hdr = (udp_hdr_t*)pkt->next->data;

    switch (_tftp_parse_type(data)) {
    case TO_RRQ:
    case TO_RWQ: {
        if (byteorder_ntohs(hdr->dst_port) != GNRC_TFTP_DEFAULT_DST_PORT) {
            /* not a valid start packet */
            return FAILED;
        }

        /* generate a random source UDP source port */
        do {
            ctxt->src_port = (genrand_uint32() & 0xff) +
                                 GNRC_TFTP_DEFAULT_SRC_PORT;
        } while (gnrc_netreg_num(GNRC_NETTYPE_UDP, ctxt->src_port));

        /* get the context of the client */
        ctxt->dst_port = byteorder_ntohs(hdr->src_port);
        ctxt->op = _tftp_parse_type(data);
        int offset = _tftp_decode_start(data, &(ctxt->file_name), &(ctxt->mode));
        if (offset < 0) {
            return FAILED;
        }

        /* TODO validate if the application accepts the filename and modes */
        /* if (ctxt->start_callback(ctxt->file_name, ctxt->mode) != 0) {
         *     tftp_state _tftp_send_error(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_err_codes_t err, const char *err_msg);
         *     return FAILED;
         * }
         */

        /* decode the options */
        if (_tftp_decode_options(ctxt, pkt, offset) > offset) {
            /* the client send the TFTP options, we must OACK */
            _tftp_send_dack(ctxt, outbuf, TO_OACK);
        }
        else {
            /* the client didn't send options, use ACK and set defaults */
            _tftp_set_default_options(ctxt);
            _tftp_send_dack(ctxt, outbuf, TO_ACK);
        }
    } break;

    case TO_DATA: {
        /* try to process the data */
        int proc = _tftp_process_data(ctxt, pkt);
        if (proc < 0) {
            /* the data is not accepted return */
            return BUSY;
        }

        /* check if this is the first block */
        if (!ctxt->block_nr) {
            /* no OACK received, restore default TFTP parameters */
            _tftp_set_default_options(ctxt);

            /* switch the destination port to the src port of the server */
            ctxt->dst_port = byteorder_ntohs(hdr->src_port);
        }

        /* wait for the next data block */
        ++(ctxt->block_nr);
        _tftp_send_dack(ctxt, outbuf, TO_ACK);

        /* check if the data transfer has finished */
        if (proc < ctxt->block_size) {
            return FINISHED;
        }

        return BUSY;
    } break;

    case TO_ACK: {
        /* validate if this is the ACK we are waiting for */
        if (!_tftp_validate_ack(ctxt, data)) {
            /* invalid packet ACK, drop */
            return BUSY;
        }

        /* check if this is the first ACK */
        if (!ctxt->block_nr) {
            /* no OACK received restore default TFTP parameters */
            _tftp_set_default_options(ctxt);

            /* switch the destination port to the src port of the server */
            ctxt->dst_port = byteorder_ntohs(hdr->src_port);
        }

        /* send the next data block */
        ++(ctxt->block_nr);
        return _tftp_send_dack(ctxt, outbuf, TO_DATA);
    } break;

    case TO_ERROR: {
        tftp_err_codes_t err;
        const char *err_msg;

        _tftp_decode_error(data, &err, &err_msg);

        /* TODO inform the user what the error was ? */

        return FAILED;
    } break;

    case TO_OACK: {
        /* decode the options */
        _tftp_decode_options(ctxt, pkt, 0);

        /* take the new source port */
        ctxt->dst_port = byteorder_ntohs(hdr->src_port);

        /* send and ACK that we accept the options */
        _tftp_send_dack(ctxt, outbuf, TO_ACK);

        return BUSY;
    } break;
    }

    return FAILED;
}

size_t _tftp_add_option(uint8_t *dst, tftp_opt_t *opt, uint32_t value) {
    size_t offset;
    size_t len;
    char *val;

    /* set the option name */
    memcpy(dst, opt->name, opt->len);
    offset = opt->len;

    /* set the option value */
    val = itoa(value, &len);
    memcpy(dst + opt->len, val, len);
    offset += len;

    /* finish option value */
    *(dst + offset) = 0;
    return ++offset;
}

tftp_state _tftp_send_start(tftp_context_t *ctxt, gnrc_pktsnip_t *buf)
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
    if (ctxt->use_options) {
        offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_BLKSIZE, ctxt->block_size);
        offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_TIMEOUT, ctxt->timeout);
        offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_TSIZE, ctxt->transfer_size);
    }

    /* send the data */
    return _tftp_send(buf, ctxt, offset + sizeof(tftp_header_t));
}

tftp_state _tftp_send_dack(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_opcodes_t op)
{
    size_t len = 0;

    assert(op == TO_DATA || op == TO_ACK || op == TO_OACK);

    /* fill the packet */
    tftp_packet_data_t *pkt = (tftp_packet_data_t*)(buf->data);
    pkt->block_nr = HTONS(ctxt->block_nr);
    pkt->opc = op;

    if (op == TO_DATA) {
        /* get the required data from the user */
        len = ctxt->cb(ctxt->block_size * ctxt->block_nr, pkt->data, ctxt->block_size);
    }

    /* send the data */
    return _tftp_send(buf, ctxt, sizeof(tftp_packet_data_t) + len);
}

tftp_state _tftp_send_error(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_err_codes_t err, const char *err_msg)
{
    int strl = err_msg
            ? strlen(err_msg) + 1
            : 0;

    (void) ctxt;

    /* fill the packet */
    tftp_packet_error_t *pkt = (tftp_packet_error_t*)(buf->data);
    pkt->opc = err;
    pkt->err_code = err;
    memcpy(pkt->err_msg, err_msg, strl);

    /* return the size of the packet */
    return sizeof(tftp_packet_error_t) + strl;
}

int _tftp_decode_options(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, uint32_t start) {
    tftp_header_t *pkt = (tftp_header_t*)buf->data;
    size_t offset = start;

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

    return sizeof(tftp_header_t) + offset;
}

int _tftp_decode_start(uint8_t *buf, const char **file_name, tftp_mode_t *mode) {
    /* decode the packet */
    tftp_header_t *hdr = (tftp_header_t*)buf;
    *file_name = (char*)(hdr->data);

    /* decode the TFTP transfer mode */
    char *str_mode = ((char*) memchr(hdr->data, 0, TFTP_DEFAULT_DATA_SIZE));
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
        return BUSY;
    }

    /* send the user data trough to the user application */
    if (ctxt->cb(ctxt->block_nr * ctxt->block_size, pkt->data, buf->size - sizeof(tftp_packet_data_t)) < 0) {
        return BUSY;
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

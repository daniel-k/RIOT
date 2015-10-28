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
#include <stdio.h>
#include <time.h>

#include "net/gnrc/tftp.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/ipv6.h"
#include "random.h"

#define ENABLE_DEBUG                (1)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu16 etc. */
#include <inttypes.h>
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define CT_HTONS(x)                 ((          \
                           (x >> 8) & 0x00FF) | \
                           ((x << 8) & 0xFF00))

#define CT_HTONL(x)                 ((          \
                      (x >> 24) & 0x000000FF) | \
                      ((x >> 8) & 0x0000FF00) | \
                      ((x << 8) & 0x00FF0000) | \
                      ((x << 24) & 0xFF000000))
#else
#define CT_HTONS(x)                 (x)
#define CT_HTONL(x)                 (x)
#endif

#define MIN(a,b)                    ((a) > (b) ? (b) : (a))
#define ARRAY_LEN(x)                (sizeof(x) / sizeof(x[0]))

#define TFTP_TIMEOUT_MSG            0x4000
#define TFTP_DEFAULT_DATA_SIZE      (GNRC_TFTP_MAX_TRANSFER_UNIT    \
                                        + sizeof(tftp_packet_data_t))

/* uint32_t max is 4,294,967,296 */
char str_buffer[11];

/**
 * @brief TFTP mode help support
 */
#define MODE(mode)                  { #mode, sizeof(#mode) }

typedef struct {
    char *name;
    uint8_t len;
} tftp_opt_t;

/**
 * @brief TFTP opcodes
 */
typedef enum {
    TO_RRQ          = CT_HTONS(1),      /**< Read Request */
    TO_RWQ          = CT_HTONS(2),      /**< Write Request */
    TO_DATA         = CT_HTONS(3),      /**< Data */
    TO_ACK          = CT_HTONS(4),      /**< Acknowledgment */
    TO_ERROR        = CT_HTONS(5),      /**< Error */
    TO_OACK         = CT_HTONS(6),      /**< Option ACK */
} tftp_opcodes_t;

/**
 * @brief TFTP Error Codes
 */
typedef enum {
    TE_UN_DEF       = CT_HTONS(0),      /**< Not defined, see error message */
    TE_NO_FILE      = CT_HTONS(1),      /**< File not found */
    TE_ACCESS       = CT_HTONS(2),      /**< Access violation */
    TE_DFULL        = CT_HTONS(3),      /**< Disk full or allocation exceeded */
    TE_ILLOPT       = CT_HTONS(4),      /**< Illegal TFTP operation */
    TE_UNKOWN_ID    = CT_HTONS(5),      /**< Unknown transfer ID */
    TE_EXISTS       = CT_HTONS(6),      /**< File already exists */
    TE_UNKOWN_USR   = CT_HTONS(7),      /**< No such user */
} tftp_err_codes_t;

/**
 * @brief TFTP Transfer modes
 */
typedef enum {
    ASCII,
    OCTET,
    MAIL
} tftp_mode_t;

/* ordered as @see tftp_mode_t */
tftp_opt_t _tftp_modes[] = {
    MODE(netascii),
    MODE(octet),
    MODE(mail),
};

/**
 * @brief TFTP Options
 */
typedef enum {
    TOPT_BLKSIZE,
    TOPT_TIMEOUT,
    TOPT_TSIZE,
} tftp_options_t;

/* ordered as @see tftp_options_t */
tftp_opt_t _tftp_options[] = {
    MODE(blksize),
    MODE(timeout),
    MODE(tsize),
};

/**
 * @brief The TFTP state
 */
typedef enum {
    FAILED      = -1,
    BUSY        = 0,
    FINISHED    = 1
} tftp_state;

/**
 * @brief The TFTP context for the current transfer
 */
typedef struct {
    char file_name[GNRC_TFTP_MAX_FILENAME_LEN];
    tftp_mode_t mode;
    tftp_opcodes_t op;
    ipv6_addr_t *peer;
    xtimer_t timer;
    msg_t timer_msg;
    uint32_t timeout;
    uint16_t dst_port;
    uint16_t src_port;
    tftp_transfer_start_callback start_cb;
    tftp_data_callback data_cb;
    gnrc_netreg_entry_t entry;

    /* transfer parameters */
    uint16_t block_nr;
    uint16_t block_size;
    uint32_t transfer_size;
    uint32_t block_timeout;
    uint32_t retries;
    bool use_options;
} tftp_context_t;

/**
 * @brief The default TFTP header
 */
typedef struct __attribute__((packed)) {
    tftp_opcodes_t opc          : 16;
    uint8_t data[];
} tftp_header_t;

/**
 * @brief The TFTP data packet
 */
typedef struct __attribute__((packed)) {
    tftp_opcodes_t opc          : 16;
    network_uint16_t block_nr;
    uint8_t data[];
} tftp_packet_data_t;

/**
 * @brief The TFTP error packet
 */
typedef struct __attribute__((packed)) {
    tftp_opcodes_t opc          : 16;
    tftp_err_codes_t err_code   : 16;
    char err_msg[];
} tftp_packet_error_t;

/* get the TFTP opcode */
static inline tftp_opcodes_t _tftp_parse_type(uint8_t *buf) {
    return ((tftp_header_t*)buf)->opc;
}

/* initialize the context to it's default state */
static int _tftp_init_ctxt(ipv6_addr_t *addr, const char *file_name,
                           tftp_transfer_start_callback start_cb,
                           tftp_data_callback data_cb, tftp_opcodes_t op,
                           tftp_context_t *ctxt);

/* set the TFTP options to use */
static int _tftp_set_opts(tftp_context_t *ctxt, size_t blksize, uint16_t timeout, size_t total_size);

/* this function registers the UDP port and won't return till the TFTP transfer is finished */
static int _tftp_do_client_transfer(tftp_context_t *ctxt);

/* the state process of the TFTP transfer */
static tftp_state _tftp_state_processes(tftp_context_t *ctxt, msg_t *m);

/* send an start request if we run in client mode */
static tftp_state _tftp_send_start(tftp_context_t *ctxt, gnrc_pktsnip_t *buf);

/* send data or and ack depending if we are reading or writing */
static tftp_state _tftp_send_dack(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_opcodes_t op);

/* send and TFTP error to the client */
static tftp_state _tftp_send_error(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, tftp_err_codes_t err, const char *err_msg);

/* this function sends the actual packet */
static tftp_state _tftp_send(gnrc_pktsnip_t *buf, tftp_context_t *ctxt, size_t len);

/* decode the default TFTP start packet */
static int _tftp_decode_start(tftp_context_t *ctxt, uint8_t *buf, gnrc_pktsnip_t *outbuf);

/* decode the TFTP option extensions */
static  int _tftp_decode_options(tftp_context_t *ctxt, gnrc_pktsnip_t *buf, uint32_t start);

/* decode the received ACK packet */
static bool _tftp_validate_ack(tftp_context_t *ctxt, uint8_t *buf);

/* processes the received data packet and calls the callback defined by the user */
static int _tftp_process_data(tftp_context_t *ctxt, gnrc_pktsnip_t *buf);

/* decode the received error packet and calls the callback defined by the user */
static int _tftp_decode_error(uint8_t *buf, tftp_err_codes_t *err, const char **err_msg);

static int _tftp_server(tftp_context_t *ctxt);

/* get the maximum allowed transfer unit to avoid 6Lo fragmentation */
static uint16_t _tftp_get_maximum_block_size(void) {
    uint16_t tmp;
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t ifnum = gnrc_netif_get(ifs);

    return 10;

    if (ifnum > 0 && gnrc_netapi_get(ifs[0], NETOPT_MAX_PACKET_SIZE, 0, &tmp, sizeof(uint16_t)) >= 0) {
        /* TODO calculate proper block size */
        return tmp - sizeof(udp_hdr_t) - sizeof(ipv6_hdr_t) - 50;
    }

    return GNRC_TFTP_MAX_TRANSFER_UNIT;
}

int gnrc_tftp_client_read(ipv6_addr_t *addr, const char *file_name,
                          tftp_data_callback data_cb, tftp_transfer_start_callback start_cb) {
    tftp_context_t ctxt;

    /* prepare the context */
    if (_tftp_init_ctxt(addr, file_name, start_cb, data_cb, TO_RRQ, &ctxt) != FINISHED) {
        return -EINVAL;
    }

    /* set the transfer options */
    uint16_t mtu = _tftp_get_maximum_block_size();
    if (_tftp_set_opts(&ctxt, mtu, 1, 0) != FINISHED) {
        return -EINVAL;
    }

    /* start the process */
    int ret = _tftp_do_client_transfer(&ctxt);

    /* remove possibly stale timer */
    xtimer_remove(&(ctxt.timer));

    return ret;
}

int gnrc_tftp_client_write(ipv6_addr_t *addr, const char *file_name,
                           tftp_data_callback data_cb, uint32_t total_size) {
    tftp_context_t ctxt;

    /* prepare the context */
    if (_tftp_init_ctxt(addr, file_name, NULL, data_cb, TO_RWQ, &ctxt) < 0) {
        return -EINVAL;
    }

    /* set the transfer options */
    uint16_t mtu = _tftp_get_maximum_block_size();
    if (_tftp_set_opts(&ctxt, mtu, 1, total_size) != FINISHED) {
        return -EINVAL;
    }

    /* start the process */
    int ret = _tftp_do_client_transfer(&ctxt);

    /* remove possibly stale timer */
    xtimer_remove(&(ctxt.timer));

    return ret;
}

int _tftp_init_ctxt(ipv6_addr_t *addr, const char *file_name,
                    tftp_transfer_start_callback start_cb,
                    tftp_data_callback data_cb, tftp_opcodes_t op,
                    tftp_context_t *ctxt) {

    if (!addr || !data_cb) {
        return FAILED;
    }

    memset(ctxt, 0, sizeof(tftp_context_t));

    /* set the default context parameters */
    ctxt->op = op;
    ctxt->peer = addr;
    ctxt->mode = OCTET;
    ctxt->data_cb = data_cb;
    ctxt->start_cb = start_cb;
    strncpy(ctxt->file_name, file_name, GNRC_TFTP_MAX_FILENAME_LEN);
    ctxt->file_name[GNRC_TFTP_MAX_FILENAME_LEN - 1] = 0;
    ctxt->dst_port = GNRC_TFTP_DEFAULT_DST_PORT;

    /* transport layer parameters */
    ctxt->block_size = GNRC_TFTP_MAX_TRANSFER_UNIT;
    ctxt->block_timeout = 1;

    /* generate a random source UDP source port */
    do {
        ctxt->src_port = (genrand_uint32() & 0xff) + GNRC_TFTP_DEFAULT_SRC_PORT;
    } while (gnrc_netreg_num(GNRC_NETTYPE_UDP, ctxt->src_port));

    return FINISHED;
}

void _tftp_set_default_options(tftp_context_t *ctxt) {
    ctxt->block_size = GNRC_TFTP_MAX_TRANSFER_UNIT;
    ctxt->timeout = 1;
    ctxt->transfer_size = 0;
    ctxt->use_options = false;
}

int _tftp_set_opts(tftp_context_t *ctxt, size_t blksize, uint16_t timeout, size_t total_size) {
    if (blksize > GNRC_TFTP_MAX_TRANSFER_UNIT || !timeout) {
        return FAILED;
    }

    ctxt->block_size = blksize;
    ctxt->timeout = timeout;
    ctxt->block_timeout = timeout;
    ctxt->transfer_size = total_size;
    ctxt->use_options = true;

    return FINISHED;
}

int gnrc_tftp_server(tftp_data_callback data_cb, tftp_transfer_start_callback start_cb)
{
    /* Context will be initialized when a connection is established */
    tftp_context_t ctxt;

    assert(data_cb);
    assert(start_cb);

    ctxt.data_cb = data_cb;
    ctxt.start_cb = start_cb;

    /* start the server */
    int ret = _tftp_server(&ctxt);

    /* remove possibly stale timer */
    xtimer_remove(&(ctxt.timer));

    return ret;
}

int _tftp_server(tftp_context_t *ctxt) {
    msg_t msg;
    gnrc_netreg_entry_t entry = { NULL, GNRC_TFTP_DEFAULT_DST_PORT,
                              thread_getpid() };

    while (1) {
        int ret = BUSY;
        bool got_client = false;

        /* register our TFTP response listener */
        if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
            DEBUG("tftp: error starting server.");
            return FAILED;
        }

        /* main processing loop */
        while (ret == BUSY) {
            /* wait for a message */
            msg_receive(&msg);
            ret = _tftp_state_processes(ctxt, &msg);

            /* release packet if we received one */
            if(msg.type == GNRC_NETAPI_MSG_TYPE_RCV) {
                gnrc_pktbuf_release((gnrc_pktsnip_t*) msg.content.ptr);
            }

            if (ret == BUSY && !got_client) {
                gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &entry);
                got_client = true;
            }
        }
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

        /* release packet if we received one */
        if(msg.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            gnrc_pktbuf_release((gnrc_pktsnip_t*) msg.content.ptr);
        }
    }

    /* unregister our UDP listener on this thread */
    gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &entry);

    return ret;
}

tftp_state _tftp_state_processes(tftp_context_t *ctxt, msg_t *m) {
    gnrc_pktsnip_t *outbuf = gnrc_pktbuf_add(NULL, NULL, TFTP_DEFAULT_DATA_SIZE,
                                             GNRC_NETTYPE_UNDEF);

    /* check if this is an client start */
    if (!m) {
        return _tftp_send_start(ctxt, outbuf);
    }
    else if (m->type == TFTP_TIMEOUT_MSG) {
        if (++(ctxt->retries) > GNRC_TFTP_MAX_RETRIES) {
            /* transfer failed due to lost peer */
            gnrc_pktbuf_release(outbuf);
            return FAILED;
        }

        /* increase the timeout for congestion control */
        ctxt->block_timeout <<= 1;

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
        gnrc_pktbuf_release(outbuf);
        return BUSY;
    }

    gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t*)(m->content.ptr);

    assert(pkt->next && pkt->next->type == GNRC_NETTYPE_UDP);
    assert(pkt->next->next && pkt->next->next->type == GNRC_NETTYPE_IPV6);

    uint8_t *data = (uint8_t*)pkt->data;
    udp_hdr_t *udp = (udp_hdr_t*)pkt->next->data;
    ipv6_hdr_t *ip = (ipv6_hdr_t*)pkt->next->next->data;

    xtimer_remove(&(ctxt->timer));

    switch (_tftp_parse_type(data)) {
    case TO_RRQ:
    case TO_RWQ: {
        if (byteorder_ntohs(udp->dst_port) != GNRC_TFTP_DEFAULT_DST_PORT) {
            /* not a valid start packet */
            gnrc_pktbuf_release(outbuf);

            return FAILED;
        }

        /* reinitialize the context with the current client */
        _tftp_init_ctxt(&(ip->src), NULL, ctxt->start_cb, ctxt->data_cb,
                        _tftp_parse_type(data), ctxt);

        /* get the context of the client */
        ctxt->dst_port = byteorder_ntohs(udp->src_port);
        int offset = _tftp_decode_start(ctxt, data, outbuf);
        if (offset < 0) {
            gnrc_pktbuf_release(outbuf);
            return FAILED;
        }

        /* validate if the application accepts the filename and modes */
        tftp_action_t action = ctxt->op == TO_RRQ ? TFTP_READ : TFTP_WRITE;
        if (ctxt->start_cb(action, ctxt->file_name, ctxt->mode) != 0) {
            _tftp_send_error(ctxt, outbuf, TE_ACCESS, "Blocked by user application");
            return FAILED;
        }

        /* register a listener for the UDP port */
        ctxt->entry.next = NULL;
        ctxt->entry.demux_ctx = ctxt->src_port;
        ctxt->entry.pid = thread_getpid();
        gnrc_netreg_register(GNRC_NETTYPE_UDP, &(ctxt->entry));

        /* decode the options */
        tftp_state state;
        if (_tftp_decode_options(ctxt, pkt, offset) > offset) {
            /* the client send the TFTP options, we must OACK */
            state = _tftp_send_dack(ctxt, outbuf, TO_OACK);
        }
        else {
            /* the client didn't send options, use ACK and set defaults */
            _tftp_set_default_options(ctxt);
            state = _tftp_send_dack(ctxt, outbuf, TO_ACK);
        }

        // check if the client negotiation was successful
        if (state != BUSY) {
            gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &(ctxt->entry));
        }
        return state;
    } break;

    case TO_DATA: {
        /* try to process the data */
        int proc = _tftp_process_data(ctxt, pkt);
        if (proc <= 0) {
            /* the data is not accepted return */
            gnrc_pktbuf_release(outbuf);
            return BUSY;
        }

        /* check if this is the first block */
        if (!ctxt->block_nr && ctxt->dst_port == GNRC_TFTP_DEFAULT_DST_PORT) {
            /* no OACK received, restore default TFTP parameters */
            _tftp_set_default_options(ctxt);

            /* switch the destination port to the src port of the server */
            ctxt->dst_port = byteorder_ntohs(udp->src_port);
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
            gnrc_pktbuf_release(outbuf);
            return BUSY;
        }

        /* check if this is the first ACK */
        if (!ctxt->block_nr) {
            /* no OACK received restore default TFTP parameters */
            _tftp_set_default_options(ctxt);

            /* switch the destination port to the src port of the server */
            ctxt->dst_port = byteorder_ntohs(udp->src_port);
        }

        /* send the next data block */
        ++(ctxt->block_nr);

        return _tftp_send_dack(ctxt, outbuf, TO_DATA);
    } break;

    case TO_ERROR: {
        tftp_err_codes_t err;
        const char *err_msg;

        _tftp_decode_error(data, &err, &err_msg);

        /* inform the user what the error was ? */
        gnrc_pktbuf_release(outbuf);
        return FAILED;
    } break;

    case TO_OACK: {
        /* decode the options */
        _tftp_decode_options(ctxt, pkt, 0);

        /* take the new source port */
        ctxt->dst_port = byteorder_ntohs(udp->src_port);

        /* send and ACK that we accept the options */
        return _tftp_send_dack(ctxt, outbuf, TO_ACK);
    } break;
    }

    gnrc_pktbuf_release(outbuf);
    return FAILED;
}

size_t _tftp_add_option(uint8_t *dst, tftp_opt_t *opt, uint32_t value) {
    size_t offset;
    size_t len;

    /* set the option name */
    memcpy(dst, opt->name, opt->len);
    offset = opt->len;

    /* set the option value */
    snprintf(str_buffer, sizeof(str_buffer), "%"PRIu32, value);
    len = strlen(str_buffer);
    memcpy(dst + opt->len, str_buffer, len);
    offset += len;

    /* finish option value */
    *(dst + offset) = 0;
    return ++offset;
}

uint32_t _tftp_append_options(tftp_context_t *ctxt, tftp_header_t *hdr, uint32_t offset) {
    offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_BLKSIZE, ctxt->block_size);
    offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_TIMEOUT, ctxt->timeout);
    offset += _tftp_add_option(hdr->data + offset, _tftp_options + TOPT_TSIZE, ctxt->transfer_size);
    return offset;
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
        offset = _tftp_append_options(ctxt, hdr, offset);
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
    pkt->block_nr = byteorder_htons(ctxt->block_nr);
    pkt->opc = op;

    if (op == TO_DATA) {
        /* get the required data from the user */
        len = ctxt->data_cb(ctxt->block_size * ctxt->block_nr, pkt->data, ctxt->block_size);
    }
    else if (op == TO_OACK) {
        /* append the options */
        len = _tftp_append_options(ctxt, (tftp_header_t*)pkt, 0);
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
    _tftp_send(buf, ctxt, sizeof(tftp_packet_error_t) + strl);

    /* special case, stop the retry mechanism */
    xtimer_remove(&(ctxt->timer));

    return FAILED;
}

tftp_state _tftp_send(gnrc_pktsnip_t *buf, tftp_context_t *ctxt, size_t len) {
    network_uint16_t src_port, dst_port;
    gnrc_pktsnip_t *udp, *ip;

    if (len > TFTP_DEFAULT_DATA_SIZE) {
        DEBUG("tftp: can't reallocate to bigger packet, buffer overflowed");
        gnrc_pktbuf_release(buf);
        return FAILED;
    }
    else if (gnrc_pktbuf_realloc_data(buf, len) != 0) {
        DEBUG("tftp: failed to reallocate data snippet");
        gnrc_pktbuf_release(buf);
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

    ctxt->timer_msg.type = TFTP_TIMEOUT_MSG;
    xtimer_set_msg(&(ctxt->timer), ctxt->block_timeout * MS_IN_USEC, &(ctxt->timer_msg), thread_getpid());

    return BUSY;
}

bool _tftp_validate_ack(tftp_context_t *ctxt, uint8_t *buf) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) buf;

    return (ctxt->block_nr == byteorder_ntohs(pkt->block_nr));
}

int _tftp_decode_start(tftp_context_t *ctxt, uint8_t *buf, gnrc_pktsnip_t *outbuf) {
    /* decode the packet */
    tftp_header_t *hdr = (tftp_header_t*)buf;

    /* find the first 0 char */
    char *str_mode = ((char*) memchr(hdr->data, 0, TFTP_DEFAULT_DATA_SIZE));

    /* get the file name */
    int fnlen = ((char*)hdr->data) - str_mode;
    if (fnlen >= GNRC_TFTP_MAX_FILENAME_LEN) {
        _tftp_send_error(ctxt, outbuf, TE_ILLOPT, "Filename to long");
        return FAILED;
    }
    memcpy(ctxt->file_name, hdr->data, fnlen);

    /* decode the TFTP transfer mode */
    if (!str_mode)
        return -EINVAL;

    for (uint32_t idx = 0; idx < ARRAY_LEN(_tftp_modes); ++idx) {
        if (memcmp(_tftp_modes[idx].name, str_mode, _tftp_modes[idx].len) == 0) {
            ctxt->mode = (tftp_mode_t)idx;
            return ((str_mode + _tftp_modes[idx].len) - (char*)buf);
        }
    }

    return -EINVAL;
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
                switch (idx)
                {
                case TOPT_BLKSIZE:
                    ctxt->block_size = atoi(value);
                    break;

                case TOPT_TSIZE:
                    ctxt->transfer_size = atoi(value);

                    if (ctxt->start_cb) {
                        ctxt->start_cb(TFTP_READ, ctxt->file_name, ctxt->transfer_size);
                    }
                    break;

                case TOPT_TIMEOUT:
                    ctxt->timeout = atoi(value);
                    break;
                }

                break;
            }
        }
    }

    return sizeof(tftp_header_t) + offset;
}

int _tftp_process_data(tftp_context_t *ctxt, gnrc_pktsnip_t *buf) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) buf->data;

    uint16_t block_nr = byteorder_ntohs(pkt->block_nr);

    /* check if this is the packet we are waiting for */
    if (block_nr != (ctxt->block_nr + 1)) {
        return BUSY;
    }

    /* send the user data trough to the user application */
    if (ctxt->data_cb(ctxt->block_nr * ctxt->block_size, pkt->data, buf->size - sizeof(tftp_packet_data_t)) < 0) {
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

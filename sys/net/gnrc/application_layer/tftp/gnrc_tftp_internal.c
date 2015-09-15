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

/**
 * @brief TFTP mode help support
 */
#define MODE(mode)                  { #mode, sizeof(#mode) }

typedef struct {
    char *name;
    uint8_t len;
} tftp_option_t;

/* ordered as @see tftp_mode_t */
tftp_option_t _tftp_modes[] = {
        MODE(netascii),
        MODE(octet),
        MODE(mail),
};

/* ordered as @see tftp_options_t */
tftp_option_t _tftp_options[] = {
        MODE(blksize),
        MODE(tsize),
        MODE(timeout),
};

int _tftp_state_processes(tftp_context_t *ctxt, uint8_t *src, uint8_t *dst)
{
    if (!src || !dst) {
        /* TODO special condition: (find solution in: ctxt)
         *  - Or start
         *  - Or timeout
         */
    }

    switch (_tftp_parse_type(dst))
    {
    case TO_RRQ:
    case TO_RWQ:
    {
        ctxt->op = _tftp_parse_type(dst);
        _tftp_parse_start(src, &(ctxt->file_name), &(ctxt->mode));

        /* TODO send ACK if valid, ERROR if not */
        _tftp_create_ack(dst, 0);
        _tftp_create_error(dst, TE_UN_DEF, "");
    } break;

    case TO_DATA:
    {
        size_t len;
        uint8_t *data;
        uint16_t blk_nr;

        _tftp_parse_data(src, &blk_nr, &data, &len);

        /* TODO send ACK if user finds data, ERROR if not */
        _tftp_create_ack(dst, blk_nr);
        _tftp_create_error(dst, TE_UN_DEF, "");
    } break;

    case TO_ACK:
    {
        uint16_t blk_nr;

        _tftp_parse_ack(src, &blk_nr);

        if (ctxt->data_offset != blk_nr) {
            /* TODO out of sync */
            /* TODO or retry ACK'ed */
        }

        /* TODO check if finished */

        /* TODO read / write the next block */
        ++(ctxt->data_offset);
        //_tftp_create_data(dst, ++blk_nr, data, len);
    } break;

    case TO_ERROR:
    {
        tftp_err_codes_t err;
        const char *err_msg;

        _tftp_parse_error(src, &err, &err_msg);

        /* TODO inform the user that there is an error */
    } break;

    case TO_OACK:
    {
        /* TODO the options where accepted */
    } break;
    }

    return -EINVAL;
}

int _tftp_create_start(uint8_t *dst, tftp_opcodes_t opc, const char *file_name, tftp_mode_t mode)
{
    /* validate parameters */
    if ((opc != TO_RRQ && opc != TO_RWQ) || mode >= ARRAY_LEN(_tftp_modes)) {
        return -EINVAL;
    }

    /* get required values */
    int len = strlen(file_name) + 1;            /* we also want the \0 char */
    tftp_option_t *m = _tftp_modes + mode;

    /* start filling the header */
    tftp_header_t *hdr = (tftp_header_t*)dst;
    hdr->opc = opc;
    memcpy(hdr->data, file_name, len);
    memcpy(hdr->data + len, m->name, m->len);

    /* return the size of the packet */
    return sizeof(tftp_header_t) + len + m->len;
}

int _tftp_create_ack(uint8_t *dst, uint16_t blk_nr)
{
    /* validate parameters */
    if (!dst) {
        return -EINVAL;
    }

    /* fill the packet */
    tftp_packet_data_t *pkt = (tftp_packet_data_t*)dst;
    pkt->opc = TO_ACK;
    pkt->block_nr = HTONS(blk_nr);

    /* return the size of the packet */
    return sizeof(tftp_packet_data_t);
}

int _tftp_create_data(uint8_t *dst, uint16_t blk_nr, uint8_t *data, size_t len)
{
    /* validate parameters */
    if (!dst || !data) {
        return -EINVAL;
    }

    /* fill the packet */
    tftp_packet_data_t *pkt = (tftp_packet_data_t*)dst;
    pkt->opc = TO_DATA;
    pkt->block_nr = HTONS(blk_nr);
    memcpy(pkt->data, data, len);

    /* return the size of the packet */
    return sizeof(tftp_packet_data_t) + len;
}

int _tftp_create_error(uint8_t *dst, tftp_err_codes_t err, const char *err_msg)
{
    /* validate parameters */
    if (!dst) {
        return -EINVAL;
    }

    int strl = err_msg
            ? strlen(err_msg)
            : 0;

    /* fill the packet */
    tftp_packet_error_t *pkt = (tftp_packet_error_t*)dst;
    pkt->opc = err;
    pkt->err_code = err;
    memcpy(pkt->err_msg, err_msg, strl);
    pkt->err_msg[strl] = 0;

    /* return the size of the packet */
    return sizeof(tftp_packet_error_t) + strl + 1;
}

int _tftp_parse_start(uint8_t *src, char **file_name, tftp_mode_t *mode) {
    /* decode the packet */
    tftp_header_t *hdr = (tftp_header_t*)src;
    *file_name = (char*)(hdr->data);

    /* decode the TFTP transfer mode */
    char *str_mode = ((char*) memchr(hdr->data, 0, GNRC_TFTP_MAX_MTU)) + 1; /* TODO get from context */
    if (!str_mode)
        return -EINVAL;

    for (uint32_t idx = 0; idx < ARRAY_LEN(_tftp_modes); ++idx) {
        if (memcmp(_tftp_modes[idx].name, str_mode, _tftp_modes[idx].len) == 0) {
            *mode = (tftp_mode_t)idx;
            return ((str_mode + _tftp_modes[idx].len) - (char*)src);
        }
    }

    return -EINVAL;
}

int _tftp_parse_ack(uint8_t *src, uint16_t *blk_nr) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) src;

    *blk_nr = NTOHS(pkt->block_nr);

    return sizeof(tftp_packet_data_t);
}

int _tftp_parse_data(uint8_t *src, uint16_t *blk_nr, uint8_t **data, size_t *len) {
    tftp_packet_data_t *pkt = (tftp_packet_data_t*) src;

    *blk_nr = NTOHS(pkt->block_nr);
    *data = pkt->data;
    *len = GNRC_TFTP_MAX_MTU;           /* TODO get from context */

    return (sizeof(tftp_packet_data_t) + *len);
}

int _tftp_parse_error(uint8_t *src, tftp_err_codes_t *err, const char **err_msg) {
    tftp_packet_error_t *pkt = (tftp_packet_error_t*) src;

    *err = pkt->err_code;
    *err_msg = pkt->err_msg;

    return (sizeof(tftp_packet_error_t) + strlen(pkt->err_msg) + 1);
}

/**
 * @}
 */

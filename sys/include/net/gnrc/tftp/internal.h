/*
 * Copyright (C) 2015 Nick van IJzendoorn <nijzendoorn@engineering-spirit.nl>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for
 * more details.
 */

/**
 * @defgroup    net_gnrc_tftp  TFTP Support Library
 * @ingroup     net_gnrc
 * @brief       Add's support for TFTP protocol parsing
 * @{
 *
 * @file
 * @brief       TFTP support library
 *
 * @author      Nick van IJzendoorn <nijzendoorn@engineering-spirit.nl>
 */


#ifndef GNRC_TFTP_INTERNAL_H_
#define GNRC_TFTP_INTERNAL_H_

#include <inttypes.h>

#include "byteorder.h"
#include "kernel_types.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/nettype.h"
#include "net/gnrc/tftp.h"
#include "vtimer.h"

#define ENABLE_DEBUG                (0)
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define CT_HTONS(x)         ((          \
                   (x >> 8) & 0x00FF) | \
                   ((x << 8) & 0xFF00))

#define CT_HTONL(x)         ((          \
              (x >> 24) & 0x000000FF) | \
              ((x >> 8) & 0x0000FF00) | \
              ((x << 8) & 0x00FF0000) | \
              ((x << 24) & 0xFF000000))
#else
#define CT_HTONS(x)    (x)
#define CT_HTONL(x)    (x)
#endif

#define MIN(a,b)            ((a) > (b) ? (b) : (a))
#define ARRAY_LEN(x)        (sizeof(x) / sizeof(x[0]))

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

/**
 * @brief TFTP Options
 */
typedef enum {
    TOPT_BLKSIZE,
    TOPT_TSIZE,
    TOPT_TIMEOUT
} tftp_options_t;

/**
 * @brief The TFTP context for the current transfer
 */
typedef struct {
    char *file_name; /* TODO [GNRC_TFTP_MAX_FILENAME_LEN]; */
    tftp_mode_t mode;
    tftp_opcodes_t op;
    ipv6_addr_t peer;
    vtimer_t timer;
    uint16_t port;

    /* transfer parameters */
    uint16_t data_offset;
    uint16_t block_size;
    uint16_t transfer_size;
    uint16_t block_timeout;
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
    uint16_t block_nr;
    uint8_t data[];
} tftp_packet_data_t;

/**
 * @brief The TFTP ACK packet
 */
typedef struct __attribute__((packed)) {
    tftp_opcodes_t opc          : 16;
    tftp_err_codes_t err_code   : 16;
    char err_msg[];
} tftp_packet_error_t;

extern int _tftp_state_processes(tftp_context_t *ctxt, uint8_t *src, uint8_t *dst);

extern int _tftp_create_start(uint8_t *dst, tftp_opcodes_t opc, const char *file_name, tftp_mode_t mode);

extern int _tftp_create_ack(uint8_t *dst, uint16_t blk_nr);

extern int _tftp_create_data(uint8_t *dst, uint16_t blk_nr, uint8_t *data, size_t len);

extern int _tftp_create_error(uint8_t *dst, tftp_err_codes_t err, const char *err_msg);

static inline tftp_opcodes_t _tftp_parse_type(uint8_t *src) {
    return ((tftp_header_t*)src)->opc;
}

extern int _tftp_parse_start(uint8_t *src, char **file_name, tftp_mode_t *mode);

extern int _tftp_parse_ack(uint8_t *src, uint16_t *blk_nr);

extern int _tftp_parse_data(uint8_t *src, uint16_t *blk_nr, uint8_t **data, size_t *len);

extern int _tftp_parse_error(uint8_t *src, tftp_err_codes_t *err, const char **err_msg);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_TFTP_H_ */

/**
 * @}
 */

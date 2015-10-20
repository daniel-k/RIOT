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
 * The TFTP module add's support for the TFTP protocol.
 * It implements the following RFC's:
 *  - https://tools.ietf.org/html/rfc1350
 *     (RFC1350 The TFTP Protocol (Revision 2)
 *
 *  - https://tools.ietf.org/html/rfc2347
 *     (RFC2347 TFTP Option Extension)
 *
 *  - https://tools.ietf.org/html/rfc2348
 *     (RFC2348 TFTP Blocksize Option)
 *
 *  - https://tools.ietf.org/html/rfc2349
 *     (RFC2349 TFTP Timeout Interval and Transfer Size Options)
 *
 * @author      Nick van IJzendoorn <nijzendoorn@engineering-spirit.nl>
 */

#ifndef GNRC_TFTP_H_
#define GNRC_TFTP_H_

#include <inttypes.h>

#include "byteorder.h"
#include "kernel_types.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/nettype.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GNRC_TFTP_MAX_FILENAME_LEN
#define GNRC_TFTP_MAX_FILENAME_LEN          (64)
#endif

#ifndef GNRC_TFTP_MAX_SERVER_CLIENTS
#define GNRC_TFTP_MAX_SERVER_CLIENTS        (2)
#endif

#ifndef GNRC_TFTP_DEFAULT_SRC_PORT
#define GNRC_TFTP_DEFAULT_SRC_PORT          (10690)
#endif

#ifndef GNRC_TFTP_DEFAULT_DST_PORT
#define GNRC_TFTP_DEFAULT_DST_PORT          (69)
#endif

#ifndef GNRC_TFTP_MAX_TRANSFER_UNIT
#define GNRC_TFTP_MAX_TRANSFER_UNIT         (512)
#endif

#ifndef GNRC_TFTP_MAX_RETRIES
#define GNRC_TFTP_MAX_RETRIES               (5)
#endif

/**
 * @brief TFTP action to
 */
typedef enum {
    TFTP_READ,
    TFTP_WRITE
} tftp_action_t;

/**
 * @brief   callback define which is called when a new server request is placed
 *          or when an client read request is made and the data length option is received
 */
typedef bool (*tftp_transfer_start_callback)(tftp_action_t action, const char *file_name, size_t data_len);

/**
 * @brief   callback define which is called to get or set data from/to the user application
 */
typedef int (*tftp_data_callback)(uint32_t offset, void *data, uint32_t data_len);

/**
<<<<<<< HEAD
 * @brief Start an TFTP client read action from the given destination
 *
 * @param [in] addr         the address of the server
 * @param [in] file_name    the filename of the file to get
=======
 * @brief Start an TFTP server
 *
>>>>>>> ce6ac59aff53a20b2a06b65b722595e08a9c084a
 * @param [in] data_cb      the callback which is called for each read data block
 * @param [in] start_cb     the callback which is called if the server returns the transfer_size option
 *
 * @return 1 on success
 * @return -1 on failure
 */
extern int gnrc_tftp_server(tftp_data_callback data_cb, tftp_transfer_start_callback start_cb);

/**
 * @brief Start an TFTP client read action from the given destination
 *
 * @param [in] addr         the address of the server
 * @param [in] file_name    the filename of the file to get
 * @param [in] data_cb      the callback which is called for each read data block
 * @param [in] start_cb     the callback which is called if the server returns the transfer_size option
 *
 * @return 1 on success
 * @return -1 on failure
 */
extern int gnrc_tftp_client_read(ipv6_addr_t *addr, const char *file_name, tftp_data_callback data_cb, tftp_transfer_start_callback start_cb);

/**
 * @brief Start an TFTP client write action to the given destination
 *
 * @param [in] addr         the address of the server
 * @param [in] file_name    the filename of the file to write
 * @param [in] data_cb      the callback which is called to store the received block
 * @param [in] total_size   the total size of the transfer
 *
 * @return 1 on success
 * @return -1 on failure
 */
extern int gnrc_tftp_client_write(ipv6_addr_t *addr, const char *file_name, tftp_data_callback data_cb, uint32_t total_size);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_TFTP_H_ */

/**
 * @}
 */

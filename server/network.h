#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>

/*
 * Read exactly n bytes from fd into buf.
 * Returns:
 *   1  on success
 *   0  if peer closed connection before all bytes were read
 *  -1  on error
 */
int read_nbytes(int fd, void *buf, size_t n);

/*
 * Write exactly n bytes from buf to fd.
 * Returns:
 *   1  on success
 *   0  if connection closed before all bytes were written
 *  -1  on error
 */
int write_nbytes(int fd, const void *buf, size_t n);

/*
 * Send one protocol message:
 *   [1-byte opcode][payload bytes]
 *
 * payload may be NULL if payload_size is 0.
 *
 * Returns:
 *   1  on success
 *   0  if connection closed during send
 *  -1  on error
 */
int send_message(int fd, uint8_t opcode, const void *payload, size_t payload_size);

/*
 * Receive one protocol message.
 *
 * First reads the 1-byte opcode, then reads the corresponding payload
 * based on payload_size(opcode) from protocol.h.
 *
 * Parameters:
 *   fd            socket/file descriptor
 *   opcode_out    where the opcode is stored
 *   payload_buf   buffer to receive payload bytes
 *   buf_size      size of payload_buf
 *
 * Returns:
 *   1  on success
 *   0  if peer closed connection cleanly
 *  -1  on error, unknown opcode, or buffer too small
 */
int recv_message(int fd, uint8_t *opcode_out, void *payload_buf, size_t buf_size);

#endif /* NETWORK_H */
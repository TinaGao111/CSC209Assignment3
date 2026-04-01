#include "network.h"
#include "protocol.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int read_nbytes(int fd, void *buf, size_t n) {
    size_t total_read = 0;
    char *p = (char *)buf;

    while (total_read < n) {
        ssize_t bytes_read = read(fd, p + total_read, n - total_read);

        if (bytes_read == 0) {
            /* Peer closed connection */
            return 0;
        }

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal so try again */
            }
            return -1;
        }

        total_read += (size_t)bytes_read;
    }

    return 1;
}

int write_nbytes(int fd, const void *buf, size_t n) {
    size_t total_written = 0;
    const char *p = (const char *)buf;

    while (total_written < n) {
        ssize_t bytes_written = write(fd, p + total_written, n - total_written);

        if (bytes_written == 0) {
            /* Treat as closed connection */
            return 0;
        }

        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        total_written += (size_t)bytes_written;
    }

    return 1;
}

int send_message(int fd, uint8_t opcode, const void *payload, size_t payload_size_bytes) {
    int result;

    result = write_nbytes(fd, &opcode, sizeof(opcode));
    if (result != 1) {
        return result; /* If opcode failed, stop */
    }

    if (payload_size_bytes > 0) {
        result = write_nbytes(fd, payload, payload_size_bytes);
        if (result != 1) {
            return result;
        }
    }

    return 1;
}

int recv_message(int fd, uint8_t *opcode_out, void *payload_buf, size_t buf_size) {
    int result;
    size_t expected_size;
    uint8_t opcode;

    if (opcode_out == NULL) {
        return -1;
    }

    result = read_nbytes(fd, &opcode, sizeof(opcode));
    if (result != 1) {
        return result;
    }

    expected_size = payload_size(opcode);
    if (expected_size == 0) {
        /* Unknown opcode or opcode with invalid size */
        return -1;
    }

    if (expected_size > buf_size) {
        return -1;
    }

    result = read_nbytes(fd, payload_buf, expected_size);
    if (result != 1) {
        return result;
    }

    *opcode_out = opcode;
    return 1;
}
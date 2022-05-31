#include "io.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int32_t sock_listen = -1;
static int32_t sock_client = -1;

/**
 * @brief Parse a string to a port (some number between 0 and 65535).
 * @param port_str Null-terminated string containing the port.
 * @return Port number on success, 0 on failure, or UINT16_MAX on failure.
 */
static uint16_t port_parse(char const *const port_str)
{
    uint64_t const sock_port_raw = strtoul(port_str, NULL, 10U);
    static_assert(sizeof(unsigned long int) <= sizeof(uint64_t),
                  "Expected strtoul to return an unsigned int of size <=8");
    if (sock_port_raw == 0U || sock_port_raw > UINT16_MAX)
    {
        printf("Failed to convert port argument to a number\n");
        return 0;
    }
    return (uint16_t)sock_port_raw; /* Safe cast due to bound checks. */
}

/**
 * @brief A shorthand for getting a TCP socket for listening for client
 * connections.
 * @param port What port to create the listening socket on.
 * @return Socket FD on success, -1 on failure.
 */
static int32_t sock_listen_create(uint16_t const port)
{
    int32_t const sock = socket(AF_INET, SOCK_STREAM, 0U);
    if (sock != -1)
    {
        struct sockaddr_in const sock_addr = {.sin_zero = {0},
                                              .sin_family = AF_INET,
                                              .sin_addr.s_addr = INADDR_ANY,
                                              .sin_port = htons(port)};
        if (bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != -1)
        {
            /* Make sure not to queue connections (hence the 0 here). */
            if (listen(sock, 0U) != -1)
            {
                printf("Listening on port %u.\n", port);
                return sock;
            }
            else
            {
                printf("Call to listen() failed: %s.\n", strerror(errno));
            }
        }
        else
        {
            printf("Call to bind() failed: %s.\n", strerror(errno));
        }
        if (close(sock) == -1)
        {
            printf("Call to close() failed: %s.\n", strerror(errno));
        }
    }
    else
    {
        printf("Call to socket() failed: %s.\n", strerror(errno));
    }
    return -1;
}

/**
 * @brief Closes the socket and invalidates the value of the socket at the
 * pointed location.
 * @param sock Where the socket FD is stored.
 * @return 0 on success, -1 on failure.
 */
static int32_t sock_close(int32_t *const sock)
{
    bool success = true;
    if (*sock != -1)
    {
        if (shutdown(*sock, SHUT_RDWR) == -1)
        {
            printf("Call to shutdown() failed: %s.\n", strerror(errno));
            /* A failed shutdown is only a problem for the client. */
        }
        if (close(*sock) == -1)
        {
            printf("Call to close() failed: %s.\n", strerror(errno));
            success = success && false;
        }
        *sock = -1;
    }
    *sock = -1;
    if (success)
    {
        return 0;
    }
    /**
     * On failure, socket is still set to -1 so it is lost forever because there
     * is no way to recover here.
     */
    return -1;
}

static int64_t msg_send(int32_t const *const sock, msg_st const *const msg)
{
    if (msg->hdr.size > sizeof(msg->data))
    {
        printf("Message header indicates a data size larger than the buffer "
               "itself.\n");
        return -1;
    }

    uint32_t const size_msg = (uint32_t)sizeof(msg_hdr_st) + msg->hdr.size;
    int64_t const sent_bytes = send(sock_client, &msg->hdr, size_msg, 0U);
    /* Safe cast since the target type can fit the sum of cast ones. */
    if (sent_bytes == size_msg)
    {
        /* Success. */
    }
    else if (sent_bytes < 0)
    {
        printf("Call to send() failed: %s.\n", strerror(errno));
    }
    else
    {
        printf("Failed to send message.\n");
        return -1;
    }

    if (sent_bytes != size_msg)
    {
        printf("Failed to send all the message bytes.\n");
        if (sock_close(&sock_client) != 0)
        {
            printf("Failed to close client socket.\n");
            return -1;
        }
    }

    printf("TX:\n");
    io_dbg_msg_print(msg);
    return sent_bytes;
}

static int64_t msg_recv(int32_t const *const sock, msg_st *const msg)
{
    bool recv_failure = false;
    int64_t recvd_bytes;
    recvd_bytes = recv(sock_client, &msg->hdr, sizeof(msg_hdr_st), 0U);
    do
    {
        /* Check if succeeded. */
        if (recvd_bytes == sizeof(msg_hdr_st))
        {
            /**
             * Check if the indicated size is too large for the static
             * message data buffer.
             */
            if (msg->hdr.size > sizeof(msg->data) ||
                msg->hdr.size < offsetof(msg_data_st, buf))
            {
                printf("Value of the size field in the message header is too "
                       "large. Got %u, expected %lu >= n <= %lu.\n",
                       msg->hdr.size, offsetof(msg_data_st, buf),
                       sizeof(msg->data));
                recv_failure = true;
                break;
            }
            recvd_bytes = recv(sock_client, &msg->data, msg->hdr.size, 0);
            /**
             * Make sure we received the whole message also the cast here is
             * safe.
             */
            if (recvd_bytes != (int64_t)msg->hdr.size)
            {
                recv_failure = true;
                break;
            }
        }
        else
        {
            printf("Failed to receive message header.\n");
            recv_failure = true;
            break;
        }
    } while (0U);
    if (recvd_bytes < 0)
    {
        printf("Call to recv() failed: %s.\n", strerror(errno));
    }

    if (recv_failure)
    {
        printf("Failed to receive message.\n");
        return -1;
    }
    else
    {
        printf("RX:\n");
        io_dbg_msg_print(msg);
        return recvd_bytes;
    }
}

int32_t io_init(char const *const port_str)
{
    uint16_t const sock_port = port_parse(port_str);
    sock_listen = sock_listen_create(sock_port);
    if (sock_listen != 0)
    {
        return 0;
    }
    return -1;
}

int32_t io_recv(msg_st *const msg, bool const init)
{
    /**
     * @todo Move this out so that sending can lead to accepting a new
     * connection.
     */
    if (sock_client == -1)
    {
        for (;;)
        {
            sock_client = accept(sock_listen, NULL, NULL);
            if (sock_client != -1)
            {
                printf("Client connected.\n");
                break;
            }
            else if (sock_client == ECONNABORTED || sock_client == EPERM ||
                     sock_client == EPROTO)
            {
                printf("Failed to accept a client connection because of "
                       "client-side problems, retrying...\n");
            }
            else
            {
                printf("Failed to accept a client connection. Call to accept() "
                       "failed: %s.\n",
                       strerror(errno));
                break;
            }
        }
        if (sock_client == -1)
        {
            return -1;
        }
    }

    int64_t const sent_bytes = init ? msg_send(&sock_client, msg) : 0;
    int64_t const recvd_bytes = msg_recv(&sock_client, msg);

    /**
     * Read from client socket, if that fails, close it and return some code
     * to indicate it.
     */
    if (recvd_bytes < 0 || sent_bytes < 0)
    {
        if (sock_close(&sock_client) != 0)
        {
            printf("Failed to close client socket.\n");
            return -1;
        }
        printf("Client disconnected.\n");
        return 1;
    }
    return 0;
}

int32_t io_send(msg_st const *const msg)
{
    if (msg_send(&sock_client, msg) >= 0)
    {
        return 0;
    }
    return -1;
}

int32_t io_fini()
{
    if (sock_close(&sock_listen) == 0 && sock_close(&sock_client) == 0)
    {
        return 0;
    }
    return -1;
}

void io_dbg_msg_print(msg_st const *const msg)
{
    printf("(Message"
           "\n    (Header (Size %u))"
           "\n    (Data"
           "\n        (Cont 0x%08X)"
           "\n        (BufLenExp %u)"
           "\n        (Buf [",
           msg->hdr.size, msg->data.cont_state, msg->data.buf_len_exp);
    if (msg->hdr.size > sizeof(msg->data.buf) ||
        msg->hdr.size < offsetof(msg_data_st, buf))
    {
        printf(" invalid");
    }
    else
    {
        for (uint32_t data_idx = 0U;
             data_idx < msg->hdr.size - offsetof(msg_data_st, buf); ++data_idx)
        {
            printf(" %02X", msg->data.buf[data_idx]);
        }
    }
    printf(" ])))\n");
}

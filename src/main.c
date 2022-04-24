#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <uicc.h>
#include <unistd.h>

/* These are here so that the signal handler can close them. */
static int32_t sock_listen = -1;
static int32_t sock_client = -1;

/**
 * @brief Exit gracefully.
 * @param signum The signal received.
 */
static void signal_exit_handler(__attribute__((unused)) int signum)
{
    printf("Shutting down...\n");
    bool success = true;
    if (sock_listen != -1)
    {
        if (shutdown(sock_listen, SHUT_RDWR) == -1)
        {
            printf("Call to shutdown() failed: %s\n", strerror(errno));
            success = success && false;
        }
        if (close(sock_listen) == -1)
        {
            printf("Call to close() failed: %s\n", strerror(errno));
            success = success && false;
        }
    }
    if (sock_client != -1)
    {
        if (shutdown(sock_listen, SHUT_RDWR) == -1)
        {
            printf("Call to shutdown() failed: %s\n", strerror(errno));
            success = success && false;
        }
        if (close(sock_listen) == -1)
        {
            printf("Call to close() failed: %s\n", strerror(errno));
            success = success && false;
        }
    }
    if (success)
    {
        exit(0);
    }
    exit(-1);
}

/**
 * @brief Register a handler for SIGINT, SIGTERM, and SIGHUP, i.e. all signals
 * requesting the program to exit.
 * @param handle_term The handler to register for the signals.
 * @return 0 on success, -1 on failure.
 * @note May exit if gets itself to an unrecoverable state.
 */
static int32_t signal_exit_handler_register(void (*handle_term)(int))
{
    struct sigaction action_new, action_old;
    action_new.sa_handler = handle_term;
    if (sigemptyset(&action_new.sa_mask) != 0)
    {
        printf("Call to sigemptyset() failed: %s\n", strerror(errno));
        return -1;
    }
    action_new.sa_flags = 0;

    if (sigaction(SIGINT, NULL, &action_old) == 0)
    {
        if (action_old.sa_handler != SIG_IGN)
        {
            if (sigaction(SIGINT, &action_new, NULL) == 0)
            {
                if (sigaction(SIGHUP, NULL, &action_old) == 0)
                {
                    if (action_old.sa_handler != SIG_IGN)
                    {
                        if (sigaction(SIGHUP, &action_new, NULL) == 0)
                        {
                            if (sigaction(SIGTERM, NULL, &action_old) == 0)
                            {
                                if (action_old.sa_handler != SIG_IGN)
                                {
                                    if (sigaction(SIGTERM, &action_new, NULL) ==
                                        0)
                                    {
                                        return 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    /**
     * Very difficult to recover here such that the state before and after call
     * (in terms of signal handler registration) so better to exit here.
     */
    printf("Failed to register signal handler for one or more signals\n");
    exit(-1);
}

/**
 * @brief Print usage.
 */
static void usage()
{
    printf("Usage: softsim port\n");
}

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
static int32_t listen_tcp(uint16_t const port)
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
                return sock;
            }
            else
            {
                printf("Call to listen() failed: %s\n", strerror(errno));
            }
        }
        else
        {
            printf("Call to bind() failed: %s\n", strerror(errno));
        }
        if (close(sock) == -1)
        {
            printf("Call to close() failed: %s\n", strerror(errno));
        }
    }
    else
    {
        printf("Call to socket() failed: %s\n", strerror(errno));
    }
    return -1;
}

int32_t main(int32_t const argc,
             __attribute__((unused)) char const *const argv[argc])
{
    if (argc != 2)
    {
        printf("Expected 1 argument but received %u\n", argc);
        usage();
        return -1;
    }

    if (signal_exit_handler_register(signal_exit_handler) != 0)
    {
        printf("Failed to register signal handler\n");
        return -1;
    }

    uint16_t const sock_port = port_parse(argv[1U]);
    sock_listen = listen_tcp(sock_port);
    if (sock_listen != -1)
    {
        /* Have a valid socket to listen for client connections. */
        for (;;)
        {
            sock_client = accept(sock_listen, NULL, NULL);
            if (sock_client != -1)
            {
                /* A client has connected. */
                printf("Client connected\n");
                /* TODO: Receive, parse, and pass to USIM. */
                printf("Client disconnected\n");
                if (shutdown(sock_client, SHUT_RDWR) == -1)
                {
                    printf("Call to shutdown() failed: %s\n", strerror(errno));
                    break;
                }
                if (close(sock_client) == -1)
                {
                    printf("Call to close() failed: %s\n", strerror(errno));
                    break;
                }
                /**
                 * Invalid between the time one client disconnects and another
                 * connects.
                 */
                sock_client = -1;
            }
            else if (sock_client == ECONNABORTED || sock_client == EPERM ||
                     sock_client == EPROTO)
            {
                printf("Failed to accept a client connection because of "
                       "client-side problems, retrying...\n");
            }
            else
            {
                printf("Failed to accept a client connection\n");
                break;
            }
        }
    }
    else
    {
        printf("Failed to get a listen socket\n");
    }

    if (raise(SIGTERM) != 0)
    {
        printf("Graceful shutdown failed\n");
        return -1;
    }
    return 0;
}

#include "io.h"
#include "sig.h"
#include "usim.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Print usage.
 */
static void usage()
{
    printf("Usage: swsim port\n");
}

int32_t main(int32_t const argc,
             __attribute__((unused)) char const *const argv[argc])
{
    if (argc != 2)
    {
        printf("Expected 1 argument but received %u\n", argc - 1);
        usage();
        return -1;
    }

    if (sig_exit_handler_register() != 0)
    {
        printf("Failed to register signal handler\n");
        return -1;
    }

    if (io_init(argv[1U]) == 0)
    {
        usim_st usim;
        if (usim_init(&usim) != 0)
        {
            printf("Failed to init USIM\n");
            if (raise(SIGTERM) != 0)
            {
                return -1;
            }
        }

        uint8_t buf_tx[UICC_DATA_MAX];
        uint16_t const buf_tx_len = sizeof(buf_tx);

        msg_st msg_rx;
        msg_st msg_tx;
        int32_t recv_ret = -1;
        while ((recv_ret = io_recv(&msg_rx)) >= 0)
        {
            if (recv_ret == 0)
            {
                printf("RX:\n");
                io_dbg_msg_print(&msg_rx);
                uint32_t const buf_rx_len = msg_rx.hdr.size - 4U;
                if (buf_rx_len <= UINT16_MAX)
                {
                    usim.uicc.buf_rx = msg_rx.data.tpdu;
                    usim.uicc.buf_rx_len = (uint16_t)
                        buf_rx_len; /* Safe cast due to bound check. */
                    usim.uicc.buf_tx = buf_tx;
                    usim.uicc.buf_tx_len = buf_tx_len;
                    if (usim_io(&usim) == 0)
                    {
                        msg_tx.hdr.size = sizeof(msg_tx.data.cont_state) +
                                          usim.uicc.buf_tx_len;
                        msg_tx.data.cont_state = 0U;
                        memcpy(msg_tx.data.tpdu, usim.uicc.buf_tx,
                               usim.uicc.buf_tx_len);
                        if (io_send(&msg_tx) != 0)
                        {
                            printf("Failed to send response\n");
                            break;
                        }
                        else
                        {
                            printf("TX:\n");
                            io_dbg_msg_print(&msg_tx);
                        }
                    }
                    else
                    {
                        printf("USIM IO failed\n");
                    }
                }
            }
        }
    }
    else
    {
        printf("Failed to init IO\n");
    }

    if (raise(SIGTERM) != 0)
    {
        printf("Graceful shutdown failed\n");
        return -1;
    }
    return 0;
}

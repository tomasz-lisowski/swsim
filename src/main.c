#include "io.h"
#include "sig.h"
#include "usim.h"
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DBG_STR_LEN 16384U

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
        printf("Expected 1 argument but received %u.\n", argc - 1);
        usage();
        return -1;
    }

    if (sig_exit_handler_register() != 0)
    {
        printf("Failed to register signal handler.\n");
        return -1;
    }

    if (io_init(argv[1U]) == 0)
    {
        msg_st msg_rx;
        msg_st msg_tx;
        usim_st usim = {0};

        usim.dbg_str_size = DBG_STR_LEN;
        usim.dbg_str_len = usim.dbg_str_size;
#ifdef DEBUG
        char dbg_str[DBG_STR_LEN];
        usim.dbg_str = dbg_str;
#else
        usim.dbg_str = NULL;
#endif

        usim.uicc.buf_rx = msg_rx.data.buf;
        usim.uicc.buf_rx_len = 0U;
        usim.uicc.buf_tx = msg_tx.data.buf;
        usim.uicc.buf_tx_len = sizeof(msg_tx.data.buf);

        if (usim_init(&usim) != 0)
        {
            printf("Failed to init USIM.\n");
            if (raise(SIGTERM) != 0)
            {
                return -1;
            }
        }

        int32_t recv_ret = -1;
        /* When a client connects, send it an init message. */
        msg_rx.data.buf_len_exp = usim.uicc.buf_rx_len;
        msg_rx.data.cont_state = 0U;
        msg_rx.hdr.size = offsetof(msg_data_st, buf);
        bool init = true;
        while ((recv_ret = io_recv(&msg_rx, init)) >= 0)
        {
            /* Only the first recv shall indicate init. */
            init = false;

            if (recv_ret == 0)
            {
                /* Safe cast since buf is not offset further than 255 bytes. */
                uint32_t const buf_rx_len =
                    msg_rx.hdr.size - (uint8_t)offsetof(msg_data_st, buf);
                if (buf_rx_len <= UINT16_MAX)
                {
                    usim.uicc.buf_rx = msg_rx.data.buf;
                    usim.uicc.buf_rx_len = (uint16_t)
                        buf_rx_len; /* Safe cast due to bound check. */
                    usim.uicc.buf_tx = msg_tx.data.buf;
                    usim.uicc.buf_tx_len = sizeof(msg_tx.data.buf);
                    if (usim_io(&usim) == 0)
                    {
                        if (sizeof(msg_tx.data.cont_state) +
                                usim.uicc.buf_tx_len >
                            UINT32_MAX)
                        {
                            break;
                        }
                        /* Safe cast because it was checked. */
                        msg_tx.hdr.size =
                            (uint32_t)(sizeof(msg_tx.data.cont_state) +
                                       sizeof(msg_tx.data.buf_len_exp) +
                                       usim.uicc.buf_tx_len);
                        msg_tx.data.cont_state = 0U;
                        msg_tx.data.buf_len_exp = usim.uicc.buf_rx_len;
                        memcpy(msg_tx.data.buf, usim.uicc.buf_tx,
                               usim.uicc.buf_tx_len);
                        if (io_send(&msg_tx) != 0)
                        {
                            printf("Failed to send response.\n");
                            break;
                        }
                    }
                    else
                    {
                        printf("USIM IO failed.\n");
                    }
                }
            }
        }
    }
    else
    {
        printf("Failed to init IO.\n");
    }

    if (raise(SIGTERM) != 0)
    {
        printf("Graceful shutdown failed.\n");
        return -1;
    }
    return 0;
}

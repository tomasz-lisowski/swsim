#include "usim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Establish a connection with a real SIM card inserted into a PC/SC
 * reader.
 * @param ctx The scraw context to use for storing a connection with the card.
 * This context must be initialized.
 * @return 0 on success, -1 on failure.
 * @note If there is more than 1 reader available, one will be selected
 * automatically.
 */
static int32_t simcard_connect(usim_st *const state)
{
    /* Free the old selected readear. */
    if (state->internal.reader_selected != NULL)
    {
        free(state->internal.reader_selected);
        state->internal.reader_selected = NULL;
    }

    /* Find the first reader with a card can we can connect to. */
    scraw_st *const ctx = &state->internal.scraw_ctx;
    int32_t ret = -1;
    char *reader_name_selected = NULL;
    if (scraw_reader_search_begin(ctx) == 0)
    {
        bool card_found = false;
        char const *reader_name_tmp = NULL;
        for (uint32_t reader_idx = 0U;; ++reader_idx)
        {
            ret = scraw_reader_search_next(ctx, &reader_name_tmp);
            if (ret == 0)
            {
                uint64_t const reader_name_len = strlen(reader_name_tmp);
                reader_name_selected = malloc(reader_name_len);
                if (reader_name_selected == NULL)
                {
                    printf("Failed to allocate memory for reader name\n");
                }
                else
                {
                    memcpy(reader_name_selected, reader_name_tmp,
                           reader_name_len);
                    if (scraw_reader_select(ctx, reader_name_selected) == 0)
                    {
                        printf("Selected reader %u (%s)\n", reader_idx,
                               reader_name_selected);
                        if (scraw_card_connect(ctx, SCRAW_PROTO_T0) == 0)
                        {
                            card_found = true;
                            printf("Connected to card\n");
                            /**
                             * Selected a reader and connected to card so can
                             * carry on.
                             */
                            break;
                        }
                        else
                        {
                            printf("Failed to connect to card\n");
                        }
                    }
                    /**
                     * Free the memory allocated for a reader that did not end
                     * up being selected.
                     */
                    free(state->internal.reader_selected);
                    state->internal.reader_selected = NULL;
                }
            }
            else if (ret == 1)
            {
                printf("Reached end of reader list\n");
                break;
            }
            else
            {
                break;
            }
        }
        if (scraw_reader_search_end(ctx) != 0)
        {
            printf("Failed to end reader search but continuing\n");
        }

        if (card_found)
        {
            return 0; /* Card connected. */
        }
        else
        {
            return -1;
        }
    }
    else
    {
        printf("Failed to search SIM card readers\n");
        return -1;
    }
}

int32_t usim_init(usim_st *const state)
{
    state->internal.reader_selected = NULL;
    int32_t ret = scraw_init(&state->internal.scraw_ctx);
    if (ret == 0)
    {
        if (simcard_connect(state) == 0)
        {
            return 0;
        }
        else
        {
            if (scraw_fini(&state->internal.scraw_ctx) != 0)
            {
                /**
                 * The scraw context is not cleared because that would just be a
                 * potential memory leak. Safer by caller to exit than try to
                 * recover but it's their problem.
                 */
                printf("Failed to de-initialize scraw lib\n");
            }
        }
    }
    else
    {
        printf("Failed to initialize scraw lib\n");
    }
    return -1;
}

int32_t usim_reset_mock(usim_st *const state)
{
    return -1;
}

int32_t usim_io(usim_st *const state)
{
    scraw_raw_st tpdu = {
        .buf = state->uicc.buf_rx,
        .len = state->uicc.buf_rx_len,
    };
    scraw_res_st res = {
        .buf = state->uicc.buf_tx,
        .buf_len = state->uicc.buf_tx_len,
        .res_len = 0,
    };
    int32_t ret = scraw_send(&state->internal.scraw_ctx, &tpdu, &res);
    if (ret != 0)
    {
        printf("Pass-through to SIM failed: %i (%li)\n", ret,
               state->internal.scraw_ctx.errno);
        return -1;
    }
    if (res.res_len > UINT16_MAX)
    {
        /* This should never happen. */
        return -1;
    }
    state->uicc.buf_tx_len =
        (uint16_t)res.res_len; /* Safe cast due to bound check */
    return 0;
}

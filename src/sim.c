#include "sim.h"
#include "apduh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void swicc_state_print(sim_st *const state)
{
    swicc_fsm_state_et state_fsm_tmp;
    swicc_fsm_state(&state->swicc, &state_fsm_tmp);

    /* Print FSM state. */
    state->dbg_str_len = state->dbg_str_size;
    swicc_ret_et const ret_fsm_str = swicc_dbg_fsm_state_str(
        state->dbg_str, &state->dbg_str_len, state_fsm_tmp);
    if (ret_fsm_str != SWICC_RET_SUCCESS)
    {
        return;
    }
    printf("%.*s", state->dbg_str_len, state->dbg_str);

    /* Print IO contact state. */
    state->dbg_str_len = state->dbg_str_size;
    swicc_ret_et const ret_io_cont_str = swicc_dbg_io_cont_str(
        state->dbg_str, &state->dbg_str_len, state->swicc.cont_state_rx,
        state->swicc.cont_state_tx);
    if (ret_io_cont_str != SWICC_RET_SUCCESS)
    {
        return;
    }
    printf("%.*s", state->dbg_str_len, state->dbg_str);
}

static void sim_rx_print(sim_st *const state)
{
    printf("(RX");
    for (uint16_t rx_idx = 0U; rx_idx < state->swicc.buf_rx_len; ++rx_idx)
    {
        printf(" %02X", state->swicc.buf_rx[rx_idx]);
    }
    printf(")\n");
}

static void sim_tx_print(sim_st *const state)
{
    printf("(TX");
    for (uint16_t tx_idx = 0U; tx_idx < state->swicc.buf_tx_len; ++tx_idx)
    {
        printf(" %02X", state->swicc.buf_tx[tx_idx]);
    }
    printf(")\n");
}

#if SIM_PASSTHROUGH == 1
/**
 * @brief Establish a connection with a real SIM card inserted into a PC/SC
 * reader.
 * @param ctx The scraw context to use for storing a connection with the card.
 * This context must be initialized.
 * @return 0 on success, -1 on failure.
 * @note If there is more than 1 reader available, one will be selected
 * automatically.
 */
static int32_t simcard_connect(sim_st *const state)
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
                    printf("Failed to allocate memory for reader name.\n");
                }
                else
                {
                    memcpy(reader_name_selected, reader_name_tmp,
                           reader_name_len);
                    if (scraw_reader_select(ctx, reader_name_selected) == 0)
                    {
                        printf("Selected reader %u: '%s'.\n", reader_idx,
                               reader_name_selected);
                        if (scraw_card_connect(ctx, SCRAW_PROTO_T0) == 0)
                        {
                            card_found = true;
                            printf("Connected to card.\n");
                            /**
                             * Selected a reader and connected to card so can
                             * carry on.
                             */
                            break;
                        }
                        else
                        {
                            printf("Failed to connect to card.\n");
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
                printf("Reached end of reader list.\n");
                break;
            }
            else
            {
                break;
            }
        }
        if (scraw_reader_search_end(ctx) != 0)
        {
            printf("Failed to end reader search but continuing.\n");
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
        printf("Failed to search SIM card readers.\n");
        return -1;
    }
}
#endif

int32_t sim_init(sim_st *const state)
{
#if SIM_PASSTHROUGH == 1
    int32_t ret = scraw_init(&state->internal.scraw_ctx);
    if (ret == 0)
    {
        if (simcard_connect(state) != 0)
        {
            if (scraw_fini(&state->internal.scraw_ctx) != 0)
            {
                /**
                 * The scraw context is not cleared because that would just be a
                 * potential memory leak. Safer for caller to exit than try to
                 * recover but it's their problem.
                 */
                printf("Failed to de-initialize scraw lib.\n");
            }
            return -1;
        }
    }
    else
    {
        printf("Failed to initialize scraw lib.\n");
    }
#endif

    state->internal.reader_selected = NULL;
    swicc_disk_st disk = {0};
    swicc_ret_et const ret_disk =
        swicc_diskjs_disk_create(&disk, "./data/fs.json");
    if (ret_disk == SWICC_RET_SUCCESS)
    {
        if (swicc_disk_save(&disk, "./fs.swicc") == SWICC_RET_SUCCESS)
        {
            state->dbg_str_len = state->dbg_str_size;
            swicc_dbg_disk_str(state->dbg_str, &state->dbg_str_len, &disk);
            printf("%.*s", state->dbg_str_len, state->dbg_str);
            if (swicc_fs_disk_mount(&state->swicc, &disk) == SWICC_RET_SUCCESS)
            {
                if (swicc_apduh_pro_register(&state->swicc, sim_apduh_demux) ==
                    SWICC_RET_SUCCESS)
                {
                    state->dbg_str_len = state->dbg_str_size;
                    if (sim_init_mock(state) == 0)
                    {
                        return 0;
                    }
                    else
                    {
                        printf("Failed to mock the init of swICC.\n");
                    }
                }
                else
                {
                    printf("Failed to register a proprietary APDU handler.\n");
                }
            }
            else
            {
                printf("Failed to mount disk.\n");
            }
        }
        else
        {
            printf("Failed to save disk.\n");
        }
        swicc_disk_unload(&disk);
    }
    else
    {
        printf("Failed to create disk: %s.\n", swicc_dbg_ret_str(ret_disk));
    }
    return -1;
}

int32_t sim_init_mock(sim_st *const state)
{
    static uint8_t const pps_req[] = {0xFF, 0x10, 0x94, 0x7B};
    static_assert(sizeof(pps_req) / sizeof(pps_req[0U] >= 2),
                  "PPS (req/res) must be at least 2 bytes long.");
    uint8_t const pps_req_len = sizeof(pps_req) / sizeof(pps_req[0U]);

    /* All contact states are set to valid. */
    state->swicc.cont_state_rx = SWICC_IO_CONT_VALID_ALL;

    uint16_t const buf_tx_size = state->swicc.buf_tx_len;

    swicc_fsm_state_et state_fsm;
    printf("Mocking swICC cold reset...\n");

    bool const mock_pps = false;

    for (uint8_t step = 0U;; ++step)
    {
        /* Setup buffers and user-controlled state before calling swICC IO. */
        switch (step)
        {
        case 0:
            printf(" 0. Interface sets RST to state L.\n");
            state->swicc.cont_state_rx &= ~((uint32_t)SWICC_IO_CONT_RST);
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 1:
            printf(" 1. Interface sets VCC to state COC.\n");
            state->swicc.cont_state_rx |= SWICC_IO_CONT_VCC;
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 2:
            printf(" 2. Interface sets IO to state H (reception mode).\n");
            state->swicc.cont_state_rx |= SWICC_IO_CONT_IO;
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 3:
            printf(" 3. Interface enables CLK.\n");
            state->swicc.cont_state_rx |= SWICC_IO_CONT_CLK;
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 4:
            printf(" 4. Interface sets RST to state H to indicate to card that "
                   "it wants the ATR.\n");
            state->swicc.cont_state_rx |= SWICC_IO_CONT_RST;
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 5:
            printf(" 5. Card sends ATR to the interface.\n");
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 6:
            printf(" 6. Card receives a PPS message header (0xFF) which starts "
                   "a PPS exchange.\n");
            state->swicc.buf_rx[0U] = pps_req[0U];
            state->swicc.buf_rx_len = 1U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 7:
            printf(" 7. Card transitions to the PPS request state and will "
                   "request all bytes until (and including) PP0.\n");
            state->swicc.buf_rx_len = 0U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 8:
            printf(
                " 8. Card receives the second byte (PPS0) of the PPS message "
                "and now realizes how long the whole PPS message is.\n");
            state->swicc.buf_rx[0U] = pps_req[1U];
            state->swicc.buf_rx_len = 1U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        case 9:
            printf(" 9. Card receives the remaining bytes of the PPS message "
                   "and send back a PPS response.\n");
            memcpy(state->swicc.buf_rx, &pps_req[2U], pps_req_len - 2U);
            state->swicc.buf_rx_len = pps_req_len - 2U;
            state->swicc.buf_tx_len = buf_tx_size;
            break;
        }

        /* Push the data to swICC IO. */
        sim_rx_print(state);
        swicc_io(&state->swicc);
        sim_tx_print(state);
        swicc_state_print(state);
        swicc_fsm_state(&state->swicc, &state_fsm);

        /* Decide if transition was done exactly like expected. */
        bool state_change_invalid = true;
        switch (step)
        {
        case 0:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_OFF ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 1:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_ACTIVATION ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 2:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_ACTIVATION ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 3:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_RESET_COLD ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 4:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_ATR_REQ ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 5:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_ATR_RES ||
                                   state->swicc.buf_rx_len != 1U;
            break;
        case 6:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_PPS_REQ ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        case 7:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_PPS_REQ ||
                                   state->swicc.buf_rx_len != 1U;
            break;
        case 8: {
            state_change_invalid = state_fsm != SWICC_FSM_STATE_PPS_REQ ||
                                   state->swicc.buf_rx_len != pps_req_len - 2U;
            break;
        }
        case 9:
            state_change_invalid = state_fsm != SWICC_FSM_STATE_CMD_WAIT ||
                                   state->swicc.buf_rx_len != 0U;
            break;
        }

        /* If transition failed, quit early. */
        if (state_change_invalid)
        {
            printf("Step %u of mock init failed.\n", step);
            return -1;
        }

        /* Perform any extra tasks before the end of this step. */
        switch (step)
        {
        case 5:
            state->dbg_str_len = state->dbg_str_size;
            swicc_dbg_atr_str(state->dbg_str, &state->dbg_str_len, swicc_atr,
                              SWICC_ATR_LEN);
            printf("%.*s", state->dbg_str_len, state->dbg_str);
            if (!mock_pps)
            {
                return 0;
            }
            break;
        case 9:
            state->dbg_str_len = state->dbg_str_size;
            swicc_dbg_pps_str(state->dbg_str, &state->dbg_str_len,
                              state->swicc.buf_tx, state->swicc.buf_tx_len);
            printf("%.*s", state->dbg_str_len, state->dbg_str);
            return 0; /* Step 9 is the last step. */
        }
    }
}

int32_t sim_io(sim_st *const state)
{
#if SIM_PASSTHROUGH == 1
    scraw_raw_st tpdu = {
        .buf = state->swicc.buf_rx,
        .len = state->swicc.buf_rx_len,
    };
    scraw_res_st res = {
        .buf = state->swicc.buf_tx,
        .buf_len = state->swicc.buf_tx_len,
        .res_len = 0,
    };
    int32_t ret = scraw_send(&state->internal.scraw_ctx, &tpdu, &res);
    if (ret != 0)
    {
        printf("Pass-through to SIM failed: %i (%li)\n", ret,
               state->internal.scraw_ctx.err_reason);
        return -1;
    }
    if (res.res_len > UINT16_MAX)
    {
        /* This should never happen. */
        return -1;
    }
    state->swicc.buf_tx_len =
        (uint16_t)res.res_len; /* Safe cast due to bound check */
#else
    swicc_fsm_state_et state_fsm;
    sim_rx_print(state);
    swicc_io(&state->swicc);
    sim_tx_print(state);
    swicc_state_print(state);
    swicc_fsm_state(&state->swicc, &state_fsm);
    if (state_fsm == SWICC_FSM_STATE_OFF)
    {
        return -1;
    }
#endif
    return 0;
}

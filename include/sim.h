#pragma once

#include <scraw.h>
#include <stdint.h>
#include <swicc/swicc.h>

/* If the pass-through to a real SIM should be done for unknown commands. */
#define SIM_PASSTHROUGH 0U

/* Hold state of SIM. */
typedef struct sim_st
{
    struct internal_s
    {
        char *reader_selected;
        scraw_st scraw_ctx;
    } internal;
    swicc_st swicc;

    char *dbg_str;
    uint16_t dbg_str_len;
    uint16_t dbg_str_size;
} sim_st;

/**
 * @brief Initialize the state of the SIM (excluding the non-internal buffers
 * that have to be set before calling this function).
 * @param state This will be initialized.
 * @return 0 on success, -1 on failure.
 */
int32_t sim_init(sim_st *const state);

/**
 * @brief A mock init simulates activation, cold reset, and protocol and
 * parameters selection. This is useful if this setup is done elsewhere and only
 * transmission of TPDU messages is actually of interest.
 * @param state
 * @return 0 on success, -1 on failure.
 * @note This means some default values for Fi, Di, and f(max) are selected and
 * the active protocol becomes T=0.
 */
int32_t sim_init_mock(sim_st *const state);

/**
 * @brief Handles a raw TPDU message coming from the interface.
 * @param state All of the state that is not internal should be updated (to
 * newest values) by the caller before calling this function.
 * @return 0 on success, -1 on failure.
 */
int32_t sim_io(sim_st *const state);

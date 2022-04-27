#pragma once

#include <scraw.h>
#include <stdint.h>
#include <uicc.h>

/* Hold state of USIM. */
typedef struct usim_st
{
    struct internal_s
    {
        char *reader_selected;
        scraw_st scraw_ctx;
    } internal;
    uicc_st uicc;
} usim_st;

/**
 * @brief Initialize the state of the USIM.
 * @param state This will be initialized.
 * @return 0 on success, -1 on failure.
 */
int32_t usim_init(usim_st *const state);

/**
 * @brief A mock reset simulates activation, cold reset, and protocol and
 * parameters selection. This is useful if this setup is done elsewhere and only
 * transmission of TPDU messages is actually of interest.
 * @param state
 * @return 0 on success, -1 on failure.
 * @note This means some default values for Fi, Di, and fmax are selected and
 * the active protocol becomes T=0.
 */
int32_t usim_reset_mock(usim_st *const state);

/**
 * @brief Handles a raw TPDU message coming from the interface.
 * @param state All of the state that is not internal should be updated (to
 * newest values) by the caller before calling this function.
 * @return 0 on success, -1 on failure.
 */
int32_t usim_io(usim_st *const state);

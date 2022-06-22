#pragma once

#include <stdint.h>
#include <swicc/swicc.h>

/* Hold state of SIM. */
typedef struct sim_st
{
    swicc_st swicc;
} sim_st;

/**
 * @brief Initialize the state of the SIM (excluding the non-internal buffers
 * that have to be set before calling this function).
 * @param state This will be initialized.
 * @return 0 on success, -1 on failure.
 */
int32_t sim_init(sim_st *const state);

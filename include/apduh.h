#pragma once

#include "common.h"

/**
 * @brief APDU handler demux for the SIM.
 */
swicc_apduh_ft sim_apduh_demux;
swicc_ret_et sim_apduh_demux(swicc_st *const swicc_state,
                             swicc_apdu_cmd_st const *const cmd,
                             swicc_apdu_res_st *const res,
                             uint32_t const procedure_count);

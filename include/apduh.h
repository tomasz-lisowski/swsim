#pragma once

#include <uicc/uicc.h>

/**
 * @brief APDU handler demux for the SIM.
 * @param uicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 */
uicc_apduh_ft sim_apduh_demux;
uicc_ret_et sim_apduh_demux(uicc_st *const uicc_state,
                            uicc_apdu_cmd_st const *const cmd,
                            uicc_apdu_res_st *const res,
                            uint32_t const procedure_count);

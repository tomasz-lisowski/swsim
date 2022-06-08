#pragma once

#include <stdint.h>
#include <uicc/uicc.h>

/**
 * @brief Parse the raw CLA byte.
 * @param cla_raw The class byte of an APDU message, expected to be in the
 * proprietary classes defined in ETSI TS 102 221 V16.4.0 pg.76 sec.10.1.1
 * table.10.3.
 * @return Parsed CLA.
 */
uicc_apdu_cla_st sim_apdu_cmd_cla_parse(uint8_t const cla_raw);

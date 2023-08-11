#include "apdu.h"

swicc_apdu_cla_st sim_apdu_cmd_cla_parse(uint8_t const cla_raw)
{
    swicc_apdu_cla_st cla = {0U};
    if (cla_raw >> (8U - 4U) ==
            0b1010U /* 3GPP TS 51 011 V4.15.0 clause.9,
                       3GPP TS 31 101 V17.0.0 clause.11,
                       ETSI TS 102 221 V16.4.0 clause.10.1.1,
                       and GSM 11.11 4.21.1 clause.9.1. */
        || cla_raw >> (8U - 4U) ==
               0b1000U /* ETSI TS 102 221 V16.4.0 clause.10.1.1 */)
    {
        cla.raw = cla_raw;
        cla.type = SWICC_APDU_CLA_TYPE_PROPRIETARY;
        cla.ccc = SWICC_APDU_CLA_CCC_INVALID; /* Unsupported per ETSI TS 102 221
                                                V16.4.0 clause.10.1.1. */
        cla.sm = (cla_raw & 0b00001100) >> 2U;
        cla.lchan = cla_raw & 0b00000011;
    }
    else
    {
        cla.type = SWICC_APDU_CLA_TYPE_INVALID;
    }
    return cla;
}

#pragma once

#include "common.h"

typedef struct milenage_s
{
    /**
     * Per ETSI TS 135 206 V17.0.0 clause.5.1 it is recommended to compute OPc
     * off the USIM. We allow the OPc computation to be done in both of the
     * specified ways, i.e., on USIM (op_present = true) and off USIM
     * (op_present = false).
     */
    bool op_present;

    uint8_t op[16]; /* Operator Variant Algorithm Configuration Field */
    uint8_t op_c[16];

    uint8_t c1[16];
    uint8_t c2[16];
    uint8_t c3[16];
    uint8_t c4[16];
    uint8_t c5[16];
    uint8_t r1;
    uint8_t r2;
    uint8_t r3;
    uint8_t r4;
    uint8_t r5;

    uint8_t k[16];
} milenage_st;

swicc_ret_et milenage(milenage_st *const milenage, uint8_t const rand[const 16],
                      uint8_t const token_auth[const 16],
                      uint8_t output[const SWICC_DATA_MAX],
                      uint16_t *const output_len);

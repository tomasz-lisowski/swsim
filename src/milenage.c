#include "milenage.h"
#include "rijndael.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Perform XOR between a and b and store result in a.
 * @param[in] a
 * @param[in] b
 * @param[in] len Number of bytes to XOR.
 * @param[out] axorb Result of XOR'ing a and b.
 */
static void xorv(uint8_t const *const a, uint8_t const *const b,
                 uint32_t const len, uint8_t *const axorb)
{
    for (uint32_t i = 0; i < len; ++i)
    {
        axorb[i] = a[i] ^ b[i];
    }
}

/**
 * @brief Rotate a number n by r bits to the left with
 * wrap-around.
 * @param[in, out] n Number to rotate.
 * @param[in] r Number of bits to rotate by.
 */
static void rotl128(uint8_t n[const 16], uint32_t const r)
{
    uint32_t const rot = r % 128;
    uint32_t const rot_byte = rot / 8;
    uint32_t const rot_bit = rot % 8;

    if (rot != r)
    {
        fprintf(
            stderr,
            "Milenage: rotating by %u (which is more than 127 bits) is equal to %u%%128=%u.\n",
            r, r, r % 128);
    }

    uint8_t n_cpy[16];
    memcpy(n_cpy, n, sizeof(n_cpy));

    for (uint8_t i = 0; i < 16; ++i)
    {
        n[i] = n_cpy[(i + rot_byte) % 16];
        n[i] >>= rot_bit;

        uint8_t const bit_carry =
            (uint8_t)(n_cpy[(i + 1 + rot_byte) % 16] << (8 - rot_bit));
        n[i] |= bit_carry;
    }
}

/**
 * @brief Function to compute OPc from OP and K per ETSI TS 135 206 V17.0.0
 * annex.1.
 * @param[in, out] rijndael State of rijndael block cipher. It must be
 * initialized before calling this function.
 * @param[in] op OP that will be used to compute OPc.
 * @param[out] op_c Compute OPc will be written here.
 */
static void opc(rijndael_st *const rijndael, uint8_t const op[const 16],
                uint8_t op_c[const 16])
{
    rijndael_encrypt(rijndael, op, op_c);
    xorv((uint8_t const *const)op_c, (uint8_t const *const)op, 16, op_c);
}

/**
 * @brief Generic function to help implement f1, and f1* as defined per
 * ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] output = E[rot(E[RAND ^ OPc]K ^ OPc, r) ^ c]K ^ OPc.
 */
static void f11star_generic(milenage_st const *const milenage,
                            uint8_t const k[const 16],
                            uint8_t const rand[const 16],
                            uint8_t const sqn[const 6],
                            uint8_t const amf[const 2],
                            uint8_t output[const 16])
{
    uint8_t op_c[16];

    rijndael_st rijndael;
    rijndael_init(&rijndael, k);

    if (milenage->op_present)
    {
        fprintf(stderr, "Milenage: f1 uses usim-computed OPc.\n");
        opc(&rijndael, milenage->op, op_c);
    }
    else
    {
        fprintf(stderr, "Milenage: f1 uses pre-computed OPc.\n");
        for (uint8_t i = 0; i < 16; ++i)
        {
            op_c[i] = milenage->op_c[i];
        }
    }

    uint8_t enc0_in[16];
    xorv((uint8_t const *const)rand, op_c, 16, enc0_in);
    uint8_t enc0_out[16];
    rijndael_encrypt(&rijndael, enc0_in, enc0_out);

    uint8_t enc1_in[16];
    /* Create 128b integer = SQN || AMF || SQN || AMF. */
    for (uint8_t i = 0; i < 6; ++i)
    {
        enc1_in[i] = sqn[i];
        enc1_in[i + 8] = sqn[i];
    }
    for (uint8_t i = 0; i < 2; ++i)
    {
        enc1_in[i + 6] = amf[i];
        enc1_in[i + 14] = amf[i];
    }

    /* (SQN || AMF || SQN || AMF) ^ OPc */
    xorv(enc1_in, op_c, 16, enc1_in);

    /* rot((SQN || AMF || SQN || AMF) ^ OPc, r1) */
    rotl128(enc1_in, milenage->r1);

    /* rot((SQN || AMF || SQN || AMF) ^ OPc, r1) ^ c1 */
    xorv(enc1_in, milenage->c1, 16, enc1_in);

    /* E[RAND ^ OPc]K ^ (rot((SQN || AMF || SQN || AMF) ^ OPc, r1) ^ c1) */
    xorv(enc1_in, enc0_out, 16, enc1_in);

    uint8_t enc1_out[16];

    /* E[E[RAND ^ OPc]K ^ (rot((SQN || AMF || SQN || AMF) ^ OPc, r1) ^ c1)]K */
    rijndael_encrypt(&rijndael, enc1_in, enc1_out);

    /**
     * At this point: enc1_out = OUT1.
     * With OUT1 defined in ETSI TS 135 206 V17.0.0.
     */

    xorv(enc1_out, op_c, 16, enc1_out);

    for (uint8_t i = 0; i < 16; ++i)
    {
        output[i] = enc1_out[i];
    }
}

/**
 * @brief Function f1 as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage state and parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[in] sqn Sequence number.
 * @param[in] amf Authentication management field.
 * @param[out] mac_a Network authnetication code.
 */
static void f1(milenage_st const *const milenage, uint8_t const k[const 16],
               uint8_t const rand[const 16], uint8_t const sqn[const 6],
               uint8_t const amf[const 2], uint8_t mac_a[const 8])
{
    uint8_t output[16];
    f11star_generic(milenage, k, rand, sqn, amf, output);
    for (uint8_t i = 0; i < 8; ++i)
    {
        mac_a[i] = output[i];
    }
}

/**
 * @brief Function f1* as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[in] sqn Sequence number.
 * @param[in] amf Authentication management field.
 * @param[out] mac_s Resynch authentication code.
 */
__attribute__((unused)) static void f1star(milenage_st const *const milenage,
                                           uint8_t const k[const 16],
                                           uint8_t const rand[const 16],
                                           uint8_t const sqn[const 6],
                                           uint8_t const amf[const 2],
                                           uint8_t mac_s[const 8])
{
    uint8_t output[16];
    f11star_generic(milenage, k, rand, sqn, amf, output);
    for (uint8_t i = 0; i < 8; ++i)
    {
        mac_s[i] = output[8 + i];
    }
}

/**
 * @brief Generic function to help implement f2, f3, f4, and f5 as defined per
 * ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] output = E[rot(E[RAND ^ OPc]K ^ OPc, r) ^ c]K ^ OPc.
 */
static void f2345_generic(milenage_st const *const milenage,
                          uint8_t const k[const 16],
                          uint8_t const rand[const 16],
                          uint8_t output[const 16], uint8_t const c[const 16],
                          uint8_t const r)
{
    rijndael_st rijndael;
    rijndael_init(&rijndael, k);

    uint8_t op_c[16];
    if (milenage->op_present)
    {
        fprintf(stderr, "Milenage: f2345 uses usim-computed OPc.\n");
        opc(&rijndael, milenage->op, op_c);
    }
    else
    {
        fprintf(stderr, "Milenage: f2345 uses pre-computed OPc.\n");
        for (uint8_t i = 0; i < 16; ++i)
        {
            op_c[i] = milenage->op_c[i];
        }
    }

    uint8_t enc0_in[16];
    /* RAND ^ OPc */
    xorv(rand, op_c, 16, enc0_in);

    uint8_t enc0_out[16];
    /* E[RAND ^ OPc]K */
    rijndael_encrypt(&rijndael, enc0_in, enc0_out);

    uint8_t enc1_in[16];
    /* E[RAND ^ OPc]K ^ OPc */
    xorv(enc0_out, op_c, 16, enc1_in);

    /* rot(E[RAND ^ OPc]K ^ OPc, r) */
    rotl128(enc1_in, r);

    /* rot(E[RAND ^ OPc]K ^ OPc, r) ^ c */
    xorv(enc1_in, c, 16, enc1_in);

    uint8_t enc1_out[16];
    /* E[rot(E[RAND ^ OPc]K ^ OPc, r) ^ c]K */
    rijndael_encrypt(&rijndael, enc1_in, enc1_out);

    /* E[rot(E[RAND ^ OPc]K ^ OPc, r) ^ c]K ^ OPc */
    xorv(enc1_out, op_c, 16, enc1_out);

    /* output = E[rot(E[RAND ^ OPc]K ^ OPc, r) ^ c]K ^ OPc */
    for (uint8_t i = 0; i < 16; ++i)
    {
        output[i] = enc1_out[i];
    }
}

/**
 * @brief Function f2 as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] res Response.
 */
static void f2(milenage_st const *const milenage, uint8_t const k[const 16],
               uint8_t const rand[const 16], uint8_t res[const 8])
{
    uint8_t output[16];
    f2345_generic(milenage, k, rand, output, milenage->c2, milenage->r2);
    for (uint8_t i = 0; i < 8; ++i)
    {
        res[i] = output[8 + i];
    }
}

/**
 * @brief Function f3 as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] ck Confidentiality key.
 */
static void f3(milenage_st const *const milenage, uint8_t const k[const 16],
               uint8_t const rand[const 16], uint8_t ck[const 16])
{
    f2345_generic(milenage, k, rand, ck, milenage->c3, milenage->r3);
}

/**
 * @brief Function f4 as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] ik Integrity key.
 */
static void f4(milenage_st const *const milenage, uint8_t const k[const 16],
               uint8_t const rand[const 16], uint8_t ik[const 16])
{
    f2345_generic(milenage, k, rand, ik, milenage->c4, milenage->r4);
}

/**
 * @brief Function f5 as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] ak Anonimity key.
 */
static void f5(milenage_st const *const milenage, uint8_t const k[const 16],
               uint8_t const rand[const 16], uint8_t ak[const 6])
{
    uint8_t output[16];
    f2345_generic(milenage, k, rand, output, milenage->c2, milenage->r2);
    for (uint8_t i = 0; i < 6; ++i)
    {
        ak[i] = output[i];
    }
}

/**
 * @brief Function f5* as defined in ETSI TS 135 206 V17.0.0.
 * @param[in] milenage Milenage parameters.
 * @param[in] k Subscriber key.
 * @param[in] rand Random challenge.
 * @param[out] ak Resynch anonimity key.
 */
__attribute__((unused)) static void f5star(milenage_st const *const milenage,
                                           uint8_t const k[const 16],
                                           uint8_t const rand[const 16],
                                           uint8_t ak[const 6])
{
    uint8_t output[16];
    f2345_generic(milenage, k, rand, output, milenage->c5, milenage->r5);
    for (uint8_t i = 0; i < 6; ++i)
    {
        ak[i] = output[i];
    }
}

swicc_ret_et milenage(milenage_st *const milenage, uint8_t const rand[const 16],
                      uint8_t const autn[const 16],
                      uint8_t output[const SWICC_DATA_MAX],
                      uint16_t *const output_len)
{
    /**
     * Per ETSI TS 135 206 V17.0.0 clause.5.3, pairs (ci,ri) must all be
     * different. This is a requirement. It is also recommended that c1 has even
     * parity, and c2-c5 all have odd parity.
     */
    {
        uint8_t c[5][4][4];
        memcpy(c[0], milenage->c1, sizeof(c[0]));
        memcpy(c[1], milenage->c2, sizeof(c[1]));
        memcpy(c[2], milenage->c3, sizeof(c[2]));
        memcpy(c[3], milenage->c4, sizeof(c[3]));
        memcpy(c[4], milenage->c5, sizeof(c[4]));

        uint8_t const r[5] = {
            milenage->r1, milenage->r2, milenage->r3,
            milenage->r4, milenage->r5,
        };

        for (uint8_t i = 0; i < 5; ++i)
        {
            for (uint8_t j = 0; j < 5; ++j)
            {
                if (i != j && r[i] == r[j] &&
                    memcmp(c[i], c[j], sizeof(c[0])) == 0)
                {
                    fprintf(
                        stderr,
                        "Per ETSI TS 135 206 V17.0.0 clause.5.3, pairs (ci,ri) must all be different. Current milenage parameters break this requirement with pair: (c%u, r%u) == (c%u, r%u) == (%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X, %u).\n",
                        i, i, j, j, c[i][0][0], c[i][0][1], c[i][0][2],
                        c[i][0][3], c[i][1][0], c[i][1][1], c[i][1][2],
                        c[i][1][3], c[i][2][0], c[i][2][1], c[i][2][2],
                        c[i][2][3], c[i][3][0], c[i][3][1], c[i][3][2],
                        c[i][3][3], r[i]);
                }
            }
        }

        uint8_t one_count[5] = {0, 0, 0, 0, 0};
        for (uint8_t i = 0; i < 5; ++i)
        {
            for (uint8_t ja = 0; ja < 4; ++ja)
            {
                for (uint8_t jb = 0; jb < 4; ++jb)
                {
                    uint8_t v = c[i][ja][jb];
                    for (uint8_t k = 0; k < 8; ++k)
                    {
                        one_count[i] += v & 0x01;
                        v >>= 1;
                    }
                }
            }
        }
        if (one_count[0] % 2 != 0)
        {
            fprintf(
                stderr,
                "Per ETSI TS 135 206 V17.0.0 clause.5.3, it is recomendded that c1 have even parity. Current milenage parameters break this recommendation since c1 has %u ones therefore it is odd.\n",
                one_count[0]);
        }
        for (uint8_t i = 1; i < 5; ++i)
        {
            if (one_count[i] % 2 == 0)
            {
                fprintf(
                    stderr,
                    "Per ETSI TS 135 206 V17.0.0 clause.5.3, it is recomendded that c%u have odd parity. Current milenage parameters break this recommendation since c%u has %u ones therefore it is even.\n",
                    i, i, one_count[i]);
            }
        }
    }

    fprintf(stderr, "Milenage: RAND=");
    for (uint8_t i = 0; i < 16; ++i)
    {
        fprintf(stderr, "%02X", rand[i]);
    }
    fprintf(stderr, ".\n");
    fprintf(stderr, "Milenage: AUTN=");
    for (uint8_t i = 0; i < 16; ++i)
    {
        if (i == 6 || i == 8)
        {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "%02X", autn[i]);
    }
    fprintf(stderr, ".\n");

    uint8_t sqn_xor_ak[6];
    memcpy(sqn_xor_ak, autn, sizeof(sqn_xor_ak));

    uint8_t ak[6];
    f5(milenage, milenage->k, rand, ak);

    /**
     * TODO: Verify the sequence number per ETSI TS 133 102 V14.1.0
     * clause.6.3.3.
     */
    uint8_t sqn[6];
    xorv(ak, sqn_xor_ak, sizeof(ak), sqn);

    fprintf(stderr, "Milenage: SQN=%02X%02X%02X%02X%02X%02X.\n", sqn[0], sqn[1],
            sqn[2], sqn[3], sqn[4], sqn[5]);

    uint8_t amf[2];
    memcpy(amf, &autn[6], sizeof(amf));

    fprintf(stderr, "Milenage: AMF=%02X%02X.\n", amf[0], amf[1]);

    uint8_t xmac_a[8];
    f1(milenage, milenage->k, rand, sqn, amf, xmac_a);
    uint8_t mac_a[8];
    memcpy(mac_a, &autn[8], sizeof(mac_a));

    fprintf(stderr, "Mileage: XMACa=");
    for (uint8_t i = 0; i < 8; ++i)
    {
        fprintf(stderr, "%02X", xmac_a[i]);
    }
    fprintf(stderr, ".\n");
    fprintf(stderr, "Milenage: MACa=");
    for (uint8_t i = 0; i < 8; ++i)
    {
        fprintf(stderr, "%02X", mac_a[i]);
    }
    fprintf(stderr, ".\n");

    /**
     * Response is per 3GPP TS 31.102 V17.5.0 clause.7.1.2.1 and clause.6.3.3.
     */
    if (memcmp(xmac_a, mac_a, sizeof(xmac_a)) == 0)
    {
        uint8_t res[8];
        f2(milenage, milenage->k, rand, res);

        uint8_t ck[16];
        f3(milenage, milenage->k, rand, ck);

        uint8_t ik[16];
        f4(milenage, milenage->k, rand, ik);

        /**
         * GSM cipher key for UMTS-GSM interoperability purposes.
         * The process of generating it, is called the "C3 conversion".
         */
        uint8_t kc[8];
        uint8_t kc_tmp0[8];
        uint8_t kc_tmp1[8];
        xorv(ck, &ik[8], 8, kc_tmp0);
        xorv(&ck[8], ik, 8, kc_tmp1);
        xorv(kc_tmp0, kc_tmp1, 8, kc);

        uint8_t i = 0;
        output[i++] = 0xDB; /* "Successful 3G authentication" tag per 3GPP
                               TS 31.102 V17.5.0 clause.7.1.2.1. */
        output[i++] = sizeof(res);
        memcpy(&output[i], res, sizeof(res));
        i += sizeof(res);

        output[i++] = sizeof(ck);
        memcpy(&output[i], ck, sizeof(ck));
        i += sizeof(ck);

        output[i++] = sizeof(ik);
        memcpy(&output[i], ik, sizeof(ik));
        i += sizeof(ik);

        output[i++] = sizeof(kc);
        memcpy(&output[i], kc, sizeof(kc));
        i += sizeof(kc);

        *output_len = i;
        fprintf(stderr, "Mileage: authenticated.\n");
        fprintf(stderr, "Mileage: response=");
        for (uint8_t j = 0; j < i; ++j)
        {
            fprintf(stderr, "%02X", output[j]);
        }
        fprintf(stderr, ".\n");

        return SWICC_RET_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Mileage: failed to validate MAC from network xmac_a=");
        for (uint8_t i = 0; i < 8; ++i)
        {
            fprintf(stderr, "%02X", xmac_a[i]);
        }
        *output_len = 0;
        return SWICC_RET_ERROR;
    }
}

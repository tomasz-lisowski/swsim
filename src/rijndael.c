#include "rijndael.h"

/* Rijndael S box table. */
static uint8_t const sbox[256] = {
    99,  124, 119, 123, 242, 107, 111, 197, 48,  1,   103, 43,  254, 215, 171,
    118, 202, 130, 201, 125, 250, 89,  71,  240, 173, 212, 162, 175, 156, 164,
    114, 192, 183, 253, 147, 38,  54,  63,  247, 204, 52,  165, 229, 241, 113,
    216, 49,  21,  4,   199, 35,  195, 24,  150, 5,   154, 7,   18,  128, 226,
    235, 39,  178, 117, 9,   131, 44,  26,  27,  110, 90,  160, 82,  59,  214,
    179, 41,  227, 47,  132, 83,  209, 0,   237, 32,  252, 177, 91,  106, 203,
    190, 57,  74,  76,  88,  207, 208, 239, 170, 251, 67,  77,  51,  133, 69,
    249, 2,   127, 80,  60,  159, 168, 81,  163, 64,  143, 146, 157, 56,  245,
    188, 182, 218, 33,  16,  255, 243, 210, 205, 12,  19,  236, 95,  151, 68,
    23,  196, 167, 126, 61,  100, 93,  25,  115, 96,  129, 79,  220, 34,  42,
    144, 136, 70,  238, 184, 20,  222, 94,  11,  219, 224, 50,  58,  10,  73,
    6,   36,  92,  194, 211, 172, 98,  145, 149, 228, 121, 231, 200, 55,  109,
    141, 213, 78,  169, 108, 86,  244, 234, 101, 122, 174, 8,   186, 120, 37,
    46,  28,  166, 180, 198, 232, 221, 116, 31,  75,  189, 139, 138, 112, 62,
    181, 102, 72,  3,   246, 14,  97,  53,  87,  185, 134, 193, 29,  158, 225,
    248, 152, 17,  105, 217, 142, 148, 155, 30,  135, 233, 206, 85,  40,  223,
    140, 161, 137, 13,  191, 230, 66,  104, 65,  153, 45,  15,  176, 84,  187,
    22,
};

/* This array does the multiplication by x in GF(2^8). */
static uint8_t const xtime[256] = {
    0,   2,   4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,  26,  28,
    30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,
    60,  62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
    90,  92,  94,  96,  98,  100, 102, 104, 106, 108, 110, 112, 114, 116, 118,
    120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142, 144, 146, 148,
    150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178,
    180, 182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 208,
    210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 238,
    240, 242, 244, 246, 248, 250, 252, 254, 27,  25,  31,  29,  19,  17,  23,
    21,  11,  9,   15,  13,  3,   1,   7,   5,   59,  57,  63,  61,  51,  49,
    55,  53,  43,  41,  47,  45,  35,  33,  39,  37,  91,  89,  95,  93,  83,
    81,  87,  85,  75,  73,  79,  77,  67,  65,  71,  69,  123, 121, 127, 125,
    115, 113, 119, 117, 107, 105, 111, 109, 99,  97,  103, 101, 155, 153, 159,
    157, 147, 145, 151, 149, 139, 137, 143, 141, 131, 129, 135, 133, 187, 185,
    191, 189, 179, 177, 183, 181, 171, 169, 175, 173, 163, 161, 167, 165, 219,
    217, 223, 221, 211, 209, 215, 213, 203, 201, 207, 205, 195, 193, 199, 197,
    251, 249, 255, 253, 243, 241, 247, 245, 235, 233, 239, 237, 227, 225, 231,
    229,
};

/**
 * @brief Rijndael key schedule function. Takes 16-byte key and creates all
 * Rijndael's internal subkeys ready for encryption.
 * @param[in, out] rijndael State of the rijndael block cipher.
 * @param[in] key Key that will be expanded into the multiple round subkeys.
 */
static void rijndael_key_schedule(rijndael_st *const rijndael,
                                  uint8_t const key[const 16])
{
    uint8_t round_const;

    /* First round key equals key. */
    for (uint8_t i = 0; i < 16; i++)
    {
        rijndael->round_keys[0][i & 0x03][i >> 2] = key[i];
    }
    round_const = 1;

    /* Now calculate round keys. */
    for (uint8_t i = 1; i < 11; i++)
    {
        rijndael->round_keys[i][0][0] =
            sbox[rijndael->round_keys[i - 1][1][3]] ^
            rijndael->round_keys[i - 1][0][0] ^ round_const;
        rijndael->round_keys[i][1][0] =
            sbox[rijndael->round_keys[i - 1][2][3]] ^
            rijndael->round_keys[i - 1][1][0];
        rijndael->round_keys[i][2][0] =
            sbox[rijndael->round_keys[i - 1][3][3]] ^
            rijndael->round_keys[i - 1][2][0];
        rijndael->round_keys[i][3][0] =
            sbox[rijndael->round_keys[i - 1][0][3]] ^
            rijndael->round_keys[i - 1][3][0];
        for (uint8_t j = 0; j < 4; ++j)
        {
            rijndael->round_keys[i][j][1] = rijndael->round_keys[i - 1][j][1] ^
                                            rijndael->round_keys[i][j][0];
            rijndael->round_keys[i][j][2] = rijndael->round_keys[i - 1][j][2] ^
                                            rijndael->round_keys[i][j][1];
            rijndael->round_keys[i][j][3] = rijndael->round_keys[i - 1][j][3] ^
                                            rijndael->round_keys[i][j][2];
        }

        /* Update round constant. */
        round_const = xtime[round_const];
    }
}

/**
 * @brief Round key addition function.
 * @param[in, out] state Block to transform.
 * @param[in] round_keys Key schedule to use.
 * @param[in] round Round index.
 */
static void key_add(uint8_t state[const 4][4],
                    uint8_t const round_keys[const 11][4][4],
                    uint32_t const round)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            state[i][j] ^= round_keys[round][i][j];
        }
    }
}

/**
 * @brief Byte substitution transformation.
 * @param[in, out] state Block to transform.
 */
static void byte_sub(uint8_t state[const 4][4])
{
    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            state[i][j] = sbox[state[i][j]];
        }
    }
}

/**
 * @brief Row shift transformation.
 * @param[in, out] state Block to transform.
 */
static void row_shift(uint8_t state[const 4][4])
{
    uint8_t temp;

    /* Left rotate row 1 by 1. */
    temp = state[1][0];
    state[1][0] = state[1][1];
    state[1][1] = state[1][2];
    state[1][2] = state[1][3];
    state[1][3] = temp;

    /* Left rotate row 2 by 2. */
    temp = state[2][0];
    state[2][0] = state[2][2];
    state[2][2] = temp;
    temp = state[2][1];
    state[2][1] = state[2][3];
    state[2][3] = temp;

    /* Left rotate row 3 by 3. */
    temp = state[3][0];
    state[3][0] = state[3][3];
    state[3][3] = state[3][2];
    state[3][2] = state[3][1];
    state[3][1] = temp;
}

/**
 * @brief Mix column transformation.
 * @param[in, out] state Block to transform.
 */
static void column_mix(uint8_t state[const 4][4])
{
    uint8_t temp, tmp, tmp0;

    /* Transform one column at a time. */
    for (uint8_t i = 0; i < 4; i++)
    {
        temp = state[0][i] ^ state[1][i] ^ state[2][i] ^ state[3][i];
        tmp0 = state[0][i];
        tmp = xtime[state[0][i] ^ state[1][i]];
        state[0][i] ^= temp ^ tmp;
        tmp = xtime[state[1][i] ^ state[2][i]];
        state[1][i] ^= temp ^ tmp;
        tmp = xtime[state[2][i] ^ state[3][i]];
        state[2][i] ^= temp ^ tmp;
        tmp = xtime[state[3][i] ^ tmp0];
        state[3][i] ^= temp ^ tmp;
    }
}

void rijndael_encrypt(rijndael_st *const rijndael,
                      uint8_t const input[const 16], uint8_t output[const 16])
{
    uint8_t state[4][4];

    /* Initialize state array from input byte string. */
    for (uint8_t i = 0; i < 16; i++)
    {
        state[i & 0x3][i >> 2] = input[i];
    }

    /* Add first round_key. */
    key_add(state, rijndael->round_keys, 0);

    /* Perform all full rounds per ETSI TS 135 206 V17.0.0 clause.a2.3. */
    uint8_t r;
    for (r = 1; r <= 10; r++)
    {
        byte_sub(state);
        row_shift(state);
        if (r <= 9)
        {
            column_mix(state);
        }
        key_add(state, rijndael->round_keys, r);
    }

    /* Produce output byte string from state array. */
    for (uint8_t i = 0; i < 16; i++)
    {
        output[i] = state[i & 0x03][i >> 2];
    }

    return;
}

void rijndael_init(rijndael_st *const rijndael, uint8_t const key[const 16])
{
    rijndael_key_schedule(rijndael, key);
}

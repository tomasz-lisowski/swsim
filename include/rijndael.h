#pragma once

#include "common.h"

typedef struct rijndael_s
{
    /* Rijndael round subkeys. */
    uint8_t round_keys[11][4][4];
} rijndael_st;

/**
 * @brief Rijndael encryption function. Takes 16-byte input and creates 16-byte
 * output using round keys already derived from 16-byte key.
 * @param[in, out] rijndael State of the rijndael block cipher.
 * @param[in] input Input.
 * @param[out] output Output.
 */
void rijndael_encrypt(rijndael_st *const rijndael,
                      uint8_t const input[const 16], uint8_t output[const 16]);

/**
 * @brief Initialize the rijndael cipher state.
 * @param[out] rijndael State of the rijndael block cipher that will be
 * initialized.
 * @param[in] key Key that will be used for all following encryption options.
 */
void rijndael_init(rijndael_st *const rijndael, uint8_t const key[const 16]);

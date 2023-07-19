#pragma once

#include "common.h"

/**
 * @brief Create a response to SELECT for a given file.
 * @param[in] fs The swICC file system containing the file.
 * @param[in] tree Tree containing the file.
 * @param[in] file The file to create response for.
 * @param[out] buf_res Where the response will be written.
 * @param[in, out] buf_res_len This shall contain the size of the response
 * buffer. It will receive the size of the response that was writen to the
 * buffer.
 * @return 0 on success, -1 on failure.
 */
int32_t gsm_select_res(swicc_fs_st const *const fs,
                       swicc_disk_tree_st *const tree,
                       swicc_fs_file_st *const file, uint8_t *const buf_res,
                       uint16_t *const buf_res_len);

/**
 * @brief Run the GSM algorithm.
 * @param[in] ki Individual subscriber authentication key.
 * @param[in] rand Challegne from base station.
 * @param[out] res Response of the GSM algorithm.
 * An implementation of the GSM A3A8 algorithm (COMP128).
 * @note This algorithm has been reversed and implemented by: Marc Briceno,
 * Ian Goldberg, and David Wagner.
 */
void gsm_algo(uint8_t const ki[const 16], uint8_t const rand[const 16],
              uint8_t res[const 12]);

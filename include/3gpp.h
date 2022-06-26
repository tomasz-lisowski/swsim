#pragma once
#include <stdint.h>
#include <swicc/swicc.h>

/**
 * @brief Create a response to SELECT for a given file.
 * @param fs The swICC file system containing the file.
 * @param tree Tree containing the file.
 * @param file The file to create response for.
 * @param buf_res Where the response will be written.
 * @param buf_res_len This shall contain the size of the response buffer. It
 * will receive the size of the response that was writen to the buffer.
 * @return 0 on success, -1 on failure.
 */
int32_t o3gpp_select_res(swicc_fs_st const *const fs,
                         swicc_disk_tree_st *const tree,
                         swicc_fs_file_st *const file, uint8_t *const buf_res,
                         uint16_t *const buf_res_len);

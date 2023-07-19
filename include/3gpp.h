#pragma once

#include "common.h"
#include <stdint.h>

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
int32_t o3gpp_select_res(swicc_fs_st const *const fs,
                         swicc_disk_tree_st const *const tree,
                         swicc_fs_file_st const *const file,
                         uint8_t *const buf_res, uint16_t *const buf_res_len);

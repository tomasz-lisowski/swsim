#pragma once
#include <stdint.h>
#include <swicc/swicc.h>

/**
 * @brief Find the number of DF and EF files present inside of a provided file.
 * @param[in] tree Tree that contains the file that will be searched.
 * @param[in] file File to count DFs and EFs in.
 * @param[in] recurse If the count should include files nested inside of DFs
 * contained in the provided file.
 * @param[out] df_count Where the count of DFs will be written.
 * @param[out] ef_count Where the count of EFs will be written.
 * @return 0 on success, -1 on failure.
 */
int32_t sim_fs_file_child_count(swicc_disk_tree_st *const tree,
                                swicc_fs_file_st *const file,
                                bool const recurse, uint32_t *const df_count,
                                uint32_t *const ef_count);

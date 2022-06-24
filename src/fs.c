#include "fs.h"

typedef struct fs_file_count_userdata_s
{
    uint32_t ef_count;
    uint32_t df_count;
} fs_file_count_userdata_st;

static fs_file_foreach_cb fs_file_child_count_cb;
static swicc_ret_et fs_file_child_count_cb(__attribute__((unused))
                                           swicc_disk_tree_st *const tree,
                                           swicc_fs_file_st *const file,
                                           void *const userdata)
{
    fs_file_count_userdata_st *const ud = userdata;
    switch (file->hdr_item.type)
    {
    case SWICC_FS_ITEM_TYPE_FILE_MF:
    case SWICC_FS_ITEM_TYPE_FILE_ADF:
    case SWICC_FS_ITEM_TYPE_FILE_DF:
        ud->df_count += 1U;
        break;
    case SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT:
    case SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED:
    case SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC:
        ud->ef_count += 1U;
        break;
    default:
        break;
    }
    return SWICC_RET_SUCCESS;
}

int32_t sim_fs_file_child_count(swicc_disk_tree_st *const tree,
                                swicc_fs_file_st *const file,
                                bool const recurse, uint32_t *const df_count,
                                uint32_t *const ef_count)
{
    fs_file_count_userdata_st userdata = {
        .ef_count = 0U,
        .df_count = 0U,
    };
    swicc_ret_et const ret_count = swicc_disk_file_foreach(
        tree, file, fs_file_child_count_cb, &userdata, recurse);
    if (ret_count == SWICC_RET_SUCCESS)
    {
        *ef_count = userdata.ef_count;
        *df_count = userdata.df_count;
        return 0;
    }
    return -1;
}

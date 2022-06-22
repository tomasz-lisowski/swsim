#include "sim.h"
#include "apduh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t sim_init(sim_st *const state, char const *const path_json,
                 char const *const path_swicc)
{
    swicc_disk_st disk = {0};
    swicc_ret_et const ret_disk = swicc_diskjs_disk_create(&disk, path_json);
    if (ret_disk == SWICC_RET_SUCCESS)
    {
        if (path_swicc == NULL ||
            swicc_disk_save(&disk, path_swicc) == SWICC_RET_SUCCESS)
        {
            if (swicc_fs_disk_mount(&state->swicc, &disk) == SWICC_RET_SUCCESS)
            {
                if (swicc_apduh_pro_register(&state->swicc, sim_apduh_demux) ==
                    SWICC_RET_SUCCESS)
                {
                    return 0;
                }
                else
                {
                    printf("Failed to register a proprietary APDU handler.\n");
                }
            }
            else
            {
                printf("Failed to mount disk.\n");
            }
        }
        else
        {
            printf("Failed to save disk.\n");
        }
        swicc_disk_unload(&disk);
    }
    else
    {
        printf("Failed to create disk: %s.\n", swicc_dbg_ret_str(ret_disk));
    }
    return -1;
}

#include "swsim.h"
#include "apduh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t swsim_init(swsim_st *const swsim_state, swicc_st *const swicc_state,
                   char const *const path_json, char const *const path_swicc)
{
    memset(swsim_state, 0U, sizeof(*swsim_state));
    memset(swicc_state, 0U, sizeof(*swicc_state));
    swicc_state->userdata = swsim_state;

    swicc_disk_st disk = {0};
    swicc_ret_et ret_disk = SWICC_RET_ERROR;
    if (path_json != NULL)
    {
        ret_disk = swicc_diskjs_disk_create(&disk, path_json);
    }
    else
    {
        ret_disk = swicc_disk_load(&disk, path_swicc);
    }

    if (ret_disk == SWICC_RET_SUCCESS)
    {
        if (path_swicc == NULL ||
            swicc_disk_save(&disk, path_swicc) == SWICC_RET_SUCCESS)
        {
            if (swicc_fs_disk_mount(swicc_state, &disk) == SWICC_RET_SUCCESS)
            {
                if (swicc_apduh_pro_register(swicc_state, sim_apduh_demux) ==
                    SWICC_RET_SUCCESS)
                {
                    sim_proactive_init(swicc_state->userdata);
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
        printf("Failed to load/generate disk: %s.\n",
               swicc_dbg_ret_str(ret_disk));
    }
    return -1;
}

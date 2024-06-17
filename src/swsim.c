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

    {
        milenage_st milenage_init = {
            .op_present = false,
            .op_c = {0xA6, 0x4A, 0x50, 0x7A, 0xE1, 0xA2, 0xA9, 0x8B, 0xB8, 0x8E,
                     0xB4, 0x21, 0x01, 0x35, 0xDC, 0x87},

            /**
             * These ci and ri values are recommended defaults per ETSI TS 135
             * 206 V17.0.0.
             */
            .c1 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
            .c2 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
            .c3 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
            .c4 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x04},
            .c5 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x08},
            .r1 = 64,
            .r2 = 0,
            .r3 = 32,
            .r4 = 64,
            .r5 = 96,

            .k = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07},
        };
        memcpy(&swsim_state->milenage, &milenage_init, sizeof(milenage_init));
    }

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
                    proactive_init(swicc_state->userdata);
                    return 0;
                }
                else
                {
                    fprintf(stderr,
                            "Failed to register a proprietary APDU handler.\n");
                }
            }
            else
            {
                fprintf(stderr, "Failed to mount disk.\n");
            }
        }
        else
        {
            fprintf(stderr, "Failed to save disk.\n");
        }
        swicc_disk_unload(&disk);
    }
    else
    {
        fprintf(stderr, "Failed to load/generate disk: %s.\n",
                swicc_dbg_ret_str(ret_disk));
    }
    return -1;
}

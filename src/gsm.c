#include "gsm.h"
#include "fs.h"
#include <netinet/in.h>
#include <string.h>

int32_t gsm_select_res(swicc_fs_st const *const fs,
                       swicc_disk_tree_st *const tree,
                       swicc_fs_file_st *const file, uint8_t *const buf_res,
                       uint16_t *const buf_res_len)
{
    switch (file->hdr_item.type)
    {
    case SWICC_FS_ITEM_TYPE_FILE_MF:
    case SWICC_FS_ITEM_TYPE_FILE_ADF:
    /**
     * @brief Not sure if this operation can select applications.
     */
    case SWICC_FS_ITEM_TYPE_FILE_DF: {
        /* Response parameters data. */
        uint32_t const mem_free = UINT32_MAX - fs->va.cur_tree->len;
        /**
         * "Total amount of memory of the selected directory which is not
         * allocated to any of the DFs or EFs under the selected directory."
         * Safe cast due to check in same statement.
         */
        uint16_t const mem_free_short =
            htobe16(mem_free > UINT16_MAX ? UINT16_MAX : (uint16_t)mem_free);
        /* "File ID." */
        uint16_t const file_id = htobe16(file->hdr_file.id);
        /* "Type of file." */
        uint8_t file_type;
        switch (file->hdr_item.type)
        {
        case SWICC_FS_ITEM_TYPE_FILE_MF:
            file_type = 0x01;
            break;
        case SWICC_FS_ITEM_TYPE_FILE_ADF:
        case SWICC_FS_ITEM_TYPE_FILE_DF:
            file_type = 0x02;
            break;
        default:
            __builtin_unreachable();
        }
        /* Length of the GSM specific data. */
        uint8_t const gsm_data_len =
            10U; /* Everything except the last portion which is optional. */

        /* GSM specific data. */
        /* "File characteristics." */
        /**
         * LSB>MSB
         *    1b Clock stop = 1 (clock stop allowed)
         *  + 1b Authentication algorithm clock frequency = 1 (13/4 MHz)
         *  + 1b Clock stop = 0 (high not preferred)
         *  + 1b Clock stop = 0 (low not preferred)
         *  + 1b 0 (from GSM 11.12 @todo Look into GSM 11.12)
         *  + 2b RFU = 00
         *  + 1b CHV1 = 1 = (disabled)
         *
         * @note Clock stop is allowed and has no preference on level.
         */
        uint8_t const file_characteristic = 0b10000011;
        uint32_t df_child_count_tmp;
        uint32_t ef_child_count_tmp;
        int32_t const ret_count = sim_fs_file_child_count(
            tree, file, false, &df_child_count_tmp, &ef_child_count_tmp);
        if (ret_count != 0 || df_child_count_tmp > UINT8_MAX ||
            ef_child_count_tmp > UINT8_MAX)
        {
            /**
             * @todo NV modified but can't indicate this, not sure what
             * to do here.
             */
            return -1;
        }
        /**
         * "Number of DFs which are a direct child of the current
         * directory."
         * Safe cast due to check in 'if' before.
         */
        uint8_t const df_child_count = (uint8_t)df_child_count_tmp;
        /**
         * "Number of EFs which are a direct child of the current
         * directory."
         * Safe cast due to check in 'if' before.
         */
        uint8_t const ef_child_count = (uint8_t)ef_child_count_tmp;
        /**
         * "Number of CHVs, UNBLOCK CHVs and administrative codes."
         */
        /**
         * Status of a secret code:
         * LSB>MSB
         *   4b Number of false presentations remaining = 0
         * + 3b RFU = 0
         * + 1b Secret code initialized = 1 (initialized)
         */
        uint8_t const code_count = 4U; /* 4 CHVs (PIN1 PIN2 PUK ADM). */
        /* "CHV1 status." */
        uint8_t const chv1_status = 0b10000011;
        /* "UNBLOCK CHV1 status." */
        uint8_t const chv1_unblock_status = 0b10001010;
        /* "CHV2." */
        uint8_t const chv2_status = 0b10000011;
        /* "UNBLOCK CHV2." */
        uint8_t const chv2_unblock_status = 0b10001010;

        /* Create the response in the response buffer. */
        {
            /* Check if destination buffer can fit the response. */
            if (*buf_res_len < 23U)
            {
                return -1;
            }
            /**
             * Using response buffer as temporary storage for complete
             * response.
             */
            memset(&buf_res[0U], 0U, 2U);
            memcpy(&buf_res[2U], &mem_free_short, 2U);
            memcpy(&buf_res[4U], &file_id, 2U);
            memcpy(&buf_res[6U], &file_type, 1U);
            memset(&buf_res[7U], 0U, 5U);
            memcpy(&buf_res[12U], &gsm_data_len, 1U);
            memcpy(&buf_res[13U], &file_characteristic, 1U);
            memcpy(&buf_res[14U], &df_child_count, 1U);
            memcpy(&buf_res[15U], &ef_child_count, 1U);
            memcpy(&buf_res[16U], &code_count, 1U);
            memset(&buf_res[17U], 0, 1U);
            memcpy(&buf_res[18U], &chv1_status, 1U);
            memcpy(&buf_res[19U], &chv1_unblock_status, 1U);
            memcpy(&buf_res[20U], &chv2_status, 1U);
            memcpy(&buf_res[21U], &chv2_unblock_status, 1U);
            memset(&buf_res[22U], 0U, 1U);
            *buf_res_len = 23U;
        }
        return 0;
    }
    case SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT:
    case SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED:
    case SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC: {
        /**
         * "File size (for transparent EF: the length of the body part of
         * the EF) (for linear fixed or cyclic EF: record length multiplied
         * by the number of records of the EF)."
         */
        uint16_t file_size_be;
        uint8_t rcrd_length;
        if (file->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT)
        {
            /* Safe cast since same statement checks for overflow. */
            file_size_be = htobe16(file->data_size > UINT16_MAX
                                       ? UINT16_MAX
                                       : (uint16_t)file->data_size);
        }
        else
        {
            uint32_t rcrd_cnt;
            swicc_ret_et const ret_rcrd_cnt =
                swicc_disk_file_rcrd_cnt(tree, file, &rcrd_cnt);
            if (ret_rcrd_cnt != SWICC_RET_SUCCESS || rcrd_cnt > UINT16_MAX)
            {
                /**
                 * @todo NV modified but can't indicate this, not sure what
                 * to do here.
                 */
                return -1;
            }
            uint8_t const rcrd_size =
                file->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED
                    ? file->hdr_spec.ef_linearfixed.rcrd_size
                    : file->hdr_spec.ef_cyclic.rcrd_size;
            /**
             * Safe cast since record count is <= UINT16_MAX and record size
             * is uint8 so can't overflow.
             */
            file_size_be = htobe16((uint16_t)(rcrd_cnt * rcrd_size));
            rcrd_length = rcrd_size;
        }
        /* "File ID." */
        uint16_t const file_id_be = htobe16(file->hdr_file.id);
        /* "Type of file." */
        uint8_t const file_type = 0x04;
        /**
         * Byte 7 (index 7, standard counts from 1 so there it's byte 8) is
         * RFU for linear-fixed and transparent EFs but for cyclic all bits
         * except b6 (index 6, in standard they index bits from 1 so it's b7
         * there) are RFU. b6=1 means INCREASE is allowed on this file.
         */
        uint8_t const b7 =
            file->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC ? 0x01
                                                                     : 0x00;
        /* "Access conditions." */
        uint8_t const access_cond[3U] = {0x00, 0x00, 0x00};
        /* "File status." */
        /**
         * All bits except b0 are RFU and shall be 0. b0=0 if invalidated,
         * b0=0, if not invalidated b0=1.
         */
        uint8_t const file_status =
            file->hdr_item.lcs == SWICC_FS_LCS_OPER_ACTIV ? 0x01 : 0x00;
        /* Length of the remainder of the response. */
        uint8_t const data_extra_len = 2U; /* Everything. */
        /* "Structure of EF." */
        uint8_t ef_structure;
        switch (file->hdr_item.type)
        {
        case SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT:
            ef_structure = 0x00;
            break;
        case SWICC_FS_ITEM_TYPE_FILE_EF_LINEARFIXED:
            ef_structure = 0x01;
            break;
        case SWICC_FS_ITEM_TYPE_FILE_EF_CYCLIC:
            ef_structure = 0x03;
            break;
        default:
            __builtin_unreachable();
        }
        rcrd_length = 0;

        /* Create the response in the response buffer. */
        {
            /* Check if destination buffer can fit the response. */
            if ((file->hdr_item.type ==
                     SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT &&
                 *buf_res_len < 15U + file->data_size) ||
                *buf_res_len < 15)
            {
                return -1;
            }

            /**
             * Using response buffer as temporary storage for complete
             * response.
             */
            memset(&buf_res[0U], 0U, 2U);
            memcpy(&buf_res[2U], &file_size_be, 2U);
            memcpy(&buf_res[4U], &file_id_be, 2U);
            memcpy(&buf_res[6U], &file_type, 1U);
            memcpy(&buf_res[7U], &b7, 1U);
            memcpy(&buf_res[8U], access_cond, 3U);
            memcpy(&buf_res[11U], &file_status, 1U);
            memcpy(&buf_res[12U], &data_extra_len, 1U);
            memcpy(&buf_res[13U], &ef_structure, 1U);
            memcpy(&buf_res[14U], &rcrd_length, 1U);
            *buf_res_len = 15U;
        }
        return 0;
    }
    default:
        return -1;
        break;
    }
}

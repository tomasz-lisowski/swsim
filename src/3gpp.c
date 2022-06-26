#include "3gpp.h"
#include <endian.h>
#include <string.h>

/* Toggle to 1 to send additional data in SELECT response. */
#define SELECT_3GPP_MEM_TOT 0
#define SELECT_3GPP_SYS_CMD 0
#define SELECT_3GPP_UICC_ENV_COND 0

int32_t o3gpp_select_res(swicc_fs_st const *const fs,
                         swicc_disk_tree_st *const tree,
                         swicc_fs_file_st *const file, uint8_t *const buf_res,
                         uint16_t *const buf_res_len)
{
    swicc_fs_file_st *const file_selected = file;

    /**
     * ETSI TS 102 221 V16.4.0 pg.86 sec.11.1.1.3 describes what BER-TLV
     * tags shall be included in responses to certain files. These are
     * the top-level tags that shall be present:
     * - MF:  '82', '83',       'A5', '8A', '8B'^'8C'^'AB', 'C6', '81'
     * - ADF: '82', '83', '84', 'A5', '8A', '8B'^'8C'^'AB', 'C6', '81'
     * - DF:  '82', '83',       'A5', '8A', '8B'^'8C'^'AB', 'C6', '81'
     * - EF:  '82', '83',       'A5', '8A', '8B'^'8C'^'AB', '80', '81',
     *        '88'
     *
     * Proprietary information 'A5' tags described in ETSI TS 102 221
     * V16.4.0 sec.11.1.1.4.6.0:
     * - MF:  '80',              '83', '87', '88', '89'
     * - ADF:        '81', '82', '83', '87'
     * - DF:                     '83', '87'
     * - EF (non-BER-TLV):
     * - EF (BER-TLV):           '83', '84', '85', '86'
     *
     * PIN status template 'C6' tags described in ETSI TS 102 221
     * V16.4.0 sec.11.1.1.4.10:
     * - '90', '95', '83'
     */
    static uint8_t const tags[] = {
        0x62, /* FCP Template. */
        0x80, /* '62': File size,
                 'A5': UICC characteristics. */
        0x81, /* '62': Total file size,
                 'A5': App power consumption. */
        0x82, /* '62': File descriptor,
                 'A5': Min app clock frequency. */
        0x83, /* '62': File ID,
                 'A5': Available memory.
                 'C6': Key reference. */
        0x84, /* '62': DF Name,
                 'A5': File descriptor. */
        0x85, /* 'A5': Reserved file size. */
        0x86, /* 'A5': Maximum file size. */
        0x87, /* 'A5': Supported system commands. */
        0x88, /* '62': Short File ID,
                 'A5': Specific UICC environmental conditions. */
        0x8A, /* '62': Life cycle status. */
        0x8B, /* '62': Security attributes (reference). */
        0x90, /* 'C6': PS_DO. */
        0x95, /* 'C6': Usage qualifier. */
        0xA5, /* '62': Proprietary information. */
        0xC6, /* '62': PIN status template DO. */
        // 0x89, /* 'A5': Platform to platform CAT secured APDU. Absent
        //          since 3GPP 31.101 V17.0.0 pg.19 sec.11.1.1.4.6
        //          indicates this shall not be present unlike what was
        //          indicated in ETSI TS 102 221 V16.4.0
        //          sec.11.1.1.4.6.10. */

        /**
         * All security attributes are indicated by reference so these
         * tags are not used.
         */
        0x8C, /* '62': Security attributes (compact). */
        // 0xAB, /* '62': Security attributes (expanded). */
    };
    static uint32_t const tags_count = sizeof(tags) / sizeof(tags[0U]);
    swicc_dato_bertlv_tag_st bertlv_tags[tags_count];
    for (uint8_t tag_idx = 0U; tag_idx < tags_count; ++tag_idx)
    {
        if (swicc_dato_bertlv_tag_create(&bertlv_tags[tag_idx],
                                         tags[tag_idx]) != SWICC_RET_SUCCESS)
        {
            return -1;
        }
    }

    /* Create data for BER-TLV DOs. */
    if (file_selected->data_size > UINT16_MAX)
    {
        return -1;
    }
    /* Safe cast due to the bound check in 'if'. */
    uint16_t const data_size_be = htobe16((uint16_t)file_selected->data_size);
#if SELECT_3GPP_MEM_TOT == 1
    uint32_t const data_size_tot_be = htobe32(file_selected->hdr_item.size);
#endif
    uint16_t const data_id_be = htobe16(file_selected->hdr_file.id);
    uint8_t const data_sid = file_selected->hdr_file.sid;
    /**
     * @warning The ATR and the UICC characteristics need to indicate
     * the same capability!
     */
    uint8_t const data_uicc_char[1U] = {
        0b01110001, /**
                     * LSB>MSB
                     * UICC characteristics
                     *    1b Clock stop allowed = 1 (allowed)
                     *  + 1b RFU                = 0
                     *  + 2b Clock stop level   = 00 (no preferrence)
                     *  + 4b RFU                = 0000
                     */
    };
    uint8_t const data_app_power_cons[3U] = {
        0x01, /* Supply voltage class at which the power consumption is
                 measured. 0x01 = Class A. */
        0x00, /* Application power consumption. 0x00 = Indicates that
                 this should be ignored. */
        0xFF, /* Power consumption reference frequency. 0xFF = No
                 reference frequency is indicated. */
    };
    uint8_t const data_app_clk_min[1U] = {
        0xFF, /* Application minimum clock frequency. 0xFF = No minimum
                 app clock frequency is indicated. According to
                 3GPP 31.101 V17.0.0 pg.20 sec.11.1.1.4.6, a value of
                 1MHz will be assumed. */
    };
    uint32_t const data_mem_available_be =
        htobe32(UINT32_MAX - fs->va.cur_tree->len);
    uint8_t const data_file_details[1U] = {
        0b00000001, /**
                     * LSB>MSB
                     * File details
                     *    1b DER coding = 1 (supported)
                     *  + 7b RFU        = 0000000
                     */
    };
    /**
     * Number of data bytes reserved for selected file that cannot be
     * allocated by any other entity.
     */
    uint16_t const data_file_size_reserved_be = htobe16(0x0000);
    /**
     * File size that shall not be exceeded excluding structural
     * information for the file.
     * @note Setting this to a value larger than any header with lots of
     * extra margin.
     */
    uint32_t const data_file_size_max_be = htobe32(UINT32_MAX - 1024U);
#if SELECT_3GPP_SYS_CMD == 1
    uint8_t const data_sys_cmd_support[1U] = {
        0b00000000, /* Supported commands. 0x00 = TERMINAL CAPABILITY
                       not supported (indicated by LSB and rest of bits
                       is RFU). */
    };
#endif
#if SELECT_3GPP_UICC_ENV_COND == 1
    uint8_t const data_uicc_env_cond[1U] = {
        0b00001011, /**
                     * LSB>MSB
                     *    3b Temperature class = 011 (class C)
                     *  + 1b High humidity     = 1 (supported)
                     *  + 4b RFU               = 0000
                     */
    };
#endif
    uint8_t const data_sec_attr_compact[] = {
        0b01111111, /* Access Mode (AM): All operations allowed. */
        /**
         * The remaining 0x00 bytes all indicate an ALWAYS security
         * condition (SC) for each of the 1 bits of AM.
         * The meaning of each ALWAYS condition changes based on type of
         * thing being selected but that is ignored as everything is
         * allowed always.
         */
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    uint8_t const data_pin_status_ps_do[] = {
        0b01110000, /**
                     * ETSI TS 102 221 V16.4.0 sec.9.5.2.
                     * LSB>MSB
                     * PIN enabled/disabled
                     *    1b key 8 = 0 (disabled)
                     *  + 1b key 7 = 0 (disabled)
                     *  + 1b key 6 = 0 (disabled)
                     *  + 1b key 5 = 0 (disabled)
                     *  + 1b key 4 = 1 (enabled)
                     *  + 1b key 3 = 1 (enabled)
                     *  + 1b key 2 = 1 (enabled)
                     *  + 1b key 1 = 0 (disabled)
                     */
    };
    uint8_t const data_pin_status_key_ref[4U][1U] = {
        /**
         * The references are defined in ETSI TS 102 221 V16.4.0
         * sec.9.5.1 table.9.3.
         */
        {
            0x01, /* PIN Appl 1 */
        },
        {
            0x81, /* Second PIN Appl 1 */
        },
        {
            0x0A, /* ADM1 */
        },
        {
            0x0B, /* ADM2 */
        },
    };
    uint8_t lcs_be;
    uint8_t descr_be[SWICC_FS_FILE_DESCR_LEN_MAX];
    uint8_t descr_len = 0U;
    if (swicc_fs_file_lcs(file_selected, &lcs_be) != SWICC_RET_SUCCESS ||
        swicc_fs_file_descr(fs->va.cur_tree, file_selected, descr_be,
                            &descr_len) != SWICC_RET_SUCCESS)
    {
        return -1;
    }
    uint8_t data_aid[SWICC_FS_ADF_AID_LEN];
    if (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF)
    {
        memcpy(file_selected->hdr_spec.adf.aid.rid, data_aid,
               SWICC_FS_ADF_AID_RID_LEN);
        memcpy(file_selected->hdr_spec.adf.aid.pix,
               &data_aid[SWICC_FS_ADF_AID_RID_LEN], SWICC_FS_ADF_AID_PIX_LEN);
    }

    uint8_t *bertlv_buf;
    uint32_t bertlv_len;
    swicc_ret_et ret_bertlv = SWICC_RET_ERROR;
    swicc_dato_bertlv_enc_st enc;
    for (bool dry_run = true;; dry_run = false)
    {
        if (dry_run)
        {
            bertlv_buf = NULL;
            bertlv_len = 0;
        }
        else
        {
            if (enc.len > *buf_res_len)
            {
                break;
            }
            bertlv_len = enc.len;
            bertlv_buf = buf_res;
        }

        swicc_dato_bertlv_enc_init(&enc, bertlv_buf, bertlv_len);
        swicc_dato_bertlv_enc_st enc_fcp;
        swicc_dato_bertlv_enc_st enc_prop_info;
        swicc_dato_bertlv_enc_st enc_pin_status;

        if (swicc_dato_bertlv_enc_nstd_start(&enc, &enc_fcp) !=
                SWICC_RET_SUCCESS ||

            /* Short file ID (SFI). */
            (SWICC_FS_FILE_EF_CHECK(file_selected) &&
                     file_selected->hdr_file.sid != SWICC_FS_SID_MISSING
                 ? (swicc_dato_bertlv_enc_data(&enc_fcp, &data_sid,
                                               sizeof(data_sid)) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[9U]) !=
                        SWICC_RET_SUCCESS)
                 : false) ||

#if SELECT_3GPP_MEM_TOT == 1
            /* Total file size. */
            swicc_dato_bertlv_enc_data(&enc_fcp, (uint8_t *)&data_size_tot_be,
                                       sizeof(data_size_tot_be)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[2U]) !=
                SWICC_RET_SUCCESS ||
#endif

            /* File size for EFs. */
            (SWICC_FS_FILE_EF_CHECK(file_selected)
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_fcp, (uint8_t *)&data_size_be,
                        sizeof(data_size_be)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[1U]) !=
                        SWICC_RET_SUCCESS)
                 : false) ||

            /* PIN status for non-EFs. */
            (SWICC_FS_FILE_FOLDER_CHECK(file_selected)
                 ? (swicc_dato_bertlv_enc_nstd_start(
                        &enc_fcp, &enc_pin_status) != SWICC_RET_SUCCESS ||

                    /* ADM2. */
                    swicc_dato_bertlv_enc_data(
                        &enc_pin_status,
                        (uint8_t *)&data_pin_status_key_ref[3U],
                        sizeof(data_pin_status_key_ref[3U])) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                              &bertlv_tags[4U]) !=
                        SWICC_RET_SUCCESS ||

                    /* ADM1. */
                    swicc_dato_bertlv_enc_data(
                        &enc_pin_status,
                        (uint8_t *)&data_pin_status_key_ref[2U],
                        sizeof(data_pin_status_key_ref[2U])) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                              &bertlv_tags[4U]) !=
                        SWICC_RET_SUCCESS ||

                    /* Second PIN Appl 1. */
                    swicc_dato_bertlv_enc_data(
                        &enc_pin_status,
                        (uint8_t *)&data_pin_status_key_ref[1U],
                        sizeof(data_pin_status_key_ref[1U])) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                              &bertlv_tags[4U]) !=
                        SWICC_RET_SUCCESS ||

                    /* PIN Appl 1. */
                    swicc_dato_bertlv_enc_data(
                        &enc_pin_status,
                        (uint8_t *)&data_pin_status_key_ref[0U],
                        sizeof(data_pin_status_key_ref[0U])) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                              &bertlv_tags[4U]) !=
                        SWICC_RET_SUCCESS ||

                    /* PS_DO. */
                    swicc_dato_bertlv_enc_data(
                        &enc_pin_status, (uint8_t *)&data_pin_status_ps_do,
                        sizeof(data_pin_status_ps_do)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                              &bertlv_tags[12U]) !=
                        SWICC_RET_SUCCESS ||

                    swicc_dato_bertlv_enc_nstd_end(&enc_fcp, &enc_pin_status) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[15U]) !=
                        SWICC_RET_SUCCESS)
                 : false) ||

            /* Security attributes (compact). */
            swicc_dato_bertlv_enc_data(
                &enc_fcp, (uint8_t *)&data_sec_attr_compact,
                sizeof(data_sec_attr_compact)) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[16U]) !=
                SWICC_RET_SUCCESS ||

            /* Life cycle status. */
            swicc_dato_bertlv_enc_data(&enc_fcp, &lcs_be, sizeof(lcs_be)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[10U]) !=
                SWICC_RET_SUCCESS ||

            /* Proprietary info. */
            swicc_dato_bertlv_enc_nstd_start(&enc_fcp, &enc_prop_info) !=
                SWICC_RET_SUCCESS ||

#if SELECT_3GPP_UICC_ENV_COND == 1
            /* Specific UICC environmental conditions for MF.  */
            (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_MF
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_uicc_env_cond,
                        sizeof(data_uicc_env_cond)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[9U]) != SWICC_RET_SUCCESS)
                 : false) ||
#endif

#if SELECT_3GPP_SYS_CMD == 1
            /* Supported system commands for folders. */
            (SWICC_FS_FILE_FOLDER_CHECK(file_selected)
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_sys_cmd_support,
                        sizeof(data_sys_cmd_support)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[8U]) != SWICC_RET_SUCCESS)
                 : false) ||
#endif

            /**
             * File details, reserved file size, and maximum file size
             * for BER-TLV EFs.
             */
            (SWICC_FS_FILE_EF_BERTLV_CHECK(file_selected)
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_file_size_max_be,
                        sizeof(data_file_size_max_be)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                              &bertlv_tags[7U]) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_file_size_reserved_be,
                        sizeof(data_file_size_reserved_be)) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                              &bertlv_tags[6U]) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_file_details,
                        sizeof(data_file_details)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[5U]) != SWICC_RET_SUCCESS)
                 : false) ||

            /* Total memory available for folders and BER-TLV EFs. */
            ((SWICC_FS_FILE_FOLDER_CHECK(file_selected) ||
              SWICC_FS_FILE_EF_BERTLV_CHECK(file_selected))
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_mem_available_be,
                        sizeof(data_mem_available_be)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[4U]) != SWICC_RET_SUCCESS)
                 : false) ||

            /* App power consumption and min app clock for ADF. */
            (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_app_clk_min,
                        sizeof(data_app_clk_min)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                              &bertlv_tags[3U]) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_app_power_cons,
                        sizeof(data_app_power_cons)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[2U]) != SWICC_RET_SUCCESS)
                 : false) ||

            /* UICC characteristics for MF. */
            (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_MF
                 ? (swicc_dato_bertlv_enc_data(
                        &enc_prop_info, (uint8_t *)&data_uicc_char,
                        sizeof(data_uicc_char)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(
                        &enc_prop_info, &bertlv_tags[1U]) != SWICC_RET_SUCCESS)
                 : false) ||

            swicc_dato_bertlv_enc_nstd_end(&enc_fcp, &enc_prop_info) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[14U]) !=
                SWICC_RET_SUCCESS ||

            /* DF name (AID) only for ADF. */
            (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF
                 ? (swicc_dato_bertlv_enc_data(&enc_fcp, data_aid,
                                               sizeof(data_aid)) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[5U]) !=
                        SWICC_RET_SUCCESS)
                 : false) ||

            /* FID. */
            swicc_dato_bertlv_enc_data(&enc_fcp, (uint8_t *)&data_id_be,
                                       sizeof(data_id_be)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[4U]) !=
                SWICC_RET_SUCCESS ||

            /* File descriptor. */
            swicc_dato_bertlv_enc_data(&enc_fcp, descr_be, descr_len) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[3U]) !=
                SWICC_RET_SUCCESS ||

            swicc_dato_bertlv_enc_nstd_end(&enc, &enc_fcp) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc, &bertlv_tags[0U]) !=
                SWICC_RET_SUCCESS)
        {
            break;
        }

        /* Stop when finished with the real run (i.e. not dry run). */
        if (!dry_run)
        {
            ret_bertlv = SWICC_RET_SUCCESS;
            break;
        }
    }

    if (ret_bertlv == SWICC_RET_SUCCESS && bertlv_len <= UINT16_MAX)
    {
        *buf_res_len = (uint16_t)bertlv_len;
        return 0;
    }
    else
    {
        return -1;
    }
}

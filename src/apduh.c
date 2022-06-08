#include "apduh.h"
#include "apdu.h"
#include "uicc/fs/common.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Handle the SELECT command in the proprietary classes 0X, 4X, and 6X.
 * @param uicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in ETSI TS 102 221 V16.4.0 pg.84 sec.11.1.1.
 */
static uicc_apduh_ft apduh_select;
static uicc_ret_et apduh_select(uicc_st *const uicc_state,
                                uicc_apdu_cmd_st const *const cmd,
                                uicc_apdu_res_st *const res,
                                uint32_t const procedure_count)
{
    enum meth_e
    {
        METH_RFU,
        METH_FID,       /* Select DF, EF or MF by file ID */
        METH_DF_NESTED, /* Select child DF of the current DF */
        METH_DF_PARENT, /* Select parent DF of the current DF */
        METH_DF_NAME,   /* Selection by DF name (AID) */
        METH_PATH_MF,   /* Select by path from MF */
        METH_PATH_DF,   /* Select by path from current DF */
    } meth = METH_RFU;

    enum data_req_e
    {
        DATA_REQ_RFU,
        DATA_REQ_FCP,
        DATA_REQ_ABSENT,
    } data_req = DATA_REQ_RFU;

    enum app_sesh_ctrl_e
    {
        APP_SESH_CTRL_RFU,
        APP_SESH_CTRL_ACTIVATION,
        APP_SESH_CTRL_TERMINATION,
    } __attribute((unused)) app_sesh_ctrl = APP_SESH_CTRL_RFU;

    /* Only used with selection by AID. */
    uicc_fs_occ_et occ = UICC_FS_OCC_FIRST;
    bool occ_rfu = false;

    /* This fully parses P1. */
    switch (cmd->hdr->p1)
    {
    case 0b00000000:
        meth = METH_FID;
        break;
    case 0b00000001:
        meth = METH_DF_NESTED;
        break;
    case 0b00000011:
        meth = METH_DF_PARENT;
        break;
    case 0b00000100:
        meth = METH_DF_NAME;
        break;
    case 0b00001000:
        meth = METH_PATH_MF;
        break;
    case 0b00001001:
        meth = METH_PATH_DF;
        break;
    default:
        meth = METH_RFU;
        break;
    }

    /* Parse P2. */
    {
        /* App session control. */
        switch (cmd->hdr->p2 & 0b01100000)
        {
        case 0b00000000:
            app_sesh_ctrl = APP_SESH_CTRL_ACTIVATION;
            break;
        case 0b01000000:
            app_sesh_ctrl = APP_SESH_CTRL_TERMINATION;
            break;
        default:
            app_sesh_ctrl = APP_SESH_CTRL_RFU;
            break;
        }

        /* Data requested. */
        switch (cmd->hdr->p2 & 0b10011100)
        {
        case 0b00000100:
            data_req = DATA_REQ_FCP;
            break;
        case 0b00001100:
            data_req = DATA_REQ_ABSENT;
            break;
        default:
            data_req = DATA_REQ_RFU;
            break;
        }

        /* Occurrence. */
        switch (cmd->hdr->p2 & 0b00000011)
        {
        case 0b00000000:
            occ = UICC_FS_OCC_FIRST;
            break;
        case 0b00000001:
            occ = UICC_FS_OCC_LAST;
            break;
        case 0b00000010:
            occ = UICC_FS_OCC_NEXT;
            break;
        case 0b00000011:
            occ = UICC_FS_OCC_PREV;
            break;
        default:
            /* RFU */
            occ_rfu = true;
            break;
        }
    }

    /**
     * 1. When P1 = 0x00, P3 = 0, P2 shall be 0xC0.
     * 2. b1 and b2 of P2 shall be 0 when P1 != 0x04.
     * 3. ADF occurrence shall not be RFU.
     */
    if ((cmd->hdr->p1 == 0x00 && *cmd->p3 == 0U && cmd->hdr->p2 != 0xC0) ||
        (cmd->hdr->p1 != 0x04 && (cmd->hdr->p2 & 0b00000011) != 0U) ||
        occ_rfu == true)
    {
        res->sw1 = UICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return UICC_RET_SUCCESS;
    }

    /**
     * Check if we only got Lc which means we need to send back a procedure
     * byte.
     */
    if (procedure_count == 0U)
    {
        /**
         * Unexpected because before sending a procedure, no data should have
         * been received.
         */
        if (cmd->data->len != 0U)
        {
            res->sw1 = UICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }

        /**
         * If Lc is 0 it means data is absent so we can process what we got,
         * otherwise we need more from the interface.
         */
        if (*cmd->p3 > 0)
        {
            res->sw1 = UICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return UICC_RET_SUCCESS;
        }
    }

    /**
     * The ACK ALL procedure was sent and we expected to receive all the data
     * (length of which was given in P3) but did not receive the expected amount
     * of data.
     */
    if (cmd->data->len != *cmd->p3 && procedure_count >= 1U)
    {
        res->sw1 = UICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x02; /* "Wrong length" */
        res->data.len = 0U;
        return UICC_RET_SUCCESS;
    }

    /* Perform the requested action. */
    {
        /* Unsupported P1/P2 parameters. */
        if (meth == METH_RFU || data_req == DATA_REQ_RFU ||
            meth == METH_DF_NESTED || meth == METH_DF_PARENT)
        {
            res->sw1 = UICC_APDU_SW1_CHER_P1P2;
            res->sw2 = 0U;
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }
        uicc_ret_et ret_select = UICC_RET_ERROR;
        switch (meth)
        {
        case METH_FID:
            /* Must contain exactly 1 file ID. */
            if (cmd->data->len != sizeof(uicc_fs_id_kt))
            {
                ret_select = UICC_RET_ERROR;
            }
            else
            {
                ret_select = uicc_va_select_file_id(
                    &uicc_state->fs,
                    __builtin_bswap16(*(uicc_fs_id_kt *)cmd->data->b));
            }
            break;
        case METH_DF_NAME:
            /* Check if maybe trying to select an ADF. */
            if (cmd->data->len > UICC_FS_ADF_AID_LEN ||
                cmd->data->len < UICC_FS_ADF_AID_RID_LEN ||
                occ != UICC_FS_OCC_FIRST)
            {
                /* Try to select by DF name at least. */
                if (cmd->data->len == 0 || occ != UICC_FS_OCC_FIRST)
                {
                    ret_select = UICC_RET_ERROR;
                }
                else
                {
                    ret_select = uicc_va_select_file_dfname(
                        &uicc_state->fs, (char *)cmd->data->b, cmd->data->len);
                }
            }
            else
            {
                ret_select = uicc_va_select_adf(&uicc_state->fs, cmd->data->b,
                                                cmd->data->len -
                                                    UICC_FS_ADF_AID_RID_LEN);
            }
            break;
        case METH_PATH_MF:
            /* Must contain at least 1 ID in the path. */
            if (cmd->data->len < sizeof(uicc_fs_id_kt) ||
                occ != UICC_FS_OCC_FIRST)
            {
                ret_select = UICC_RET_ERROR;
            }
            else
            {
                uicc_fs_path_st const path = {
                    .b = cmd->data->b,
                    .len = cmd->data->len,
                    .type = UICC_FS_PATH_TYPE_MF,
                };
                ret_select = uicc_va_select_file_path(&uicc_state->fs, path);
            }
            break;
        case METH_PATH_DF:
            /* Must contain at least 1 ID in the path. */
            if (cmd->data->len < sizeof(uicc_fs_id_kt) ||
                occ != UICC_FS_OCC_FIRST)
            {
                ret_select = UICC_RET_ERROR;
            }
            else
            {
                uicc_fs_path_st const path = {
                    .b = cmd->data->b,
                    .len = cmd->data->len,
                    .type = UICC_FS_PATH_TYPE_DF,
                };
                ret_select = uicc_va_select_file_path(&uicc_state->fs, path);
            }
            break;
        default:
            /**
             * Will never get here because param values have been rejected if
             * unsupported.
             */
            __builtin_unreachable();
        }

        if (ret_select == UICC_RET_FS_NOT_FOUND)
        {
            res->sw1 = UICC_APDU_SW1_CHER_P1P2_INFO;
            res->sw2 = 0x82; /* "Not found" */
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }
        else if (ret_select != UICC_RET_SUCCESS)
        {
            /* Failed to select. */
            res->sw1 = UICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }

        if (data_req == DATA_REQ_ABSENT)
        {
            res->sw1 = UICC_APDU_SW1_NORM_NONE;
            res->sw2 = 0U;
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }
        else
        {
            /* The file that was requested to be selected. */
            uicc_fs_file_st *const file_selected = &uicc_state->fs.va.cur_file;

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
                // 0x89, /* 'A5': Platform to platform CAT secured APDU. */
                // 0x8C, /* '62': Security attributes (compact). */
                // 0xAB, /* '62': Security attributes (expanded). */
            };
            static uint32_t const tags_count = sizeof(tags) / sizeof(tags[0U]);
            uicc_dato_bertlv_tag_st bertlv_tags[tags_count];
            for (uint8_t tag_idx = 0U; tag_idx < tags_count; ++tag_idx)
            {
                if (uicc_dato_bertlv_tag_create(&bertlv_tags[tag_idx],
                                                tags[tag_idx]) !=
                    UICC_RET_SUCCESS)
                {
                    res->sw1 = UICC_APDU_SW1_CHER_UNK;
                    res->sw2 = 0U;
                    res->data.len = 0U;
                    return UICC_RET_SUCCESS;
                }
            }

            /* Create data for BER-TLV DOs. */

            uint32_t const data_size_be =
                __builtin_bswap32(file_selected->data_size);
            uint32_t const data_size_tot_be =
                __builtin_bswap32(file_selected->hdr_item.size);
            uint16_t const data_id_be =
                __builtin_bswap16(file_selected->hdr_file.id);
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
                         app clock frequency is indicated. */
            };
            uint32_t const data_mem_available_be =
                __builtin_bswap32(UINT32_MAX - uicc_state->fs.va.cur_tree->len);
            uint8_t const data_file_details[1U] = {
                0b00000001, /**
                             * LSB>MSB
                             * File details
                             *    1b DER coding = 1 (supported)
                             *  + 7b RFU        = 0000000
                             */
            };
            uint16_t const data_file_size_reserved_be = __builtin_bswap16(
                0x0000); /* Number of data bytes reserved for selected file that
                            cannot be allocated by any other entity. */
            uint32_t const data_file_size_max_be =
                UINT32_MAX -
                1024U /* Larger than any header with lots of extra margin. */
                ; /* File size that shall not be exceeded excluding structural
                     information for the file. */
            uint8_t const data_sys_cmd_support[1U] = {
                0b00000000, /* Supported commands. 0x00 = TERMINAL CAPABILITY
                               not supported (indicated by LSB and rest of bits
                               is RFU). */
            };
            uint8_t const data_uicc_env_cond[1U] = {
                0b00001011, /**
                             * LSB>MSB
                             *    3b Temperature class = 011 (class C)
                             *  + 1b High humidity     = 1 (supported)
                             *  + 4b RFU               = 0000
                             */
            };
            uint8_t const data_sec_attr[3U] = {
                0x06, /* B1 of EF ARR FID. */
                0x6F, /* B0 of EF ARR FID. */
                0x00, /* Record number in EF ARR that contains the security
                         attributes. */
            };
            uint8_t const data_pin_status_ps_do[] = {
                0b10000000, /**
                             * ETSI TS 102 221 V16.4.0 sec.9.5.2.
                             * LSB>MSB
                             * PIN enabled/disabled
                             *    1b key 8 = 0 (disabled)
                             *  + 1b key 7 = 0 (disabled)
                             *  + 1b key 6 = 0 (disabled)
                             *  + 1b key 5 = 0 (disabled)
                             *  + 1b key 4 = 0 (disabled)
                             *  + 1b key 3 = 0 (disabled)
                             *  + 1b key 2 = 0 (disabled)
                             *  + 1b key 1 = 1 (enabled)
                             */
            };
            uint8_t const data_pin_status_usage_qualif[1U][1U] = {
                {
                    0x08, /* b4 = 1 means "use the PIN for verification (Key
                             Reference data user knowledge based)". This is the
                             usage qualifier needed for the universal PIN.
                             ETSI TS 102 221 V16.4.0 sec.9.5.1 table.9.3. */
                },
            };
            uint8_t const data_pin_status_key_ref[1U][1U] = {
                {
                    0x11, /* '11' is a special value reserved for the universal
                             PIN.
                             ETSI TS 102 221 V16.4.0 sec.9.5.1 table.9.3. */
                },
            };
            uint8_t lcs_be;
            uint8_t descr_be[UICC_FS_FILE_DESCR_LEN_MAX];
            uint8_t descr_len = 0U;
            if (uicc_fs_file_lcs(file_selected, &lcs_be) != UICC_RET_SUCCESS ||
                uicc_fs_file_descr(uicc_state->fs.va.cur_tree, file_selected,
                                   descr_be, &descr_len) != UICC_RET_SUCCESS)
            {
                res->sw1 = UICC_APDU_SW1_CHER_UNK;
                res->sw2 = 0U;
                res->data.len = 0U;
                return UICC_RET_SUCCESS;
            }
            uint8_t data_aid[UICC_FS_ADF_AID_LEN];
            if (file_selected->hdr_item.type == UICC_FS_ITEM_TYPE_FILE_ADF)
            {
                memcpy(file_selected->hdr_spec.adf.aid.rid, data_aid,
                       UICC_FS_ADF_AID_RID_LEN);
                memcpy(file_selected->hdr_spec.adf.aid.pix,
                       &data_aid[UICC_FS_ADF_AID_RID_LEN],
                       UICC_FS_ADF_AID_PIX_LEN);
            }

            uint8_t *bertlv_buf;
            uint32_t bertlv_len;
            uicc_ret_et ret_bertlv = UICC_RET_ERROR;
            uicc_dato_bertlv_enc_st enc;
            for (bool dry_run = true;; dry_run = false)
            {
                if (dry_run)
                {
                    bertlv_buf = NULL;
                    bertlv_len = 0;
                }
                else
                {
                    if (enc.len > sizeof(res->data.b))
                    {
                        break;
                    }
                    bertlv_len = enc.len;
                    bertlv_buf = res->data.b;
                }

                uicc_dato_bertlv_enc_init(&enc, bertlv_buf, bertlv_len);
                uicc_dato_bertlv_enc_st enc_fcp;
                uicc_dato_bertlv_enc_st enc_prop_info;
                uicc_dato_bertlv_enc_st enc_pin_status;

                if (uicc_dato_bertlv_enc_nstd_start(&enc, &enc_fcp) !=
                        UICC_RET_SUCCESS ||

                    /* Short file ID (SFI). */
                    (UICC_FS_FILE_EF_CHECK(file_selected)
                         ? (uicc_dato_bertlv_enc_data(&enc_fcp, &data_sid,
                                                      sizeof(data_sid)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(
                                &enc_fcp, &bertlv_tags[9U]) != UICC_RET_SUCCESS)
                         : false) ||

                    /* Total file size. */
                    uicc_dato_bertlv_enc_data(
                        &enc_fcp, (uint8_t *)&data_size_tot_be,
                        sizeof(data_size_tot_be)) != UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[2U]) !=
                        UICC_RET_SUCCESS ||

                    /* File size for EFs. */
                    (UICC_FS_FILE_EF_CHECK(file_selected)
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_fcp, (uint8_t *)&data_size_be,
                                sizeof(data_size_be)) != UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(
                                &enc_fcp, &bertlv_tags[1U]) != UICC_RET_SUCCESS)
                         : false) ||

                    /* PIN status for non-EFs. */
                    (UICC_FS_FILE_FOLDER_CHECK(file_selected)
                         ? (uicc_dato_bertlv_enc_nstd_start(&enc_fcp,
                                                            &enc_pin_status) !=
                                UICC_RET_SUCCESS ||

                            /* Key reference 1. */
                            uicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_key_ref[0U],
                                sizeof(data_pin_status_key_ref[0U])) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                     &bertlv_tags[4U]) !=
                                UICC_RET_SUCCESS ||

                            /* Usage qualifier 1. */
                            uicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_usage_qualif[0U],
                                sizeof(data_pin_status_usage_qualif[0U])) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                     &bertlv_tags[13U]) !=
                                UICC_RET_SUCCESS ||

                            /* PS_DO. */
                            uicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_ps_do,
                                sizeof(data_pin_status_ps_do)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                     &bertlv_tags[12U]) !=
                                UICC_RET_SUCCESS ||

                            uicc_dato_bertlv_enc_nstd_end(&enc_fcp,
                                                          &enc_pin_status) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_fcp,
                                                     &bertlv_tags[15U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /* Security attributes (reference). */
                    uicc_dato_bertlv_enc_data(
                        &enc_fcp, (uint8_t *)&data_sec_attr,
                        sizeof(data_sec_attr)) != UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[11U]) !=
                        UICC_RET_SUCCESS ||

                    /* Life cycle status. */
                    uicc_dato_bertlv_enc_data(&enc_fcp, &lcs_be,
                                              sizeof(lcs_be)) !=
                        UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[10U]) !=
                        UICC_RET_SUCCESS ||

                    /* Proprietary info. */
                    uicc_dato_bertlv_enc_nstd_start(&enc_fcp, &enc_prop_info) !=
                        UICC_RET_SUCCESS ||

                    /* Specific UICC environmental conditions for MF.  */
                    (file_selected->hdr_item.type == UICC_FS_ITEM_TYPE_FILE_MF
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_uicc_env_cond,
                                sizeof(data_uicc_env_cond)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[9U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /* Supported system commands for folders. */
                    (UICC_FS_FILE_FOLDER_CHECK(file_selected)
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_sys_cmd_support,
                                sizeof(data_sys_cmd_support)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[8U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /**
                     * File details, reserved file size, and maximum file size
                     * for BER-TLV EFs.
                     */
                    (UICC_FS_FILE_EF_BERTLV_CHECK(file_selected)
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_file_size_max_be,
                                sizeof(data_file_size_max_be)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[7U]) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_file_size_reserved_be,
                                sizeof(data_file_size_reserved_be)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[6U]) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_file_details,
                                sizeof(data_file_details)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[5U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /* Total memory available for folders and BER-TLV EFs. */
                    ((UICC_FS_FILE_FOLDER_CHECK(file_selected) ||
                      UICC_FS_FILE_EF_BERTLV_CHECK(file_selected))
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_mem_available_be,
                                sizeof(data_mem_available_be)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[4U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /* App power consumption and min app clock for ADF. */
                    (file_selected->hdr_item.type == UICC_FS_ITEM_TYPE_FILE_ADF
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_app_clk_min,
                                sizeof(data_app_clk_min)) != UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[3U]) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_app_power_cons,
                                sizeof(data_app_power_cons)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[2U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    /* UICC characteristics for MF. */
                    (file_selected->hdr_item.type == UICC_FS_ITEM_TYPE_FILE_MF
                         ? (uicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_uicc_char,
                                sizeof(data_uicc_char)) != UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                     &bertlv_tags[1U]) !=
                                UICC_RET_SUCCESS)
                         : false) ||

                    uicc_dato_bertlv_enc_nstd_end(&enc_fcp, &enc_prop_info) !=
                        UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[14U]) !=
                        UICC_RET_SUCCESS ||

                    /* DF name (AID) only for ADF. */
                    (file_selected->hdr_item.type == UICC_FS_ITEM_TYPE_FILE_ADF
                         ? (uicc_dato_bertlv_enc_data(&enc_fcp, data_aid,
                                                      sizeof(data_aid)) !=
                                UICC_RET_SUCCESS ||
                            uicc_dato_bertlv_enc_hdr(
                                &enc_fcp, &bertlv_tags[5U]) != UICC_RET_SUCCESS)
                         : false) ||

                    /* FID. */
                    uicc_dato_bertlv_enc_data(&enc_fcp, (uint8_t *)&data_id_be,
                                              sizeof(data_id_be)) !=
                        UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[4U]) !=
                        UICC_RET_SUCCESS ||

                    /* File descriptor. */
                    uicc_dato_bertlv_enc_data(&enc_fcp, descr_be, descr_len) !=
                        UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[3U]) !=
                        UICC_RET_SUCCESS ||

                    uicc_dato_bertlv_enc_nstd_end(&enc, &enc_fcp) !=
                        UICC_RET_SUCCESS ||
                    uicc_dato_bertlv_enc_hdr(&enc, &bertlv_tags[0U]) !=
                        UICC_RET_SUCCESS)
                {
                    break;
                }

                /* Stop when finished with the real run (i.e. not dry run). */
                if (!dry_run)
                {
                    ret_bertlv = UICC_RET_SUCCESS;
                    break;
                }
            }

            if (ret_bertlv == UICC_RET_SUCCESS &&
                uicc_apdu_rc_enq(&uicc_state->apdu_rc, res->data.b,
                                 bertlv_len) == UICC_RET_SUCCESS)
            {
                res->sw1 = UICC_APDU_SW1_NORM_BYTES_AVAILABLE;
                /**
                 * Make sure to fail when extended APDUs are used since they are
                 * unsupported here.
                 */
                static_assert(UICC_DATA_MAX == UICC_DATA_MAX_SHRT,
                              "Response buffer length might not fit in SW2 "
                              "if SW1 is 0x61");
                /* Safe cast due to check inside the BER-TLV loop. */
                res->sw2 = (uint8_t)bertlv_len;
                res->data.len = 0U;
                return UICC_RET_SUCCESS;
            }
            else
            {
                res->sw1 = UICC_APDU_SW1_CHER_UNK;
                res->sw2 = 0U;
                res->data.len = 0U;
                return UICC_RET_SUCCESS;
            }
        }
    }
}

/**
 * @brief Handle the TERMINAL PROFILE command in the proprietary class 0x80.
 * @param uicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note
 */
static uicc_apduh_ft apduh_terminal_profile;
static uicc_ret_et apduh_terminal_profile(uicc_st *const uicc_state,
                                          uicc_apdu_cmd_st const *const cmd,
                                          uicc_apdu_res_st *const res,
                                          uint32_t const procedure_count)
{
    /**
     * 1. P1 == 0x00.
     * 2. P2 == 0x00.
     */
    if (cmd->hdr->p1 != 0x00 || cmd->hdr->p2 != 0x00)
    {
        res->sw1 = UICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return UICC_RET_SUCCESS;
    }

    /**
     * Check if we only got Lc which means we need to send back a procedure
     * byte.
     */
    if (procedure_count == 0U)
    {
        /**
         * Unexpected because before sending a procedure, no data should have
         * been received.
         */
        if (cmd->data->len != 0U)
        {
            res->sw1 = UICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return UICC_RET_SUCCESS;
        }

        /**
         * If Lc is 0 it means data is absent so we can process what we got,
         * otherwise we need more from the interface.
         */
        if (*cmd->p3 > 0)
        {
            res->sw1 = UICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return UICC_RET_SUCCESS;
        }
    }

    /* Terminal profile is ignored. */
    res->sw1 = UICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    res->data.len = 0U;
    return UICC_RET_SUCCESS;
}

uicc_ret_et sim_apduh_demux(uicc_st *const uicc_state,
                            uicc_apdu_cmd_st const *const cmd,
                            uicc_apdu_res_st *const res,
                            uint32_t const procedure_count)
{
    uicc_ret_et ret = UICC_RET_APDU_UNHANDLED;
    switch (cmd->hdr->cla.type)
    {
    case UICC_APDU_CLA_TYPE_INTERINDUSTRY:
        switch (cmd->hdr->ins)
        {
        case 0xA4: /* SELECT */
            /**
             * Override the default SELECT because the proprietary one supports
             * fewer/different features and responds with proprietary BER-TLV
             * tags.
             */
            ret = apduh_select(uicc_state, cmd, res, procedure_count);
            break;
        default:
            break;
        }
        break;
    case UICC_APDU_CLA_TYPE_PROPRIETARY:
        /* The UICC does not parse proprietary CLAs beyond just the type. */
        cmd->hdr->cla = sim_apdu_cmd_cla_parse(cmd->hdr->cla.raw);

        /**
         * The constraints on what instructions live in what CLA is described in
         * ETSI TS 102 221 V16.4.0 pg.77 sec.10.1.2 table.10.5.
         */
        switch (cmd->hdr->ins)
        {
        case 0xA4: /* SELECT */
            if ((cmd->hdr->cla.raw & 0xF0) != 0x00 &&
                (cmd->hdr->cla.raw & 0xF0) != 0x40 &&
                (cmd->hdr->cla.raw & 0xF0) != 0x60)
            {
                ret = apduh_select(uicc_state, cmd, res, procedure_count);
            }
            break;
        case 0x10: /* TERMINAL PROFILE */
            if (cmd->hdr->cla.raw == 0x80)
            {
                ret = apduh_terminal_profile(uicc_state, cmd, res,
                                             procedure_count);
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return ret;
}
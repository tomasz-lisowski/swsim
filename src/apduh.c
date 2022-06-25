#include "apduh.h"
#include "apdu.h"
#include "fs.h"
#include "gsm.h"
#include "swicc/apdu.h"
#include "swsim.h"
#include <arpa/inet.h>
#include <endian.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <swicc/fs/common.h>

/**
 * @brief Handle the SELECT command in the proprietary class A0 of GSM 11.11.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) sec.9.2.1 (command),
 * sec.9.3 (coding), and 9.4 (status conditions).
 * @note Some SW1 and SW2 values are non-ISO since they originate from the
 * GSM 11.11 standard and seem to exist there and only there.
 */
static swicc_apduh_ft apduh_gsm_select;
static swicc_ret_et apduh_gsm_select(swicc_st *const swicc_state,
                                     swicc_apdu_cmd_st const *const cmd,
                                     swicc_apdu_res_st *const res,
                                     uint32_t const procedure_count)
{
    swsim_st *const swsim_state = (swsim_st *)swicc_state->userdata;
    if (swsim_state == NULL)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    uint8_t const data_len_exp = 2U;
    if (cmd->hdr->p1 != 0 || cmd->hdr->p2 != 0 || *cmd->p3 != data_len_exp)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Expecting to receive a FID. */
    if (procedure_count == 0U)
    {
        /* Make sure no data is received before first procedure is sent. */
        if (cmd->data->len != 0)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
        res->sw2 = 0U;
        res->data.len = data_len_exp; /* Length of expected data. */
        return SWICC_RET_SUCCESS;
    }

    /**
     * The ACK ALL procedure was sent and we expected to receive all the data
     * but did not receive the expected amount of data.
     */
    if (cmd->data->len != data_len_exp && procedure_count >= 1U)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x02; /* "Incorrect parameter P3" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Perform requested operation. */
    {
        /* GSM SELECT can only select by FID. */
        swicc_fs_id_kt const fid = be16toh(*(uint16_t *)cmd->data->b);
        swicc_ret_et const ret_select =
            swicc_va_select_file_id(&swicc_state->fs, fid);
        if (ret_select == SWICC_RET_FS_NOT_FOUND)
        {
            res->sw1 = 0x94; /* "File ID not found" */
            res->sw2 = 0x04;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        else if (ret_select != SWICC_RET_SUCCESS)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        swicc_fs_file_st *const file_selected = &swicc_state->fs.va.cur_file;
        uint16_t select_res_len = sizeof(res->data);
        if (gsm_select_res(&swicc_state->fs, swicc_state->fs.va.cur_tree,
                           file_selected, res->data.b, &select_res_len) != 0 ||
            select_res_len > UINT8_MAX)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        /* Safe cast due to check in if for GSM SELECT response creation. */
        res->data.len = (uint8_t)select_res_len;

        /* Copy response to the GET RESPONSE buffer. */
        if (swicc_apdu_rc_enq(&swicc_state->apdu_rc, res->data.b,
                              res->data.len) != SWICC_RET_SUCCESS)
        {
            /**
             * @todo NV modified but can't indicate this, not sure what
             * to do here.
             */
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* "Length 'XX' of the response data." where 'XX' is SW2. */
        res->sw1 = 0x9F;
        /* Safe cast due to check while making GSM SELECT response. */
        res->sw2 = (uint8_t)select_res_len; /* Length of response. */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the GET RESPONSE command in the proprietary class A0 of
 * GSM 11.11.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) sec.9.2.18 (command),
 * sec.9.3 (coding), and 9.4 (status conditions).
 * @note Some SW1 and SW2 values are non-ISO since they originate from the
 * GSM 11.11 standard and seem to exist there and only there.
 */
static swicc_apduh_ft apduh_gsm_res_get;
static swicc_ret_et apduh_gsm_res_get(swicc_st *const swicc_state,
                                      swicc_apdu_cmd_st const *const cmd,
                                      swicc_apdu_res_st *const res,
                                      uint32_t const procedure_count)
{
    if (cmd->hdr->p1 != 0 || cmd->hdr->p2 != 0 || cmd->data->len != 0)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    uint32_t data_req_len = *cmd->p3;
    if (swicc_apdu_rc_deq(&swicc_state->apdu_rc, res->data.b, &data_req_len) !=
            SWICC_RET_SUCCESS ||
        data_req_len != *cmd->p3)
    {
        /* Failed to get the requested data. */
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    res->sw1 = SWICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    res->data.len = *cmd->p3;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the READ BINARY command in the proprietary class A0 of
 * GSM 11.11.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) sec.9.2.3 (command),
 * sec.9.3 (coding), and 9.4 (status conditions).
 * @note Some SW1 and SW2 values are non-ISO since they originate from the
 * GSM 11.11 standard and seem to exist there and only there.
 */
static swicc_apduh_ft apduh_gsm_bin_read;
static swicc_ret_et apduh_gsm_bin_read(swicc_st *const swicc_state,
                                       swicc_apdu_cmd_st const *const cmd,
                                       swicc_apdu_res_st *const res,
                                       uint32_t const procedure_count)
{
    /**
     * This command takes no data as input.
     */
    if (cmd->data->len != 0)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0x00;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    uint8_t const len_expected = *cmd->p3;
    uint8_t const offset_hi = cmd->hdr->p1;
    uint8_t const offset_lo = cmd->hdr->p2;
    /* Safe cast as just concatenating the two offset bytes. */
    uint16_t const offset = le16toh((offset_hi << 8U) | offset_lo);

    swicc_fs_file_st *const file = &swicc_state->fs.va.cur_file;
    switch (file->hdr_item.type)
    {
    /**
     * GSM 11.11 v4.21.1 pg.26 sec.8 table.8 indicates that READ BINARY shall
     * only work for transparent EFs.
     */
    case SWICC_FS_ITEM_TYPE_FILE_EF_TRANSPARENT:
        if (len_expected > file->data_size)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_LEN; /* "Incorrect parameter P3." */
            res->sw2 = 0x00;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        else if (offset > file->data_size)
        {
            res->sw1 =
                SWICC_APDU_SW1_CHER_P1P2; /* "Incorrect parameter P1 or P2." */
            res->sw2 = 0x00;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* Enqueue the file data for later retrieval using GET RESPONSE. */
        if (swicc_apdu_rc_enq(&swicc_state->apdu_rc, file->data,
                              len_expected) != SWICC_RET_SUCCESS)
        {
            res->sw1 = 0x92; /* "Memory problem." */
            res->sw2 = 0x40;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* "Length 'XX' of the response data." where 'XX' is SW2. */
        res->sw1 = 0x9F;
        res->sw2 = len_expected;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    case SWICC_FS_ITEM_TYPE_INVALID:
        res->sw1 = 0x94; /* "No EF selected." */
        res->sw2 = 0x00;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    default:
        res->sw1 = 0x94; /* "File is inconsistent with the command." */
        res->sw2 = 0x08;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the STATUS command in the proprietary class A0 of
 * GSM 11.11.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) sec.9.2.2 (command),
 * sec.9.3 (coding), and 9.4 (status conditions).
 * @note Some SW1 and SW2 values are non-ISO since they originate from the
 * GSM 11.11 standard and seem to exist there and only there.
 */
static swicc_apduh_ft apduh_gsm_status;
static swicc_ret_et apduh_gsm_status(swicc_st *const swicc_state,
                                     swicc_apdu_cmd_st const *const cmd,
                                     swicc_apdu_res_st *const res,
                                     uint32_t const procedure_count)
{
    swsim_st *const swsim_state = (swsim_st *)swicc_state->userdata;
    if (swsim_state == NULL)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    if (cmd->hdr->p1 != 0 || cmd->hdr->p2 != 0)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Prepare the response body. */
    memset(res->data.b, 0x00, *cmd->p3);

    /* Get the SELECT response for the currently selected folder. */
    uint16_t select_res_len = sizeof(res->data);
    if (gsm_select_res(&swicc_state->fs, swicc_state->fs.va.cur_tree,
                       &swicc_state->fs.va.cur_df, res->data.b,
                       &select_res_len) != 0 ||
        select_res_len > UINT8_MAX)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    res->sw1 = SWICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    /* Safe cast due to check in if for GSM SELECT response creation. */
    res->data.len = *cmd->p3;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the SELECT command in the proprietary classes 0X, 4X, and 6X of
 * ETSI TS 102 221 V16.4.0.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in 3GPP 31.101 V17.0.0 pg.19 sec.11.1.1.
 * (and ETSI TS 102 221 V16.4.0 pg.84 sec.11.1.1.)
 */
static swicc_apduh_ft apduh_3gpp_select;
static swicc_ret_et apduh_3gpp_select(swicc_st *const swicc_state,
                                      swicc_apdu_cmd_st const *const cmd,
                                      swicc_apdu_res_st *const res,
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
    swicc_fs_occ_et occ = SWICC_FS_OCC_FIRST;
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
            occ = SWICC_FS_OCC_FIRST;
            break;
        case 0b00000001:
            occ = SWICC_FS_OCC_LAST;
            break;
        case 0b00000010:
            occ = SWICC_FS_OCC_NEXT;
            break;
        case 0b00000011:
            occ = SWICC_FS_OCC_PREV;
            break;
        default:
            /* RFU */
            occ_rfu = true;
            break;
        }
    }

    /**
     * 1. When P1 = 0x00, P3 = 0, P2 shall be 0x0C.
     * 2. b1 and b2 of P2 shall be 0 when P1 != 0x04.
     * 3. ADF occurrence shall not be RFU.
     */
    if ((cmd->hdr->p1 == 0x00 && *cmd->p3 == 0U && cmd->hdr->p2 != 0x0C) ||
        (cmd->hdr->p1 != 0x04 && (cmd->hdr->p2 & 0b00000011) != 0U) ||
        occ_rfu == true)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
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
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /**
         * If Lc is 0 it means data is absent so we can process what we got,
         * otherwise we need more from the interface.
         */
        if (*cmd->p3 > 0)
        {
            res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return SWICC_RET_SUCCESS;
        }
    }

    /**
     * The ACK ALL procedure was sent and we expected to receive all the data
     * (length of which was given in P3) but did not receive the expected amount
     * of data.
     */
    if (cmd->data->len != *cmd->p3 && procedure_count >= 1U)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x02; /* "Wrong length" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Perform the requested action. */
    {
        /* Unsupported P1/P2 parameters. */
        if (meth == METH_RFU || data_req == DATA_REQ_RFU ||
            meth == METH_DF_NESTED || meth == METH_DF_PARENT)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_P1P2;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        swicc_ret_et ret_select = SWICC_RET_ERROR;
        switch (meth)
        {
        case METH_FID:
            /* Must contain exactly 1 file ID. */
            if (cmd->data->len != sizeof(swicc_fs_id_kt))
            {
                ret_select = SWICC_RET_ERROR;
            }
            else
            {
                swicc_fs_id_kt const fid = be16toh(*(uint16_t *)cmd->data->b);
                ret_select = swicc_va_select_file_id(&swicc_state->fs, fid);
            }
            break;
        case METH_DF_NAME:
            /* Check if maybe trying to select an ADF. */
            if (cmd->data->len > SWICC_FS_ADF_AID_LEN ||
                cmd->data->len < SWICC_FS_ADF_AID_RID_LEN ||
                occ != SWICC_FS_OCC_FIRST)
            {
                ret_select = SWICC_RET_ERROR;
            }
            else
            {
                ret_select = swicc_va_select_adf(&swicc_state->fs, cmd->data->b,
                                                 cmd->data->len -
                                                     SWICC_FS_ADF_AID_RID_LEN);
                if (ret_select == SWICC_RET_FS_NOT_FOUND)
                {
                    ret_select = swicc_va_select_file_dfname(
                        &swicc_state->fs, cmd->data->b, cmd->data->len);
                }
            }
            break;
        case METH_PATH_MF:
            /* Must contain at least 1 ID in the path. */
            if (cmd->data->len < sizeof(swicc_fs_id_kt) ||
                occ != SWICC_FS_OCC_FIRST)
            {
                ret_select = SWICC_RET_ERROR;
            }
            else
            {
                swicc_fs_path_st const path = {
                    .b = cmd->data->b,
                    .len = cmd->data->len,
                    .type = SWICC_FS_PATH_TYPE_MF,
                };
                ret_select = swicc_va_select_file_path(&swicc_state->fs, path);
            }
            break;
        case METH_PATH_DF:
            /* Must contain at least 1 ID in the path. */
            if (cmd->data->len < sizeof(swicc_fs_id_kt) ||
                occ != SWICC_FS_OCC_FIRST)
            {
                ret_select = SWICC_RET_ERROR;
            }
            else
            {
                swicc_fs_path_st const path = {
                    .b = cmd->data->b,
                    .len = cmd->data->len,
                    .type = SWICC_FS_PATH_TYPE_DF,
                };
                ret_select = swicc_va_select_file_path(&swicc_state->fs, path);
            }
            break;
        default:
            /**
             * Will never get here because param values have been rejected if
             * unsupported.
             */
            __builtin_unreachable();
        }

        if (ret_select == SWICC_RET_FS_NOT_FOUND)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
            res->sw2 = 0x82; /* "Not found" */
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        else if (ret_select != SWICC_RET_SUCCESS)
        {
            /* Failed to select. */
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        if (data_req == DATA_REQ_ABSENT)
        {
            res->sw1 = SWICC_APDU_SW1_NORM_NONE;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        else
        {
            /* The file that was requested to be selected. */
            swicc_fs_file_st *const file_selected =
                &swicc_state->fs.va.cur_file;

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
                // 0x8C, /* '62': Security attributes (compact). */
                // 0xAB, /* '62': Security attributes (expanded). */
            };
            static uint32_t const tags_count = sizeof(tags) / sizeof(tags[0U]);
            swicc_dato_bertlv_tag_st bertlv_tags[tags_count];
            for (uint8_t tag_idx = 0U; tag_idx < tags_count; ++tag_idx)
            {
                if (swicc_dato_bertlv_tag_create(&bertlv_tags[tag_idx],
                                                 tags[tag_idx]) !=
                    SWICC_RET_SUCCESS)
                {
                    res->sw1 = SWICC_APDU_SW1_CHER_UNK;
                    res->sw2 = 0U;
                    res->data.len = 0U;
                    return SWICC_RET_SUCCESS;
                }
            }

            /* Create data for BER-TLV DOs. */

            uint32_t const data_size_be = htobe32(file_selected->data_size);
            uint32_t const data_size_tot_be =
                htobe32(file_selected->hdr_item.size);
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
                htobe32(UINT32_MAX - swicc_state->fs.va.cur_tree->len);
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
            uint8_t descr_be[SWICC_FS_FILE_DESCR_LEN_MAX];
            uint8_t descr_len = 0U;
            if (swicc_fs_file_lcs(file_selected, &lcs_be) !=
                    SWICC_RET_SUCCESS ||
                swicc_fs_file_descr(swicc_state->fs.va.cur_tree, file_selected,
                                    descr_be, &descr_len) != SWICC_RET_SUCCESS)
            {
                res->sw1 = SWICC_APDU_SW1_CHER_UNK;
                res->sw2 = 0U;
                res->data.len = 0U;
                return SWICC_RET_SUCCESS;
            }
            uint8_t data_aid[SWICC_FS_ADF_AID_LEN];
            if (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF)
            {
                memcpy(file_selected->hdr_spec.adf.aid.rid, data_aid,
                       SWICC_FS_ADF_AID_RID_LEN);
                memcpy(file_selected->hdr_spec.adf.aid.pix,
                       &data_aid[SWICC_FS_ADF_AID_RID_LEN],
                       SWICC_FS_ADF_AID_PIX_LEN);
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
                    if (enc.len > sizeof(res->data.b))
                    {
                        break;
                    }
                    bertlv_len = enc.len;
                    bertlv_buf = res->data.b;
                }

                swicc_dato_bertlv_enc_init(&enc, bertlv_buf, bertlv_len);
                swicc_dato_bertlv_enc_st enc_fcp;
                swicc_dato_bertlv_enc_st enc_prop_info;
                swicc_dato_bertlv_enc_st enc_pin_status;

                if (swicc_dato_bertlv_enc_nstd_start(&enc, &enc_fcp) !=
                        SWICC_RET_SUCCESS ||

                    /* Short file ID (SFI). */
                    (SWICC_FS_FILE_EF_CHECK(file_selected)
                         ? (swicc_dato_bertlv_enc_data(&enc_fcp, &data_sid,
                                                       sizeof(data_sid)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_fcp,
                                                      &bertlv_tags[9U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* Total file size. */
                    swicc_dato_bertlv_enc_data(
                        &enc_fcp, (uint8_t *)&data_size_tot_be,
                        sizeof(data_size_tot_be)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[2U]) !=
                        SWICC_RET_SUCCESS ||

                    /* File size for EFs. */
                    (SWICC_FS_FILE_EF_CHECK(file_selected)
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_fcp, (uint8_t *)&data_size_be,
                                sizeof(data_size_be)) != SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_fcp,
                                                      &bertlv_tags[1U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* PIN status for non-EFs. */
                    (SWICC_FS_FILE_FOLDER_CHECK(file_selected)
                         ? (swicc_dato_bertlv_enc_nstd_start(&enc_fcp,
                                                             &enc_pin_status) !=
                                SWICC_RET_SUCCESS ||

                            /* Key reference 1. */
                            swicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_key_ref[0U],
                                sizeof(data_pin_status_key_ref[0U])) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                      &bertlv_tags[4U]) !=
                                SWICC_RET_SUCCESS ||

                            /* Usage qualifier 1. */
                            swicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_usage_qualif[0U],
                                sizeof(data_pin_status_usage_qualif[0U])) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                      &bertlv_tags[13U]) !=
                                SWICC_RET_SUCCESS ||

                            /* PS_DO. */
                            swicc_dato_bertlv_enc_data(
                                &enc_pin_status,
                                (uint8_t *)&data_pin_status_ps_do,
                                sizeof(data_pin_status_ps_do)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_pin_status,
                                                      &bertlv_tags[12U]) !=
                                SWICC_RET_SUCCESS ||

                            swicc_dato_bertlv_enc_nstd_end(&enc_fcp,
                                                           &enc_pin_status) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_fcp,
                                                      &bertlv_tags[15U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* Security attributes (reference). */
                    swicc_dato_bertlv_enc_data(
                        &enc_fcp, (uint8_t *)&data_sec_attr,
                        sizeof(data_sec_attr)) != SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[11U]) !=
                        SWICC_RET_SUCCESS ||

                    /* Life cycle status. */
                    swicc_dato_bertlv_enc_data(&enc_fcp, &lcs_be,
                                               sizeof(lcs_be)) !=
                        SWICC_RET_SUCCESS ||
                    swicc_dato_bertlv_enc_hdr(&enc_fcp, &bertlv_tags[10U]) !=
                        SWICC_RET_SUCCESS ||

                    /* Proprietary info. */
                    swicc_dato_bertlv_enc_nstd_start(
                        &enc_fcp, &enc_prop_info) != SWICC_RET_SUCCESS ||

                    /* Specific UICC environmental conditions for MF.  */
                    (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_MF
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_uicc_env_cond,
                                sizeof(data_uicc_env_cond)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[9U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* Supported system commands for folders. */
                    (SWICC_FS_FILE_FOLDER_CHECK(file_selected)
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_sys_cmd_support,
                                sizeof(data_sys_cmd_support)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[8U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /**
                     * File details, reserved file size, and maximum file size
                     * for BER-TLV EFs.
                     */
                    (SWICC_FS_FILE_EF_BERTLV_CHECK(file_selected)
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_file_size_max_be,
                                sizeof(data_file_size_max_be)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[7U]) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_file_size_reserved_be,
                                sizeof(data_file_size_reserved_be)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[6U]) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_file_details,
                                sizeof(data_file_details)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[5U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* Total memory available for folders and BER-TLV EFs. */
                    ((SWICC_FS_FILE_FOLDER_CHECK(file_selected) ||
                      SWICC_FS_FILE_EF_BERTLV_CHECK(file_selected))
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info,
                                (uint8_t *)&data_mem_available_be,
                                sizeof(data_mem_available_be)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[4U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* App power consumption and min app clock for ADF. */
                    (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_ADF
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_app_clk_min,
                                sizeof(data_app_clk_min)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[3U]) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_app_power_cons,
                                sizeof(data_app_power_cons)) !=
                                SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[2U]) !=
                                SWICC_RET_SUCCESS)
                         : false) ||

                    /* UICC characteristics for MF. */
                    (file_selected->hdr_item.type == SWICC_FS_ITEM_TYPE_FILE_MF
                         ? (swicc_dato_bertlv_enc_data(
                                &enc_prop_info, (uint8_t *)&data_uicc_char,
                                sizeof(data_uicc_char)) != SWICC_RET_SUCCESS ||
                            swicc_dato_bertlv_enc_hdr(&enc_prop_info,
                                                      &bertlv_tags[1U]) !=
                                SWICC_RET_SUCCESS)
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
                            swicc_dato_bertlv_enc_hdr(&enc_fcp,
                                                      &bertlv_tags[5U]) !=
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

            if (ret_bertlv == SWICC_RET_SUCCESS &&
                swicc_apdu_rc_enq(&swicc_state->apdu_rc, res->data.b,
                                  bertlv_len) == SWICC_RET_SUCCESS)
            {
                res->sw1 = SWICC_APDU_SW1_NORM_BYTES_AVAILABLE;
                /**
                 * Make sure to fail when extended APDUs are used since they are
                 * unsupported here.
                 */
                static_assert(SWICC_DATA_MAX == SWICC_DATA_MAX_SHRT,
                              "Response buffer length might not fit in SW2 "
                              "if SW1 is 0x61");
                /* Safe cast due to check inside the BER-TLV loop. */
                res->sw2 = (uint8_t)bertlv_len;
                res->data.len = 0U;
                return SWICC_RET_SUCCESS;
            }
            else
            {
                res->sw1 = SWICC_APDU_SW1_CHER_UNK;
                res->sw2 = 0U;
                res->data.len = 0U;
                return SWICC_RET_SUCCESS;
            }
        }
    }
}

/**
 * @brief Handle the TERMINAL PROFILE command in the proprietary class 0x80 of
 * ETSI TS 102 221 V16.4.0.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in and ETSI TS 102 221 V16.4.0 pg.133 sec.11.2.1
 */
static swicc_apduh_ft apduh_etsi_terminal_profile;
static swicc_ret_et apduh_etsi_terminal_profile(
    swicc_st *const swicc_state, swicc_apdu_cmd_st const *const cmd,
    swicc_apdu_res_st *const res, uint32_t const procedure_count)
{
    /* Make sure the command is valid. */
    if (cmd->hdr->p1 != 0x00 || cmd->hdr->p2 != 0x00)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
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
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /**
         * If Lc is 0 it means data is absent so we can process what we got,
         * otherwise we need more from the interface.
         */
        if (*cmd->p3 > 0)
        {
            res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return SWICC_RET_SUCCESS;
        }
    }

    /* Terminal profile is ignored. */
    res->sw1 = SWICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    res->data.len = 0U;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the STATUS command in the proprietary classes 0x8X, 0xCX, 0xEx
 * of ETSI TS 102 221 V16.4.0.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in 3GPP 31.101 V17.0.0 pg.20 sec.11.1.2
 * (and ETSI TS 102 221 V16.4.0 pg.95 sec.11.1.2)
 */
static swicc_apduh_ft apduh_3gpp_status;
static swicc_ret_et apduh_3gpp_status(swicc_st *const swicc_state,
                                      swicc_apdu_cmd_st const *const cmd,
                                      swicc_apdu_res_st *const res,
                                      uint32_t const procedure_count)
{
    enum app_info_e
    {
        APP_INFO_RFU,
        APP_INFO_NONE,
        APP_INFO_INIT,
        APP_INFO_DEINIT,
    } app_info = APP_INFO_RFU;

    enum data_req_e
    {
        DATA_REQ_RFU,
        DATA_REQ_SELECT,
        DATA_REQ_DF_NAME,
        DATA_REQ_NONE,
    } data_req = DATA_REQ_RFU;

    switch (cmd->hdr->p1)
    {
    case 0b00000000:
        app_info = APP_INFO_NONE;
        break;
    case 0b00000001:
        app_info = APP_INFO_INIT;
        break;
    case 0b00000010:
        app_info = APP_INFO_DEINIT;
        break;
    default:
        app_info = APP_INFO_RFU;
        break;
    }

    switch (cmd->hdr->p2)
    {
    case 0b00000000:
        data_req = DATA_REQ_SELECT;
        break;
    case 0b00000001:
        data_req = DATA_REQ_DF_NAME;
        break;
    case 0b00001100:
        data_req = DATA_REQ_NONE;
        break;
    default:
        data_req = DATA_REQ_RFU;
        break;
    }

    /* Make sure the command is valid. */
    if (cmd->data->len != 0 || *cmd->p3 != 0 || data_req == DATA_REQ_RFU ||
        app_info == APP_INFO_RFU)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    switch (data_req)
    {
    case DATA_REQ_NONE:
        res->sw1 = SWICC_APDU_SW1_NORM_NONE;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
        break;
    case DATA_REQ_SELECT:
    case DATA_REQ_DF_NAME:
    default:
        /* Unsupported params. */
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
        break;
    }
}

/**
 * @brief Handle the UNBLOCK PIN command in the proprietary classes 0x0X, 0x4X,
 * 0x6X of ETSI TS 102 221 V16.4.0.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in 3GPP 31.101 V17.0.0 pg.20 sec.11.1.13
 * (and ETSI TS 102 221 V16.4.0 pg.106 sec.11.1.13)
 */
static swicc_apduh_ft apduh_3gpp_pin_unblock;
static swicc_ret_et apduh_3gpp_pin_unblock(swicc_st *const swicc_state,
                                           swicc_apdu_cmd_st const *const cmd,
                                           swicc_apdu_res_st *const res,
                                           uint32_t const procedure_count)
{
    enum ref_data_e
    {
        REF_DATA_RFU,
        REF_DATA_GLOBAL,
        REF_DATA_SPECIFIC,
    } __attribute__((unused)) ref_data = REF_DATA_RFU;

    /* Parse P2. */
    __attribute__((unused)) uint8_t const ref_data_num =
        cmd->hdr->p2 & 0b00011111;
    if (cmd->hdr->p2 & 0b10000000)
    {
        ref_data = REF_DATA_SPECIFIC;
    }
    else
    {
        ref_data = REF_DATA_GLOBAL;
    }

    /* Make sure the command is valid. */
    if (cmd->hdr->p1 != 0U || (cmd->hdr->p2 & 0b01100000) != 0U ||
        cmd->hdr->p2 == 0U)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }
    /* Length can either be '00' or '10'. */
    if (!(*cmd->p3 == 0x00 || *cmd->p3 == 0x10))
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x00; /* "Wrong length." */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    if (procedure_count == 0U)
    {
        /**
         * Unexpected because before sending a procedure, no data should have
         * been received.
         */
        if (cmd->data->len != 0U)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* If the command has data, send a procedure. */
        if (*cmd->p3 > 0)
        {
            res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return SWICC_RET_SUCCESS;
        }
    }

    if (procedure_count >= 1 && cmd->data->len != *cmd->p3)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x00; /* "Wrong length." */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Perform the requested operation. */
    {
        /**
         * @todo Implement the exact logic described in sec.11.1.13.
         */

        /* When empty, used to acquire the retry counter. */
        if (*cmd->p3 == 0x00)
        {
            /**
             * 10 is the default (there is no PIN so just sending back a
             * default).
             */
            uint8_t const retries = 10U;
            res->sw1 = SWICC_APDU_SW1_WARN_NVM_CHGM;
            res->sw2 =
                0xC0 | retries; /* Indicates how many tries are remaining. */
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        /* Received both an UNBLOCK PIN and a new PIN. */
        else
        {
            /**
             * @todo Handle this case.
             */
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
    }
}

/**
 * @brief Handle the VERIFY PIN command in the proprietary classes 0x0X, 0x4X,
 * and 0x6X of ETSI TS 102 221 V16.4.0.
 * @param swicc_state
 * @param cmd
 * @param res
 * @param procedure_count
 * @return Return code.
 * @note As described in 3GPP 31.101 V17.0.0 pg.20 sec.11.1.9
 * (and ETSI TS 102 221 V16.4.0 pg.102 sec.11.1.9)
 */
static swicc_apduh_ft apduh_3gpp_pin_verify;
static swicc_ret_et apduh_3gpp_pin_verify(swicc_st *const swicc_state,
                                          swicc_apdu_cmd_st const *const cmd,
                                          swicc_apdu_res_st *const res,
                                          uint32_t const procedure_count)
{
    enum ref_data_e
    {
        REF_DATA_RFU,
        REF_DATA_GLOBAL,
        REF_DATA_SPECIFIC,
    } __attribute__((unused)) ref_data = REF_DATA_RFU;

    /* Make sure the command is valid. */
    if (cmd->hdr->p1 != 0U)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_P1P2_INFO;
        res->sw2 = 0x86; /* "Incorrect parameters P1 to P2" */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }
    /* Length can either be '00' or '08'. */
    if (!(*cmd->p3 == 0x00 || *cmd->p3 == 0x08))
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x00; /* "Wrong length." */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    if (procedure_count == 0U)
    {
        /**
         * Unexpected because before sending a procedure, no data should have
         * been received.
         */
        if (cmd->data->len != 0U)
        {
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* If the command has data, send a procedure. */
        if (*cmd->p3 > 0)
        {
            res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
            res->sw2 = 0U;
            res->data.len = *cmd->p3; /* Length of expected data. */
            return SWICC_RET_SUCCESS;
        }
    }

    if (procedure_count >= 1 && cmd->data->len != *cmd->p3)
    {
        res->sw1 = SWICC_APDU_SW1_CHER_LEN;
        res->sw2 = 0x00; /* "Wrong length." */
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }

    /* Perform the requested operation. */
    {
        /**
         * @todo Implement the exact logic described in sec.11.1.9.
         */
        if (*cmd->p3 == 0x00)
        {
            /* There is no PIN so sending back a default value of 3. */
            uint8_t const retries = 3;
            res->sw1 = SWICC_APDU_SW1_WARN_NVM_CHGM;
            res->sw2 = 0xC0 | retries;
            res->data.len = 0U;
        }
        else
        {
            /**
             * @todo Handle this case.
             */
            res->sw1 = SWICC_APDU_SW1_CHER_UNK;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }
        return SWICC_RET_SUCCESS;
    }

    return SWICC_RET_UNKNOWN;
}

swicc_ret_et sim_apduh_demux(swicc_st *const swicc_state,
                             swicc_apdu_cmd_st const *const cmd,
                             swicc_apdu_res_st *const res,
                             uint32_t const procedure_count)
{
    swicc_ret_et ret = SWICC_RET_APDU_UNHANDLED;
    switch (cmd->hdr->cla.type)
    {
    case SWICC_APDU_CLA_TYPE_INTERINDUSTRY:
        switch (cmd->hdr->ins)
        {
        case 0xA4: /* SELECT */
            /**
             * Override the default SELECT because the proprietary one supports
             * fewer/different features and responds with proprietary BER-TLV
             * tags.
             */
            ret = apduh_3gpp_select(swicc_state, cmd, res, procedure_count);
            break;
        case 0x2C: /* RESET RETRY COUNTER */
            /**
             * Override the default RESET RETRY COUNTER instruction with am
             * UNBLOCK PIN.
             */
            ret =
                apduh_3gpp_pin_unblock(swicc_state, cmd, res, procedure_count);
            break;
        case 0x20: /* VERIFY */
            /**
             * Override the default VERIFY instruction with a VERIFY PIN.
             */
            ret = apduh_3gpp_pin_verify(swicc_state, cmd, res, procedure_count);
        default:
            break;
        }
        break;
    case SWICC_APDU_CLA_TYPE_PROPRIETARY:
        /* ETSI + 3GPP + GSM */
        if (cmd->hdr->ins != 0xC0) /* GET RESPONSE instruction */
        {
            /* Make GET RESPONSE deterministically not work if resumed. */
            swicc_apdu_rc_reset(&swicc_state->apdu_rc);
        }

        /* The swICC does not parse proprietary CLAs beyond just the type. */
        cmd->hdr->cla = sim_apdu_cmd_cla_parse(cmd->hdr->cla.raw);

        /**
         * The constraints on what instructions live in what CLA is described in
         * ETSI TS 102 221 V16.4.0 pg.77 sec.10.1.2 table.10.5.
         */
        switch (cmd->hdr->ins)
        {
        case 0xA4: /* SELECT */
            /* ETSI + 3GPP */
            if ((cmd->hdr->cla.raw & 0xF0) == 0x00 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x40 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x60)
            {
                ret = apduh_3gpp_select(swicc_state, cmd, res, procedure_count);
            }
            /* GSM */
            else if (cmd->hdr->cla.raw == 0xA0)
            {
                ret = apduh_gsm_select(swicc_state, cmd, res, procedure_count);
            }
            break;
        case 0x10: /* TERMINAL PROFILE */
            /* ETSI */
            if (cmd->hdr->cla.raw == 0x80)
            {
                ret = apduh_etsi_terminal_profile(swicc_state, cmd, res,
                                                  procedure_count);
            }
            break;
        case 0xC0: /* GET RESPONSE */
            /* GSM */
            if (cmd->hdr->cla.raw == 0xA0)
            {
                ret = apduh_gsm_res_get(swicc_state, cmd, res, procedure_count);
            }
            break;
        case 0xB0: /* READ BINARY */
            /* GSM */
            if (cmd->hdr->cla.raw == 0xA0)
            {
                ret =
                    apduh_gsm_bin_read(swicc_state, cmd, res, procedure_count);
            }
            break;
        case 0xF2: /* STATUS */
            /* ETSI + 3GPP */
            if ((cmd->hdr->cla.raw & 0xF0) == 0x80 ||
                (cmd->hdr->cla.raw & 0xF0) == 0xC0 ||
                (cmd->hdr->cla.raw & 0xF0) == 0xE0)
            {
                ret = apduh_3gpp_status(swicc_state, cmd, res, procedure_count);
            }
            /* GSM */
            else if (cmd->hdr->cla.raw == 0xA0)
            {
                ret = apduh_gsm_status(swicc_state, cmd, res, procedure_count);
            }
            break;
        case 0x2C: /* UNBLOCK PIN */
            if ((cmd->hdr->cla.raw & 0xF0) == 0x00 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x40 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x60)
            {
                ret = apduh_3gpp_pin_unblock(swicc_state, cmd, res,
                                             procedure_count);
            }
            break;
        case 0x20:
            if ((cmd->hdr->cla.raw & 0xF0) == 0x00 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x40 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x60)
            {
                ret = apduh_3gpp_pin_verify(swicc_state, cmd, res,
                                            procedure_count);
            }
        default:
            break;
        }
        break;
    default:
        break;
    }
    return ret;
}

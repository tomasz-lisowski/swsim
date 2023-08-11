#include "apduh.h"
#include "3gpp.h"
#include "apdu.h"
#include "fs.h"
#include "gsm.h"
#include "proactive.h"
#include "swicc/apdu.h"
#include "swicc/common.h"
#include "swsim.h"
#include <endian.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <swicc/fs/common.h>

/**
 * @brief Handle the SELECT command in the proprietary class A0 of GSM 11.11.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) clause.9.2.1 (command),
 * clause.9.3 (coding), and 9.4 (status conditions).
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
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) clause.9.2.18
 * (command), clause.9.3 (coding), and 9.4 (status conditions).
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
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) clause.9.2.3 (command),
 * clause.9.3 (coding), and 9.4 (status conditions).
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
    uint16_t const offset =
        le16toh((uint16_t)((uint16_t)(offset_hi << 8U) | offset_lo));

    swicc_fs_file_st *const file = &swicc_state->fs.va.cur_file;
    switch (file->hdr_item.type)
    {
    /**
     * GSM 11.11 v4.21.1 clause.8 table.8 indicates that READ BINARY shall
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
        else if (offset + len_expected > file->data_size)
        {
            res->sw1 =
                SWICC_APDU_SW1_CHER_P1P2; /* "Incorrect parameter P1 or P2." */
            res->sw2 = 0x00;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        /* Copy the file data to the response. */
        memcpy(res->data.b, file->data, len_expected);

        /* Send back the data. */
        res->sw1 = 0x90;
        res->sw2 = 0x00;
        res->data.len = len_expected;
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
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) clause.9.2.2 (command),
 * clause.9.3 (coding), and 9.4 (status conditions).
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
    /* Safe cast due to check for GSM SELECT response creation. */
    res->data.len = *cmd->p3;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the RUN GSM ALGORITHM command in the proprietary class A0 of
 * GSM 11.11.
 * @note As described in GSM 11.11 v4.21.1 (ETS 300 608) clause.9.2.16
 * (command), clause.9.3 (coding), and 9.4 (status conditions).
 * @note Some SW1 and SW2 values are non-ISO since they originate from the
 * GSM 11.11 standard and seem to exist there and only there.
 */
static swicc_apduh_ft apduh_gsm_gsm_algo_run;
static swicc_ret_et apduh_gsm_gsm_algo_run(swicc_st *const swicc_state,
                                           swicc_apdu_cmd_st const *const cmd,
                                           swicc_apdu_res_st *const res,
                                           uint32_t const procedure_count)
{
    swsim_st *const swsim_state = (swsim_st *)swicc_state->userdata;
    if (swsim_state == NULL)
    {
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }

    /* Validate command parameters. */
    if (cmd->hdr->p1 != 0 || cmd->hdr->p2 != 0)
    {
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }
    uint8_t const data_len_exp = 0x10;
    if (*cmd->p3 != data_len_exp)
    {
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }

    /* Expecting to receive a random data. */
    if (procedure_count == 0U)
    {
        /* Make sure no data is received before first procedure is sent. */
        if (cmd->data->len != 0)
        {
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }

        SWICC_APDUH_RES(res, SWICC_APDU_SW1_PROC_ACK_ALL, 0U, data_len_exp);
        return SWICC_RET_SUCCESS;
    }

    /**
     * The ACK ALL procedure was sent and we expected to receive all the data
     * but did not receive the expected amount of data.
     */
    if (procedure_count >= 1U && cmd->data->len != data_len_exp)
    {
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }

    /* A3/A8 individual subscriber authentication key. */
    uint8_t const ki[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07,
    };

    gsm_algo(ki, cmd->data->b, res->data.b);
    res->data.len = 12U;
    /* Copy response to the GET RESPONSE buffer. */
    if (swicc_apdu_rc_enq(&swicc_state->apdu_rc, res->data.b, res->data.len) ==
        SWICC_RET_SUCCESS)
    {
        SWICC_APDUH_RES(res, 0x9F, 12U, 0U);
        return SWICC_RET_SUCCESS;
    }
    else
    {
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the SELECT command in the proprietary classes 0X, 4X, and 6X of
 * ETSI TS 102 221 V16.4.0.
 * @note As described in 3GPP 31.101 V17.0.0 clause.11.1.1.
 * (and ETSI TS 102 221 V16.4.0 clause.11.1.1.)
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
                /**
                 * Special FID reserved for current application. ETSI TS 102 221
                 * V16.4.0 clause.8.3.
                 */
                if (fid == 0x7FFF)
                {
                    if (swicc_state->fs.va.cur_adf.hdr_item.type ==
                        SWICC_FS_ITEM_TYPE_FILE_ADF)
                    {
                        /* Reselect ADF. */
                        uint8_t aid[SWICC_FS_ADF_AID_LEN];
                        memcpy(&aid[0U],
                               swicc_state->fs.va.cur_adf.hdr_spec.adf.aid.rid,
                               SWICC_FS_ADF_AID_RID_LEN);
                        memcpy(&aid[SWICC_FS_ADF_AID_RID_LEN],
                               swicc_state->fs.va.cur_adf.hdr_spec.adf.aid.pix,
                               SWICC_FS_ADF_AID_PIX_LEN);
                        ret_select = swicc_va_select_adf(
                            &swicc_state->fs, aid, SWICC_FS_ADF_AID_PIX_LEN);
                    }
                    else
                    {
                        ret_select = SWICC_RET_FS_NOT_FOUND;
                    }
                }
                else
                {
                    ret_select = swicc_va_select_file_id(&swicc_state->fs, fid);
                }
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
        case METH_PATH_DF:
            /* Must contain at least 1 ID in the path. */
            if (cmd->data->len < sizeof(swicc_fs_id_kt) ||
                occ != SWICC_FS_OCC_FIRST || cmd->data->len % 2 != 0)
            {
                ret_select = SWICC_RET_ERROR;
            }
            else
            {
                swicc_fs_path_st const path = {
                    .b = (swicc_fs_id_kt *)cmd->data->b,
                    .len = cmd->data->len / 2,
                    .type = meth == METH_PATH_MF ? SWICC_FS_PATH_TYPE_MF
                                                 : SWICC_FS_PATH_TYPE_DF,
                };
                /* Convert path to host endianness. */
                for (uint32_t path_idx = 0U; path_idx < path.len; ++path_idx)
                {
                    path.b[path_idx] = be16toh(path.b[path_idx]);
                }
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
            /**
             * Make sure to fail when extended APDUs are used since they are
             * unsupported here.
             */
            static_assert(
                SWICC_DATA_MAX == SWICC_DATA_MAX_SHRT,
                "Response buffer length might not fit in SW2 if SW1 is 0x61");

            /* The file that was requested to be selected. */
            swicc_fs_file_st *const file_selected =
                &swicc_state->fs.va.cur_file;
            uint16_t buf_select_len = sizeof(res->data.b);
            int32_t const ret_select_res =
                o3gpp_select_res(&swicc_state->fs, swicc_state->fs.va.cur_tree,
                                 file_selected, res->data.b, &buf_select_len);
            if (ret_select_res == 0 && buf_select_len <= UINT8_MAX &&
                swicc_apdu_rc_enq(&swicc_state->apdu_rc, res->data.b,
                                  buf_select_len) == SWICC_RET_SUCCESS)
            {
                /* Safe cast due to check in the 'if'. */
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_NORM_BYTES_AVAILABLE,
                                (uint8_t)buf_select_len, 0U);
                return SWICC_RET_SUCCESS;
            }
            else
            {
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
                return SWICC_RET_SUCCESS;
            }
        }
    }
}

/**
 * @brief Handle the TERMINAL PROFILE command in the proprietary class 0x80 of
 * ETSI TS 102 221 V16.4.0.
 * @note As described in and ETSI TS 102 221 V16.4.0 clause.11.2.1
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
 * @brief Handle the FETCH command in the proprietary class 0x80 of
 * ETSI TS 102 221 V16.4.0.
 * @note As described in and ETSI TS 102 221 V16.4.0 clause.11.2.3
 */
static swicc_apduh_ft apduh_etsi_cat_fetch;
static swicc_ret_et apduh_etsi_cat_fetch(swicc_st *const swicc_state,
                                         swicc_apdu_cmd_st const *const cmd,
                                         swicc_apdu_res_st *const res,
                                         uint32_t const procedure_count)
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
        swsim_st *const swsim_state = swicc_state->userdata;
        if (*cmd->p3 != swsim_state->proactive.command_length)
        {
            /* Expected Le to be the exact length of the command. */
            res->sw1 = SWICC_APDU_SW1_CHER_LE;
            res->sw2 = 0U;
            res->data.len = 0U;
            return SWICC_RET_SUCCESS;
        }

        memcpy(res->data.b, swsim_state->proactive.command,
               swsim_state->proactive.command_length);

        res->sw1 = SWICC_APDU_SW1_NORM_NONE;
        res->sw2 = 0U;
        res->data.len = swsim_state->proactive.command_length;
        swsim_state->proactive.command_length = 0;
        return SWICC_RET_SUCCESS;
    }
    else
    {
        /* Unexpected. */
        res->sw1 = SWICC_APDU_SW1_CHER_UNK;
        res->sw2 = 0U;
        res->data.len = 0U;
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the TERMINAL RESPONSE command in the proprietary class 0x80 of
 * ETSI TS 102 221 V16.4.0.
 * @note As described in and ETSI TS 102 221 V16.4.0 clause.11.2.4.
 */
static swicc_apduh_ft apduh_etsi_cat_terminal_response;
static swicc_ret_et apduh_etsi_cat_terminal_response(
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
        /* Get response from the terminal. */
        res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
        res->sw2 = 0U;
        res->data.len = *cmd->p3; /* Length of expected data. */
        return SWICC_RET_SUCCESS;
    }

    swsim_st *const swsim_state = swicc_state->userdata;
    if (swsim_state->proactive.app_default_response_wait)
    {
        /* Give response to default app. */
        swsim_state->proactive.app_default_response_wait = false;
        /**
         * TODO: Copy resoonse to struct.
         */
    }
    else
    {
        /* Didn't expect a response but got it anyways? */
    }

    res->sw1 = SWICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    res->data.len = 0U;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the ENVELOPE command in the proprietary class 0x80 of
 * ETSI TS 102 221 V16.4.0.
 * @note As described in and ETSI TS 102 221 V16.4.0 clause.11.2.2.
 */
static swicc_apduh_ft apduh_etsi_cat_envelope;
static swicc_ret_et apduh_etsi_cat_envelope(swicc_st *const swicc_state,
                                            swicc_apdu_cmd_st const *const cmd,
                                            swicc_apdu_res_st *const res,
                                            uint32_t const procedure_count)
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
        /* Get response from the terminal. */
        res->sw1 = SWICC_APDU_SW1_PROC_ACK_ALL;
        res->sw2 = 0U;
        res->data.len = *cmd->p3; /* Length of expected data. */
        return SWICC_RET_SUCCESS;
    }

    swsim_st *const swsim_state = swicc_state->userdata;
    swsim_state->proactive.envelope_length = cmd->data->len;
    memcpy(swsim_state->proactive.envelope, cmd->data->b, cmd->data->len);

    /**
     * TODO: Return the BER-TLV object described in ETSI TS 102 223.
     */
    res->sw1 = SWICC_APDU_SW1_NORM_NONE;
    res->sw2 = 0U;
    res->data.len = 0U;
    return SWICC_RET_SUCCESS;
}

/**
 * @brief Handle the STATUS command in the proprietary classes 0x8X, 0xCX, 0xEx
 * of ETSI TS 102 221 V16.4.0.
 * @note As described in 3GPP 31.101 V17.0.0 clause.11.1.2
 * (and ETSI TS 102 221 V16.4.0 clause.11.1.2)
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
    if (cmd->data->len != 0 || data_req == DATA_REQ_RFU ||
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
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_NORM_NONE, 0U, 0U);
        return SWICC_RET_SUCCESS;
    case DATA_REQ_DF_NAME:
        memcpy(&res->data.b[0U],
               swicc_state->fs.va.cur_adf.hdr_spec.adf.aid.rid,
               SWICC_FS_ADF_AID_RID_LEN);
        memcpy(&res->data.b[SWICC_FS_ADF_AID_RID_LEN],
               swicc_state->fs.va.cur_adf.hdr_spec.adf.aid.pix,
               SWICC_FS_ADF_AID_PIX_LEN);
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_NORM_NONE, 0U, SWICC_FS_NAME_LEN);
        return SWICC_RET_SUCCESS;
    case DATA_REQ_SELECT: {
        memset(res->data.b, 0xFF, *cmd->p3);
        swicc_fs_file_st *const file_selected = &swicc_state->fs.va.cur_adf;
        uint16_t buf_select_len = sizeof(res->data.b);
        int32_t const ret_select_res =
            o3gpp_select_res(&swicc_state->fs, swicc_state->fs.va.cur_tree_adf,
                             file_selected, res->data.b, &buf_select_len);
        if (ret_select_res == 0 && buf_select_len <= UINT8_MAX &&
            swicc_apdu_rc_enq(&swicc_state->apdu_rc, res->data.b,
                              buf_select_len) == SWICC_RET_SUCCESS)
        {
            if (buf_select_len == *cmd->p3)
            {
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_NORM_NONE, 0U, *cmd->p3);
                return SWICC_RET_SUCCESS;
            }
            else
            {
                /* Requested wrong response length. */
                /* "Wrong Le field." */
                /* Safe cast due to check in 'if'. */
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LE,
                                (uint8_t)buf_select_len, 0U);
                return SWICC_RET_SUCCESS;
            }
        }
        else
        {
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }
    }
    default:
        /* Unsupported params. */
        /* "Incorrect parameters P1 to P2" */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2_INFO, 0x86, 0U);
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the UNBLOCK PIN command in the proprietary classes 0x0X, 0x4X,
 * 0x6X of ETSI TS 102 221 V16.4.0.
 * @note As described in 3GPP 31.101 V17.0.0 clause.11.1.13
 * (and ETSI TS 102 221 V16.4.0 clause.11.1.13)
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
        /* "Incorrect parameters P1 to P2" */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2_INFO, 0x86, 0U);
        return SWICC_RET_SUCCESS;
    }
    /* Length can either be '00' or '10'. */
    if (!(*cmd->p3 == 0x00 || *cmd->p3 == 0x10))
    {
        /* "Wrong length." */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0x00, 0U);
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
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }

        /* If the command has data, send a procedure. */
        if (*cmd->p3 > 0)
        {
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_PROC_ACK_ALL, 0U,
                            *cmd->p3 /* Length of expected data. */);
            return SWICC_RET_SUCCESS;
        }
    }

    if (procedure_count >= 1 && cmd->data->len != *cmd->p3)
    {
        /* "Wrong length." */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0x00, 0U);
        return SWICC_RET_SUCCESS;
    }

    /* Perform the requested operation. */
    {
        /**
         * @todo Implement the exact logic described in clause.11.1.13.
         */

        /* When empty, used to acquire the retry counter. */
        if (*cmd->p3 == 0x00)
        {
            /**
             * 10 is the default (there is no PIN so just sending back a
             * default).
             */
            uint8_t const retries = 10U;
            SWICC_APDUH_RES(
                res, SWICC_APDU_SW1_WARN_NVM_CHGM,
                0xC0 | retries /* Indicates how many tries are remaining. */,
                0U);
            return SWICC_RET_SUCCESS;
        }
        /* Received both an UNBLOCK PIN and a new PIN. */
        else
        {
            /**
             * @todo Handle this case.
             */
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }
    }
}

/**
 * @brief Handle the VERIFY PIN command in the proprietary classes 0x0X, 0x4X,
 * and 0x6X of ETSI TS 102 221 V16.4.0.
 * @note As described in 3GPP 31.101 V17.0.0 clause.11.1.9
 * (and ETSI TS 102 221 V16.4.0 clause.11.1.9)
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
        /* "Incorrect parameters P1 to P2" */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2_INFO, 0x86, 0U);
        return SWICC_RET_SUCCESS;
    }
    /* Length can either be '00' or '08'. */
    if (!(*cmd->p3 == 0x00 || *cmd->p3 == 0x08))
    {
        /* "Wrong length." */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0x00, 0U);
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
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }

        /* If the command has data, send a procedure. */
        if (*cmd->p3 > 0)
        {
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_PROC_ACK_ALL, 0U,
                            *cmd->p3 /* Length of expected data. */);
            return SWICC_RET_SUCCESS;
        }
    }

    if (procedure_count >= 1 && cmd->data->len != *cmd->p3)
    {
        /* "Wrong length." */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0x00, 0U);
        return SWICC_RET_SUCCESS;
    }

    /* Perform the requested operation. */
    {
        /**
         * @todo Implement the exact logic described in clause.11.1.9.
         */
        if (*cmd->p3 == 0x00)
        {
            /* There is no PIN so sending back a default value of 3. */
            uint8_t const retries = 3;
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_WARN_NVM_CHGM, 0xC0 | retries,
                            0U);
            return SWICC_RET_SUCCESS;
        }
        else
        {
            /**
             * @todo Handle this case.
             */
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the UPDATE BINARY command in the proprietary classes 0x0X,
 * 0x4X, and 0x6X of ETSI TS 102 221 V16.4.0.
 * @note As described in 3GPP 31.101 V17.0.0 clause.11.1.4
 * (and ETSI TS 102 221 V16.4.0 clause.11.1.4)
 */
static swicc_apduh_ft apduh_3gpp_bin_update;
static swicc_ret_et apduh_3gpp_bin_update(swicc_st *const swicc_state,
                                          swicc_apdu_cmd_st const *const cmd,
                                          swicc_apdu_res_st *const res,
                                          uint32_t const procedure_count)
{
    if ((cmd->hdr->p1 & 0b10000000) == 0b10000000 &&
        (cmd->hdr->p1 & 0b01100000) != 0)
    {
        /* P1 is invalid. */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2, 0U, 0U);
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
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
            return SWICC_RET_SUCCESS;
        }

        /* If the command has data, send a procedure. */
        if (*cmd->p3 > 0)
        {
            SWICC_APDUH_RES(res, SWICC_APDU_SW1_PROC_ACK_ALL, 0U,
                            *cmd->p3 /* Length of expected data. */);
            return SWICC_RET_SUCCESS;
        }
    }

    if (procedure_count >= 1U && cmd->data->len != *cmd->p3)
    {
        /* Invalid length was given. */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }

    enum meth_e
    {
        METH_CUR,
        METH_SFI,
    } meth = METH_CUR;

    uint8_t offset_hi = 0U;
    uint8_t offset_lo = 0U;
    swicc_fs_sid_kt sid = 0x00;

    switch (cmd->hdr->p1 & 0b10000000)
    {
    case 0b00000000:
        meth = METH_CUR;
        offset_hi = cmd->hdr->p1 & 0b01111111;
        offset_lo = cmd->hdr->p2;
        break;
    case 0b10000000:
        meth = METH_SFI;
        sid = cmd->hdr->p1 & 0b00011111;
        offset_lo = cmd->hdr->p2;
        break;
    }

    /* Safe cast as just concatenating two offset bytes into a short. */
    uint16_t const offset =
        meth == METH_SFI
            ? offset_lo
            : be16toh((uint16_t)((uint16_t)(offset_hi << 8) | offset_lo));
    /* Perform requested operation. */
    {
        swicc_fs_file_st *file_edit = NULL;
        swicc_fs_file_st file_sid;

        switch (meth)
        {
        case METH_CUR:
            file_edit = &swicc_state->fs.va.cur_file;
            break;
        case METH_SFI: {
            if (swicc_disk_lutsid_lookup(swicc_state->fs.va.cur_tree, sid,
                                         &file_sid) != SWICC_RET_SUCCESS)
            {
                /* "File not found." */
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2_INFO, 0x82, 0U);
                return SWICC_RET_SUCCESS;
            }
            file_edit = &file_sid;
            break;
        }
        }

        if (file_edit != NULL)
        {
            if (cmd->data->len <= file_edit->data_size)
            {
                if (offset + cmd->data->len <= file_edit->data_size)
                {
                    memcpy(&file_edit->data[offset], cmd->data->b,
                           cmd->data->len);
                    SWICC_APDUH_RES(res, SWICC_APDU_SW1_NORM_NONE, 0U, 0U);
                    return SWICC_RET_SUCCESS;
                }
                else
                {
                    /* Invalid offset. */
                    /* "Incorrect parameters P1 to P2." */
                    SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2_INFO, 0x86,
                                    0U);
                    return SWICC_RET_SUCCESS;
                }
            }
            else
            {
                /* Data was not the right size. */
                SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_LEN, 0U, 0U);
                return SWICC_RET_SUCCESS;
            }
        }
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_UNK, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }
}

/**
 * @brief Handle the UPDATE BINARY command in the proprietary class 0xA0
 * of 3GPP TS 51.011.
 * @note As described in 3GPP TS 51.011 sec. 9.2.4
 */
static swicc_apduh_ft apduh_gsm_bin_update;
static swicc_ret_et apduh_gsm_bin_update(swicc_st *const swicc_state,
                                         swicc_apdu_cmd_st const *const cmd,
                                         swicc_apdu_res_st *const res,
                                         uint32_t const procedure_count)
{
    if ((cmd->hdr->p1 & 0b10000000) == 0b10000000)
    {
        /* P1 is invalid. */
        SWICC_APDUH_RES(res, SWICC_APDU_SW1_CHER_P1P2, 0U, 0U);
        return SWICC_RET_SUCCESS;
    }

    return apduh_3gpp_bin_update(swicc_state, cmd, res, procedure_count);
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
            break;
        case 0xD6: /* UPDATE BINARY */
            /**
             * Override the default UPDATE BINARY with the UPDATE BINARY from
             * 3GPP.
             */
            ret = apduh_3gpp_bin_update(swicc_state, cmd, res, procedure_count);
            break;
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
         * ETSI TS 102 221 V16.4.0 clause.10.1.2 table.10.5.
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
        case 0x12: /* FETCH */
            /* ETSI */
            if (cmd->hdr->cla.raw == 0x80)
            {
                ret = apduh_etsi_cat_fetch(swicc_state, cmd, res,
                                           procedure_count);
            }
            break;
        case 0x14: /* TERMINAL RESPONSE */
            /* ETSI */
            if (cmd->hdr->cla.raw == 0x80)
            {
                ret = apduh_etsi_cat_terminal_response(swicc_state, cmd, res,
                                                       procedure_count);
            }
            break;
        case 0xC2: /* ENVELOPE */
            /* ETSI */
            if (cmd->hdr->cla.raw == 0x80)
            {
                ret = apduh_etsi_cat_envelope(swicc_state, cmd, res,
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
        case 0x20: /* VERIFY PIN */
            if ((cmd->hdr->cla.raw & 0xF0) == 0x00 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x40 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x60)
            {
                ret = apduh_3gpp_pin_verify(swicc_state, cmd, res,
                                            procedure_count);
            }
            break;
        case 0xD6: /* UPDATE BINARY */
            /* ETSI + 3GPP */
            if ((cmd->hdr->cla.raw & 0xF0) == 0x00 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x40 ||
                (cmd->hdr->cla.raw & 0xF0) == 0x60)
            {
                ret = apduh_3gpp_bin_update(swicc_state, cmd, res,
                                            procedure_count);
            }
            else if (cmd->hdr->cla.raw == 0xA0)
            {
                ret = apduh_gsm_bin_update(swicc_state, cmd, res,
                                           procedure_count);
            }
            break;
        case 0x88: /* RUN GSM ALGORITHM */
            /* GSM */
            if (cmd->hdr->cla.raw == 0xA0)
            {
                ret = apduh_gsm_gsm_algo_run(swicc_state, cmd, res,
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

    /* Run proactive applications. */
    swsim_st *sim_state = swicc_state->userdata;
    sim_proactive_step(sim_state);
    if (ret == SWICC_RET_SUCCESS)
    {
        if (res->sw1 == SWICC_APDU_SW1_NORM_NONE && res->sw2 == 0)
        {
            if (sim_state->proactive.command_length > 0)
            {
                fprintf(
                    stderr,
                    "Proactive command present, overwriting status 9000 to 91%02X len=0x%04X.\n",
                       (uint8_t)sim_state->proactive.command_length,
                       sim_state->proactive.command_length);
                res->sw1 = 0x91;
                static_assert(
                    sizeof(sim_state->proactive.command) == 256,
                    "Proactive command length does not fit in one byte.");
                /**
                 * Safe cast since the command buffer is no longer than the max
                 * APDU length hence no longer than one byte.
                 */
                res->sw2 = (uint8_t)sim_state->proactive.command_length;
            }
        }
    }

    return ret;
}

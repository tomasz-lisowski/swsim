#include "proactive.h"
#include "swsim.h"
#include <stdint.h>
#include <stdio.h>

void sim_proactive_init(swsim_st *const swsim_state)
{

    memset(swsim_state->proactive.command, 0,
           sizeof(swsim_state->proactive.command));
    swsim_state->proactive.command_length = 0;
    memset(swsim_state->proactive.response, 0,
           sizeof(swsim_state->proactive.response));
    swsim_state->proactive.response_length = 0;

    swsim_state->proactive.command_count = 0;

    swsim_state->proactive.app_default_response_wait = false;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.2.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__alpha_identifier(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__alpha_identifier_st const
        *const tlv_alpha_identifier)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x85) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_alpha_identifier->valid)
    {
        if (swicc_dato_bertlv_enc_data(
                encoder,
                (uint8_t const *const)tlv_alpha_identifier->alpha_identifier,
                /**
                 * Safe cast since in worst case, the string will be
                 * truncated, null-termination isn't necessary.
                 */
                (uint32_t)strlen(tlv_alpha_identifier->alpha_identifier)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.80.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__frame_identifier(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__frame_identifier_st const
        *const tlv_frame_identifier)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xE8) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_frame_identifier->valid)
    {
        if (swicc_dato_bertlv_enc_data(encoder,
                                       &tlv_frame_identifier->frame_identifier,
                                       1) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.72.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__text_attribute(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__text_attribute_st const *const tlv_text_attribute)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xD0) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_text_attribute->valid)
    {
        swicc_ret_et ret_text_attribute = SWICC_RET_SUCCESS;
        for (uint8_t format_i = 0;
             format_i < tlv_text_attribute->text_formatting_count; ++format_i)
        {
            /**
             * For iterating backwards safely. Safe cast since format index will
             * never be greater than the formatting count - 1.
             */
            uint8_t const index =
                (uint8_t)((tlv_text_attribute->text_formatting_count -
                           format_i) -
                          1);

            swsim__proactive__tlv__text_attribute__text_formatting_st const
                tlv_text_formatting =
                    tlv_text_attribute->text_formatting[index];
            /* Order of these items is flipped for a correct encoding. */
            uint8_t const text_formatting[4] = {
                (uint8_t)tlv_text_formatting.color,
                (uint8_t)tlv_text_formatting.style,
                tlv_text_formatting.length,
                tlv_text_formatting.start_offset,
            };

            if (swicc_dato_bertlv_enc_data(encoder, text_formatting, 4) !=
                SWICC_RET_SUCCESS)
            {
                ret_text_attribute = SWICC_RET_ERROR;
                break;
            }
        }
        if (ret_text_attribute != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.48.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__url(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__url_st const *const tlv_url)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xB1) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_url->valid)
    {
        if (swicc_dato_bertlv_enc_data(
                encoder, (uint8_t const *const)tlv_url->url,
                /**
                 * Safe cast since in worst case, the string will be
                 * truncated, null-termination isn't necessary.
                 */
                (uint32_t)strlen(tlv_url->url)) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.47.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__browser_identity(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__browser_identity_st const
        *const tlv_browser_identity)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xB0) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_browser_identity->valid)
    {
        uint8_t const browser_identitiy =
            (uint8_t)tlv_browser_identity->browser_identity;
        if (swicc_dato_bertlv_enc_data(encoder, &browser_identitiy, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.8.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__duration(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__duration_st const *const tlv_duration)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x84) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_duration->valid)
    {
        uint8_t const time_unit = (uint8_t)tlv_duration->time_unit;
        if (swicc_dato_bertlv_enc_data(encoder, &tlv_duration->time_interval,
                                       1) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &time_unit, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.43.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__immediate_response(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__immediate_response_st const
        *const tlv_immediate_response)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xAB) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_immediate_response->valid)
    {
        if (swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
            SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.31.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__icon_identifier(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__icon_identifier_st const *const tlv_icon_identifier)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x1E) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_icon_identifier->valid)
    {
        uint8_t const icon_identifier =
            (uint8_t)tlv_icon_identifier->icon_identifier;
        uint8_t const icon_qualifier =
            (uint8_t)tlv_icon_identifier->icon_qualifier;
        if (swicc_dato_bertlv_enc_data(encoder, &icon_identifier, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &icon_qualifier, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.15.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__text_string(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__text_string_st const *const tlv_text_string)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x8D) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_text_string->valid)
    {
        uint8_t const data_coding_scheme =
            (uint8_t)tlv_text_string->data_coding_scheme;
        if (swicc_dato_bertlv_enc_data(
                encoder, (uint8_t const *const)tlv_text_string->text_string,
                /**
                 * Safe cast since in worst case, the string will be truncated,
                 * null-termination isn't necessary.
                 */
                (uint32_t)strlen(tlv_text_string->text_string)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &data_coding_scheme, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.73.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__item_text_attribute_list(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__item_text_attribute_list_st const
        *const tlv_item_text_attribute_list)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xD1) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_item_text_attribute_list->valid)
    {
        for (uint8_t text_attribute_list_i = 0;
             text_attribute_list_i <
             tlv_item_text_attribute_list->text_formatting_count;
             ++text_attribute_list_i)
        {
            /**
             * For iterating backwards safely. Safe cast since text format index
             * will never be greater than the text format count - 1.
             */
            uint8_t const index_backwards =
                (uint8_t)((tlv_item_text_attribute_list->text_formatting_count -
                           text_attribute_list_i) -
                          1);

            swsim__proactive__tlv__text_attribute__text_formatting_st const
                tlv_text_formatting = tlv_item_text_attribute_list
                                          ->text_formatting[index_backwards];
            /* Order of these items is flipped for a correct encoding. */
            uint8_t const text_formatting[4] = {
                (uint8_t)tlv_text_formatting.color,
                (uint8_t)tlv_text_formatting.style,
                tlv_text_formatting.length,
                tlv_text_formatting.start_offset,
            };
            if (swicc_dato_bertlv_enc_data(encoder, text_formatting, 4) !=
                    SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                    SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.32.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__item_icon_identifier_list(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__item_icon_identifier_list_st const
        *const tlv_item_icon_identifier_list)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x9F) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_item_icon_identifier_list->valid)
    {
        for (uint8_t icon_identifier_i = 0;
             icon_identifier_i <
             tlv_item_icon_identifier_list->icon_identifier_count;
             ++icon_identifier_i)
        {
            /**
             * For iterating backwards safely. Safe cast since the icon
             * identifier list length - 1 is always at least the size of the
             * icon identifier index.
             */
            uint8_t const index_backwards =
                (uint8_t)(tlv_item_icon_identifier_list->icon_identifier_count -
                          icon_identifier_i) -
                1;
            if (swicc_dato_bertlv_enc_data(
                    encoder,
                    &tlv_item_icon_identifier_list
                         ->icon_identifier_list[index_backwards],
                    1) != SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
        }
        uint8_t const icon_list_qualifier =
            (uint8_t)tlv_item_icon_identifier_list->icon_list_qualifier;
        if (swicc_dato_bertlv_enc_data(encoder, &icon_list_qualifier, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.24.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__item_next_action_indicator(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__item_next_action_indicator_st const
        *const tlv_item_next_action_indicator)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(
            &bertlv_tag,
            0x18 /* Specs shows one tag but 0x98 might also work here. */) !=
        SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_item_next_action_indicator->valid)
    {
        /* Iterate backwards to encode items in user-specified order. */
        for (uint8_t item_i = 0;
             item_i <
             tlv_item_next_action_indicator->item_next_action_indcator_count;
             ++item_i)
        {
            /**
             * For iterating backwards safely. Safe cast since the count will
             * always be at least 1 larger than the index.
             */
            uint8_t const index_backwards =
                (uint8_t)(tlv_item_next_action_indicator
                              ->item_next_action_indcator_count -
                          item_i - 1);
            if (swicc_dato_bertlv_enc_data(
                    encoder,
                    &tlv_item_next_action_indicator
                         ->item_next_action_indicator_list[index_backwards],
                    1) != SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
        }
        if (swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
            SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.9.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__item(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__item_st const *const tlv_item)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x8F) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_item->valid)
    {
        if (swicc_dato_bertlv_enc_data(
                encoder, (uint8_t const *const)tlv_item->item_text_string,
                /**
                 * Safe cast since in worst case, the string will be
                 * truncated, null-termination isn't necessary.
                 */
                (uint32_t)strlen(tlv_item->item_text_string)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &tlv_item->item_identifier,
                                       1) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.16.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__tone(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__tone_st const *const tlv_tone)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x8E) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_tone->valid)
    {
        uint8_t const tone = (uint8_t)tlv_tone->tone;
        if (swicc_dato_bertlv_enc_data(encoder, &tone, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.52.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__bearer_description(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__bearer_description_st const
        *const tlv_bearer_description)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xB5) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_bearer_description->valid)
    {
        uint8_t const bearer_type =
            (uint8_t)tlv_bearer_description->bearer_type;
        if (swicc_dato_bertlv_enc_data(
                encoder, tlv_bearer_description->bearer_parameter,
                tlv_bearer_description->bearer_parameter_count) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &bearer_type, 1) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.55.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__buffer_size(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__buffer_size_st const *const tlv_buffer_size)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xB9) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_buffer_size->valid)
    {
        if (swicc_dato_bertlv_enc_data(
                encoder, (uint8_t const *const)&tlv_buffer_size->buffer_size,
                2U) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.59.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__uicc_terminal_interface_transport_level(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__uicc_terminal_interface_transport_level_st const
        *const tlv_uicc_terminal_interface_transport_level)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xBC) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_uicc_terminal_interface_transport_level->valid)
    {
        uint8_t const transport_protocol_type =
            (uint8_t)tlv_uicc_terminal_interface_transport_level
                ->transport_protocol_type;
        if (swicc_dato_bertlv_enc_data(
                encoder,
                (uint8_t const
                     *const)&tlv_uicc_terminal_interface_transport_level
                    ->port_number,
                2U) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &transport_protocol_type, 1U) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.58.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__other_address(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__other_address_st const *const tlv_other_address)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xBE) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_other_address->valid)
    {
        if (tlv_other_address->null == false)
        {
            switch (tlv_other_address->address_type)
            {
            case SWSIM__PROACTIVE__OTHER_ADDRESS__ADDRESS_TYPE__IPV4:
                if (swicc_dato_bertlv_enc_data(
                        encoder, tlv_other_address->ipv4,
                        sizeof(tlv_other_address->ipv4)) != SWICC_RET_SUCCESS)
                {
                    return SWICC_RET_ERROR;
                }
                break;
            case SWSIM__PROACTIVE__OTHER_ADDRESS__ADDRESS_TYPE__IPV6:
                if (swicc_dato_bertlv_enc_data(
                        encoder, tlv_other_address->ipv6,
                        sizeof(tlv_other_address->ipv6)) != SWICC_RET_SUCCESS)
                {
                    return SWICC_RET_ERROR;
                }
                break;
            }

            uint8_t const address_type =
                (uint8_t)tlv_other_address->address_type;
            if (swicc_dato_bertlv_enc_data(encoder, &address_type, 1U) !=
                SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
        }
        else
        {
            /**
             * When null is set, we don't send the value part, so the length
             * becomes 0.
             */
        }
        if (swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
            SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.1.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__address(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__address_st const *const tlv_address)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x86) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_address->valid)
    {
        uint8_t const ton_npi =
            (uint8_t)((uint8_t)((((uint8_t)tlv_address->type_of_number) &
                                 (uint8_t)0b00000111)
                                << 4U) |
                      (tlv_address->numbering_plan_identification &
                       (uint8_t)0b00001111)) |
            (uint8_t)0b10000000;
        if (swicc_dato_bertlv_enc_data(encoder, tlv_address->dialing_number,
                                       tlv_address->dialing_number_length) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &ton_npi, 1U) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.3.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__subaddress(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__subaddress_st const *const tlv_subaddress)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0x88) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_subaddress->valid)
    {
        uint8_t const ton_npi =
            (uint8_t)((uint8_t)((((uint8_t)tlv_subaddress->type_of_number) &
                                 (uint8_t)0b00000111)
                                << 4U) |
                      (tlv_subaddress->numbering_plan_identification &
                       (uint8_t)0b00001111)) |
            (uint8_t)0b10000000;
        if (swicc_dato_bertlv_enc_data(
                encoder, &tlv_subaddress->extension1_record_identifier, 1U) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(
                encoder,
                &tlv_subaddress->capability_configuration1_record_identifier,
                1U) != SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(
                encoder, tlv_subaddress->dialing_number_ssc,
                sizeof(tlv_subaddress->dialing_number_ssc)) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_data(encoder, &ton_npi, 1U) !=
                SWICC_RET_SUCCESS ||
            /**
             * NOTE: Per ETSI TS 102 223 V17.2.0 clause.8.3, we omit the length
             * of BCD number/SSC contents. Normally it would go right here per
             * ETSI TS 131 102 V17.10.0 clause.4.4.2.3.
             */
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.70.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__network_access_name(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__network_access_name_st const
        *const tlv_network_access_name)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xC7) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_network_access_name->valid)
    {
        if (swicc_dato_bertlv_enc_data(
                encoder,
                (uint8_t const *const)&tlv_network_access_name
                    ->network_access_name,
                tlv_network_access_name->network_access_name_length) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

/**
 * BER-TLV per ETSI TS 102 223 V17.2.0 clause.8.68.
 * Tag per ETSI TS 101 220 V17.1.0 clause.7.2 table.7.23.
 */
static swicc_ret_et proactive_cmd__tlv__remote_entity_address(
    swicc_dato_bertlv_enc_st *const encoder,
    swsim__proactive__tlv__remote_entity_address_st const
        *const tlv_remote_entity_address)
{
    swicc_dato_bertlv_tag_st bertlv_tag;
    if (swicc_dato_bertlv_tag_create(&bertlv_tag, 0xC9) != SWICC_RET_SUCCESS)
    {
        return SWICC_RET_ERROR;
    }

    if (tlv_remote_entity_address->valid)
    {
        switch (tlv_remote_entity_address->coding_type)
        {
        case SWSIM__PROACTIVE__REMOTE_ENTITY_ADDRESS__CODING_TYPE__IEEE_802_16_2009_ADDRESS_48BIT:
            if (swicc_dato_bertlv_enc_data(
                    encoder, tlv_remote_entity_address->ieee_802_16_2009,
                    sizeof(tlv_remote_entity_address->ieee_802_16_2009)) !=
                SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
            break;
        case SWSIM__PROACTIVE__REMOTE_ENTITY_ADDRESS__CODING_TYPE__IRDA_DEVICE_ADDRESS_32BIT:
            if (swicc_dato_bertlv_enc_data(
                    encoder, tlv_remote_entity_address->irda,
                    sizeof(tlv_remote_entity_address->irda)) !=
                SWICC_RET_SUCCESS)
            {
                return SWICC_RET_ERROR;
            }
            break;
        }

        uint8_t const coding_type =
            (uint8_t)tlv_remote_entity_address->coding_type;
        if (swicc_dato_bertlv_enc_data(encoder, &coding_type, 1U) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(encoder, &bertlv_tag) !=
                SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }
    return SWICC_RET_SUCCESS;
}

// remote_entity_address

static swicc_ret_et proactive_cmd(
    swsim__proactive__command_st const *const command,
    uint8_t (*const command_buffer)[SWICC_DATA_MAX],
    uint16_t *const command_length)
{
    /* ETSI TS 101 220 V17.1.0 clause 7.2. */
    static uint8_t const tags[] = {
        0xD0, /* 'D0': Proactive UICC command */
        0x81, /* '81': Command details */
        0x82, /* '82': Device identity */
    };
    static uint32_t const tags_count = sizeof(tags) / sizeof(tags[0U]);
    swicc_dato_bertlv_tag_st bertlv_tags[tags_count];
    for (uint8_t tag_idx = 0U; tag_idx < tags_count; ++tag_idx)
    {
        if (swicc_dato_bertlv_tag_create(&bertlv_tags[tag_idx],
                                         tags[tag_idx]) != SWICC_RET_SUCCESS)
        {
            return SWICC_RET_ERROR;
        }
    }

    uint8_t(*bertlv_buf)[SWICC_DATA_MAX];
    uint32_t bertlv_len;
    swicc_ret_et ret_bertlv = SWICC_RET_ERROR;
    swicc_dato_bertlv_enc_st enc;

    for (bool dry_run = true;; dry_run = false)
    {
        if (dry_run)
        {
            fprintf(stderr, "Proactive UICC Command: Encoding dry run.\n");
            bertlv_buf = NULL;
            bertlv_len = 0;
        }
        else
        {
            if (enc.len > sizeof(*command_buffer))
            {
                break;
            }
            bertlv_len = enc.len;
            bertlv_buf = command_buffer;
            fprintf(stderr,
                    "Proactive UICC Command: Encoding real run: len=%u.\n",
                    bertlv_len);
        }

        swicc_dato_bertlv_enc_init(&enc, (uint8_t *const)bertlv_buf,
                                   bertlv_len);
        swicc_dato_bertlv_enc_st enc_nstd;

        if (swicc_dato_bertlv_enc_nstd_start(&enc, &enc_nstd) !=
            SWICC_RET_SUCCESS)
        {
            break;
        }

        swicc_ret_et ret_command_specific = SWICC_RET_ERROR;
        switch (command->command_type)
        {
        case SWSIM__PROACTIVE__COMMAND_TYPE__REFRESH:
        case SWSIM__PROACTIVE__COMMAND_TYPE__MORE_TIME:
        case SWSIM__PROACTIVE__COMMAND_TYPE__POLL_INTERVAL:
        case SWSIM__PROACTIVE__COMMAND_TYPE__POLLING_OFF:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_EVENT_LIST:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_CALL:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SEND_SS:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SEND_USSD:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SEND_SHORT_MESSAGE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SEND_DTMF:
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__LAUNCH_BROWSER:
            if (/* clang-format off */
                proactive_cmd__tlv__alpha_identifier(&enc_nstd, &command->launch_browser.alpha_identifier_user_confirmation_phase) != SWICC_RET_SUCCESS ||
                /* Mandatory not enforced. */
                proactive_cmd__tlv__url(&enc_nstd, &command->launch_browser.url) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__browser_identity( &enc_nstd, &command->launch_browser.browser_identity) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }
            ret_command_specific = SWICC_RET_SUCCESS;
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__GEOGRAPHICAL_LOCATION_REQUEST:
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__PLAY_TONE:
            if (/* clang-format off */
                proactive_cmd__tlv__frame_identifier(&enc_nstd, &command->play_tone.frame_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__text_attribute(&enc_nstd, &command->play_tone.text_attribute) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__icon_identifier(&enc_nstd, &command->play_tone.icon_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__duration(&enc_nstd, &command->play_tone.duration) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__tone(&enc_nstd, &command->play_tone.tone) != SWICC_RET_SUCCESS ||
                /* Mandatory not enforced. */
                proactive_cmd__tlv__alpha_identifier(&enc_nstd, &command->play_tone.alpha_identifier) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }
            ret_command_specific = SWICC_RET_SUCCESS;
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__DISPLAY_TEXT:
            if (/* clang-format off */
                proactive_cmd__tlv__frame_identifier(&enc_nstd, &command->display_text.frame_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__text_attribute(&enc_nstd, &command->display_text.text_attribute) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__duration(&enc_nstd, &command->display_text.duration) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__immediate_response(&enc_nstd, &command->display_text.immediate_response) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__icon_identifier(&enc_nstd, &command->display_text.icon_identifier) != SWICC_RET_SUCCESS ||
                /* Mandatory not enforced. */
                proactive_cmd__tlv__text_string(&enc_nstd, &command->display_text.text_string) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }
            ret_command_specific = SWICC_RET_SUCCESS;
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_INKEY:
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_INPUT:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SELECT_ITEM:
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_MENU: {
            if (/* clang-format off */
                proactive_cmd__tlv__item_text_attribute_list(&enc_nstd, &command->set_up_menu.item_text_attribute_list) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__text_attribute(&enc_nstd, &command->set_up_menu.text_attribute) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__item_icon_identifier_list(&enc_nstd, &command->set_up_menu.item_icon_identifier_list) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__icon_identifier(&enc_nstd, &command->set_up_menu.icon_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__item_next_action_indicator(&enc_nstd, &command->set_up_menu.item_next_action_indicator) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }

            for (uint8_t item_i = 0; item_i < command->set_up_menu.item_count;
                 ++item_i)
            {
                /**
                 * Iterate backwards to encode items in user-specified order.
                 * Safe cast since the count will always be at least 1 greater
                 * than the index.
                 */
                uint8_t const index_backwards =
                    (uint8_t)(command->set_up_menu.item_count - item_i - 1);
                proactive_cmd__tlv__item(
                    &enc_nstd, &command->set_up_menu.item[index_backwards]);
            }

            /* Mandatory not enforced. */
            if (proactive_cmd__tlv__alpha_identifier(
                    &enc_nstd, &command->set_up_menu.alpha_identifier) !=
                SWICC_RET_SUCCESS)
            {
                break;
            }

            ret_command_specific = SWICC_RET_SUCCESS;
            break;
        }
        case SWSIM__PROACTIVE__COMMAND_TYPE__PROVIDE_LOCAL_INFORMATION:
        case SWSIM__PROACTIVE__COMMAND_TYPE__TIMER_MANAGEMENT:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_IDLE_MODE_TEXT:
        case SWSIM__PROACTIVE__COMMAND_TYPE__PERFORM_CARD_APDU:
        case SWSIM__PROACTIVE__COMMAND_TYPE__POWER_ON_CARD:
        case SWSIM__PROACTIVE__COMMAND_TYPE__POWER_OFF_CARD:
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_READER_STATUS:
        case SWSIM__PROACTIVE__COMMAND_TYPE__RUN_AT_COMMAND:
        case SWSIM__PROACTIVE__COMMAND_TYPE__LANGUAGE_NOTIFICATION:
            break;
        case SWSIM__PROACTIVE__COMMAND_TYPE__OPEN_CHANNEL: {
            bool nested_failed = false;
            switch (command->open_channel.type)
            {
            case SWSIM__PROACTIVE__COMMAND__OPEN_CHANNEL__TYPE__CS_BEARER:
                if (/* clang-format off */
                    proactive_cmd__tlv__text_string(&enc_nstd, &command->open_channel.cs_bearer.text_string_user_login) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__other_address(&enc_nstd, &command->open_channel.cs_bearer.other_address_local_address) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__duration(&enc_nstd, &command->open_channel.cs_bearer.duration2) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__subaddress(&enc_nstd, &command->open_channel.cs_bearer.subaddress) != SWICC_RET_SUCCESS ||

                    /* Conditional not enforced. */
                    proactive_cmd__tlv__duration(&enc_nstd, &command->open_channel.cs_bearer.duration1) != SWICC_RET_SUCCESS ||

                    /* Mandatory not enforced. */
                    proactive_cmd__tlv__address(&enc_nstd, &command->open_channel.cs_bearer.address) != SWICC_RET_SUCCESS
                    /* clang-format on */)
                {
                    nested_failed = true;
                    break;
                }
                break;
            case SWSIM__PROACTIVE__COMMAND__OPEN_CHANNEL__TYPE__PACKET_DATA_SERVICE_BEARER:
                if (/* clang-format off */
                    proactive_cmd__tlv__network_access_name(&enc_nstd, &command->open_channel.packet_data_service_bearer.network_access_name) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__text_string(&enc_nstd, &command->open_channel.packet_data_service_bearer.text_string_user_login) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__other_address(&enc_nstd, &command->open_channel.packet_data_service_bearer.other_address_local_address) != SWICC_RET_SUCCESS
                    /* clang-format on */)
                {
                    nested_failed = true;
                    break;
                }
                break;
            case SWSIM__PROACTIVE__COMMAND__OPEN_CHANNEL__TYPE__LOCAL_BEARER:
                if (/* clang-format off */
                    proactive_cmd__tlv__remote_entity_address(&enc_nstd, &command->open_channel.local_bearer.remote_entity_address) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__duration(&enc_nstd, &command->open_channel.local_bearer.duration2) != SWICC_RET_SUCCESS ||

                    /* Conditional not enforced. */
                    proactive_cmd__tlv__duration(&enc_nstd, &command->open_channel.local_bearer.duration1) != SWICC_RET_SUCCESS
                    /* clang-format on */)
                {
                    nested_failed = true;
                    break;
                }
                break;
            case SWSIM__PROACTIVE__COMMAND__OPEN_CHANNEL__TYPE__DEFAULT_NETWORK_BEARER:
                if (/* clang-format off */
                    proactive_cmd__tlv__text_string(&enc_nstd, &command->open_channel.default_network_bearer.text_string_user_login) != SWICC_RET_SUCCESS ||
                    proactive_cmd__tlv__other_address(&enc_nstd, &command->open_channel.default_network_bearer.other_address_local_address) != SWICC_RET_SUCCESS
                    /* clang-format on */)
                {
                    nested_failed = true;
                    break;
                }
                break;
            }
            if (nested_failed)
            {
                break;
            }

            if (/* clang-format off */
                proactive_cmd__tlv__frame_identifier(&enc_nstd, &command->open_channel.frame_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__uicc_terminal_interface_transport_level(&enc_nstd, &command->open_channel.uicc_terminal_interface_transport_level) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__text_string(&enc_nstd, &command->open_channel.text_string_user_password) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__icon_identifier(&enc_nstd, &command->open_channel.icon_identifier) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__alpha_identifier(&enc_nstd, &command->open_channel.alpha_identifier) != SWICC_RET_SUCCESS ||

                /* Conditional not enforced. */
                proactive_cmd__tlv__text_attribute(&enc_nstd, &command->open_channel.text_attribute) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__other_address(&enc_nstd, &command->open_channel.data_destination_address) != SWICC_RET_SUCCESS ||

                /* Mandatory not enforced. */
                proactive_cmd__tlv__buffer_size(&enc_nstd, &command->open_channel.buffer_size) != SWICC_RET_SUCCESS ||
                proactive_cmd__tlv__bearer_description(&enc_nstd, &command->open_channel.bearer_description) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }
            ret_command_specific = SWICC_RET_SUCCESS;
            break;
        }
        case SWSIM__PROACTIVE__COMMAND_TYPE__CLOSE_CHANNEL:
        case SWSIM__PROACTIVE__COMMAND_TYPE__RECEIVE_DATA:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SEND_DATA:
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_CHANNEL_STATUS:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SERVICE_SEARCH:
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_SERVICE_INFORMATION:
        case SWSIM__PROACTIVE__COMMAND_TYPE__DECLARE_SERVICE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SET_FRAMES:
        case SWSIM__PROACTIVE__COMMAND_TYPE__GET_FRAMES_STATUS:
        case SWSIM__PROACTIVE__COMMAND_TYPE__RETRIEVE_MULTIMEDIA_MESSAGE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__SUBMIT_MULTIMEDIA_MESSAGE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__DISPLAY_MULTIMEDIA_MESSAGE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__ACTIVATE:
        case SWSIM__PROACTIVE__COMMAND_TYPE__CONTACTLESS_STATE_CHANGED:
        case SWSIM__PROACTIVE__COMMAND_TYPE__COMMAND_CONTAINER:
        case SWSIM__PROACTIVE__COMMAND_TYPE__ENCAPSULATED_SESSION_CONTROL:
        case SWSIM__PROACTIVE__COMMAND_TYPE__LSI_COMMAND:
        case SWSIM__PROACTIVE__COMMAND_TYPE__END_OF_PROACTIVE_UICC_SESSION:
            break;
        }
        if (ret_command_specific != SWICC_RET_SUCCESS)
        {
            break;
        }

        {
            /**
             * ETSI TS 102 223 V17.2.0 clause.6.6 describes the structure of
             * every proactive UICC command. The 'command details' and 'device
             * identities' objects are not technically part of any command
             * header because there is no such header. Nevertheless, every
             * single command has these two objects encoded at the start in the
             * same order so here we just encode it at the start of every single
             * command (instead of once in every case of the command type
             * switch).
             * XXX: If ETSI TS 102 223 introduces new commands without 'command
             * details' and 'device identities' as the first two objects, then
             * this will need to move into every appropriate case of the command
             * type switch.
             */
            uint8_t const device_identity_destination =
                (uint8_t)command->device_identities.destination;
            uint8_t const device_identity_source =
                (uint8_t)command->device_identities.source;
            uint8_t const command_qualifier =
                (uint8_t)command->command_qualifier;
            uint8_t const command_type = (uint8_t)command->command_type;
            if (/* clang-format off */
                /* Device identity source and destination. */
                swicc_dato_bertlv_enc_data(&enc_nstd, &device_identity_destination, 1) != SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_data(&enc_nstd, &device_identity_source, 1) != SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_hdr(&enc_nstd, &bertlv_tags[2]) != SWICC_RET_SUCCESS ||

                /* Command details. */
                swicc_dato_bertlv_enc_data(&enc_nstd, &command_qualifier, 1) !=  SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_data(&enc_nstd, &command_type, 1) != SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_data(&enc_nstd, &command->command_number, 1) != SWICC_RET_SUCCESS ||
                swicc_dato_bertlv_enc_hdr(&enc_nstd, &bertlv_tags[1]) != SWICC_RET_SUCCESS
                /* clang-format on */)
            {
                break;
            }
        }

        if (swicc_dato_bertlv_enc_nstd_end(&enc, &enc_nstd) !=
                SWICC_RET_SUCCESS ||
            swicc_dato_bertlv_enc_hdr(&enc, &bertlv_tags[0]) !=
                SWICC_RET_SUCCESS)
        {
            break;
        }

        /* Stop when finished with the real run (i.e. not dry run). */
        if (!dry_run)
        {
            if (bertlv_len <= UINT16_MAX)
            {
                ret_bertlv = SWICC_RET_SUCCESS;
                /* Safe cast since we check for type overflow in if. */
                *command_length = (uint16_t)bertlv_len;
            }
            break;
        }
    }

    return ret_bertlv;
}

typedef enum app_default__screen_e
{
    APP_DEFAULT__SCREEN__NONE,
    APP_DEFAULT__SCREEN__HOME,
    APP_DEFAULT__SCREEN__LAUNCH_BROWSER,
    APP_DEFAULT__SCREEN__DISPLAY_TEXT,
    APP_DEFAULT__SCREEN__SET_UP_MENU,
    APP_DEFAULT__SCREEN__SET_UP_MENU__RUN,
    APP_DEFAULT__SCREEN__PLAY_TONE,
    APP_DEFAULT__SCREEN__OPEN_CHANNEL,
    APP_DEFAULT__SCREEN__INVALID,
} app_default__screen_et;

swicc_ret_et sim_proactive_step(swsim_st *const swsim_state)
{
    /* Initialize the default app in the very first step. */
    if (swsim_state->proactive.command_count == 0)
    {
        swsim_state->proactive.app_default.select_screen_last =
            APP_DEFAULT__SCREEN__NONE;
        swsim_state->proactive.app_default.select_screen_new =
            APP_DEFAULT__SCREEN__HOME;
    }

    if (swsim_state->proactive.envelope_length > 0)
    {
        fprintf(stderr, "Envelope: start parsing.\n");
        static uint8_t const root_tag[] = {
            0xD3, /* 'D3': Menu Selection */
        };
        static uint32_t const root_tag_count =
            sizeof(root_tag) / sizeof(root_tag[0U]);
        swicc_dato_bertlv_tag_st bertlv_root_tag[root_tag_count];
        swicc_ret_et ret_tag = SWICC_RET_ERROR;
        for (uint8_t tag_idx = 0U; tag_idx < root_tag_count; ++tag_idx)
        {
            if (swicc_dato_bertlv_tag_create(&bertlv_root_tag[tag_idx],
                                             root_tag[tag_idx]) !=
                SWICC_RET_SUCCESS)
            {
                break;
            }
            if (tag_idx + 1U >= root_tag_count)
            {
                ret_tag = SWICC_RET_SUCCESS;
            }
        }

        static uint8_t const tag[] = {
            0x82, /* '82': Device identity */
            0x90, /* '90': Item identifier */
            0x95, /* '95': Help request */
        };
        static uint32_t const tag_count = sizeof(tag) / sizeof(tag[0U]);
        swicc_dato_bertlv_tag_st bertlv_tag[tag_count];
        ret_tag = SWICC_RET_ERROR;
        for (uint8_t tag_idx = 0U; tag_idx < tag_count; ++tag_idx)
        {
            if (swicc_dato_bertlv_tag_create(&bertlv_tag[tag_idx],
                                             tag[tag_idx]) != SWICC_RET_SUCCESS)
            {
                break;
            }
            if (tag_idx + 1U >= tag_count)
            {
                ret_tag = SWICC_RET_SUCCESS;
            }
        }

        if (ret_tag == SWICC_RET_SUCCESS)
        {
            fprintf(stderr, "Envelope: tags created.\n");

            swicc_dato_bertlv_dec_st tlv_decoder;
            swicc_dato_bertlv_dec_init(&tlv_decoder,
                                       swsim_state->proactive.envelope,
                                       swsim_state->proactive.envelope_length);

            swicc_dato_bertlv_st tlv_root;
            swicc_dato_bertlv_dec_st tlv_decoder_root;

            /**
             * Make sure the BER-TLV object has a supported length format and
             * valid tag class.
             */
            if (swicc_dato_bertlv_dec_next(&tlv_decoder) == SWICC_RET_SUCCESS &&
                swicc_dato_bertlv_dec_cur(&tlv_decoder, &tlv_decoder_root,
                                          &tlv_root) == SWICC_RET_SUCCESS &&
                (tlv_root.len.form ==
                     SWICC_DATO_BERTLV_LEN_FORM_DEFINITE_SHORT ||
                 tlv_root.len.form ==
                     SWICC_DATO_BERTLV_LEN_FORM_DEFINITE_LONG) &&
                tlv_root.tag.cla == SWICC_DATO_BERTLV_TAG_CLA_PRIVATE)
            {
                fprintf(
                    stderr,
                    "Envelope: root tag has valid class and root has valid length format.\n");

                for (uint32_t tag_i = 0; tag_i < root_tag_count; ++tag_i)
                {
                    if (bertlv_root_tag[tag_i].num == tlv_root.tag.num)
                    {
                        /* Found a recognized tag. */
                        switch (tag_i)
                        {
                        /* Menu Selection */
                        case 0: {
                            fprintf(stderr, "Envelope menu selection.\n");
                            swicc_dato_bertlv_dec_st
                                tlv_decoder_device_identities;
                            swicc_dato_bertlv_st tlv_device_identities;
                            swicc_dato_bertlv_dec_st
                                tlv_decoder_item_identifier;
                            swicc_dato_bertlv_st tlv_item_identifier;
                            if (swicc_dato_bertlv_dec_next(&tlv_decoder_root) ==
                                    SWICC_RET_SUCCESS &&
                                swicc_dato_bertlv_dec_cur(
                                    &tlv_decoder_root,
                                    &tlv_decoder_device_identities,
                                    &tlv_device_identities) ==
                                    SWICC_RET_SUCCESS &&
                                swicc_dato_bertlv_dec_next(&tlv_decoder_root) ==
                                    SWICC_RET_SUCCESS &&
                                swicc_dato_bertlv_dec_cur(
                                    &tlv_decoder_root,
                                    &tlv_decoder_item_identifier,
                                    &tlv_item_identifier) ==
                                    SWICC_RET_SUCCESS &&
                                tlv_device_identities.tag.num ==
                                    bertlv_tag[0].num &&
                                tlv_device_identities.tag.pc ==
                                    bertlv_tag[0].pc &&
                                tlv_device_identities.tag.cla ==
                                    bertlv_tag[0].cla &&
                                tlv_item_identifier.tag.num ==
                                    bertlv_tag[1].num &&
                                tlv_item_identifier.tag.pc ==
                                    bertlv_tag[1].pc &&
                                tlv_item_identifier.tag.cla ==
                                    bertlv_tag[1].cla &&
                                tlv_device_identities.len.form ==
                                    SWICC_DATO_BERTLV_LEN_FORM_DEFINITE_SHORT &&
                                tlv_item_identifier.len.form ==
                                    SWICC_DATO_BERTLV_LEN_FORM_DEFINITE_SHORT)
                            {
                                fprintf(
                                    stderr,
                                    "Envelope menu selection: extracted mandatory items.\n");

                                /**
                                 * This field is optional so this decode may
                                 * fail.
                                 */
                                swicc_dato_bertlv_dec_st
                                    tlv_decoder_help_request;
                                swicc_dato_bertlv_st tlv_help_request;
                                if (swicc_dato_bertlv_dec_next(
                                        &tlv_decoder_root) ==
                                        SWICC_RET_SUCCESS &&
                                    swicc_dato_bertlv_dec_cur(
                                        &tlv_decoder_root,
                                        &tlv_decoder_help_request,
                                        &tlv_help_request) == SWICC_RET_SUCCESS)
                                {
                                    /**
                                     * TODO: Verify the tag and lenght fields.
                                     */
                                }

                                uint8_t const item_identifier =
                                    tlv_decoder_item_identifier.buf[0];
                                fprintf(
                                    stderr,
                                    "Envelope menu selection: item identifier 0x%02X.\n",
                                    item_identifier);
                                if (item_identifier >=
                                    APP_DEFAULT__SCREEN__INVALID)
                                {
                                    swsim_state->proactive.app_default
                                        .select_screen_new =
                                        APP_DEFAULT__SCREEN__HOME;
                                }
                                else
                                {
                                    switch (swsim_state->proactive.app_default
                                                .select_screen_last)
                                    {
                                    case APP_DEFAULT__SCREEN__HOME:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__LAUNCH_BROWSER;
                                            break;
                                        case 0x02:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__DISPLAY_TEXT;
                                            break;
                                        case 0x03:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__SET_UP_MENU;
                                            break;
                                        case 0x04:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__PLAY_TONE;
                                            break;
                                        case 0x05:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__OPEN_CHANNEL;
                                            break;
                                        default:
                                            break;
                                        }
                                        break;
                                    case APP_DEFAULT__SCREEN__LAUNCH_BROWSER:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__HOME;
                                            break;
                                        case 0x02: {
                                            swsim__proactive__tlv__provisioning_file_reference_st const
                                                provisioning_file_reference[] =
                                                    {
                                                        {
                                                            .valid = false,
                                                        },
                                                    };
                                            swsim__proactive__command__launch_browser_st
                                                command_launch_browser = {
                                                    .browser_identity =
                                                        {
                                                            .valid = true,
                                                            .browser_identity =
                                                                SWSIM__PROACTIVE__BROWSER_IDENTITY__BROWSER_IDENTITY__DEFAULT_BROWSER,
                                                        },
                                                    .url =
                                                        {
                                                            .valid = true,
                                                            .url =
                                                                /* clang-format off */ "https://ziglang.org/" /* clang-format on */
                                                            ,
                                                        },
                                                    .bearer =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .provisioning_file_reference_count =
                                                        sizeof(
                                                            provisioning_file_reference) /
                                                        sizeof(
                                                            provisioning_file_reference
                                                                [0]),
                                                    .provisioning_file_reference =
                                                        (swsim__proactive__tlv__provisioning_file_reference_st const
                                                             *)
                                                            provisioning_file_reference,
                                                    .text_string_gateway_proxy_identity =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .alpha_identifier_user_confirmation_phase =
                                                        {
                                                            .valid = true,
                                                            .alpha_identifier =
                                                                /* clang-format off */ "This totally isn't a confirmation prompt to open the browser..." /* clang-format on */
                                                            ,
                                                        },
                                                    .icon_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_attribute =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .frame_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .network_access_name =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_string_user_login =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_string_user_password =
                                                        {
                                                            .valid = false,
                                                        },
                                                };

                                            swsim__proactive__command_st const command = {
                                                .command_number = 0,
                                                .command_type =
                                                    SWSIM__PROACTIVE__COMMAND_TYPE__LAUNCH_BROWSER,
                                                .command_qualifier =
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__LAUNCH_BROWSER__LAUNCH_BROWSER_IF_NOT_ALREADY_LAUNCHED,
                                                .device_identities =
                                                    {
                                                        .valid = true,
                                                        .destination =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__TERMINAL,
                                                        .source =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                                                    },
                                                .launch_browser =
                                                    command_launch_browser,
                                            };

                                            swicc_ret_et const ret =
                                                proactive_cmd(
                                                    &command,
                                                    &swsim_state->proactive
                                                         .command,
                                                    &swsim_state->proactive
                                                         .command_length);
                                            if (ret == SWICC_RET_SUCCESS)
                                            {
                                                swsim_state->proactive
                                                    .app_default_response_wait =
                                                    true;
                                            }
                                            break;
                                        }
                                        default:
                                            break;
                                        }
                                        break;
                                    case APP_DEFAULT__SCREEN__DISPLAY_TEXT:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__HOME;
                                            break;
                                        case 0x02: {
                                            /**
                                             * Display the GSM 7bit default
                                             * alphabet (without extensions).
                                             */
                                            char text[8 * 16];
                                            for (uint8_t text_i = 0;
                                                 text_i < sizeof(text);
                                                 ++text_i)
                                            {
                                                char const ch =
                                                    (char)((sizeof(text) -
                                                            text_i) -
                                                           1);
                                                switch (ch)
                                                {
                                                case 0x20:
                                                case 0x0A:
                                                case 0x1B:
                                                case 0x0D:
                                                    text[text_i] = ' ';
                                                    break;
                                                default:
                                                    text[text_i] = ch;
                                                }
                                            }

                                            swsim__proactive__tlv__text_attribute__text_formatting_st const
                                                text_formatting[1] = {{
                                                    .start_offset = 0,
                                                    .length = sizeof(text),
                                                    .style =
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__00_ALIGNMENT_CENTER |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__12_FONT_SIZE_LARGE |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__33_STYLE_BOLD_ON |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__44_STYLE_ITALIC_OFF |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__55_STYLE_UNDERLINED_OFF |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_STYLE__66_STYLE_STRIKETHROUGH_OFF,
                                                    .color =
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_COLOR__FG_BRIGHT_CYAN |
                                                        SWSIM__PROACTIVE__TEXT_ATTRIBUTE__FORMATTING_COLOR__BG_DARK_MAGENTA,
                                                }};

                                            swsim__proactive__command__display_text_st const
                                                command_display_text = {
                                                    .text_string =
                                                        {
                                                            .valid = true,
                                                            .data_coding_scheme =
                                                                SWSIM__PROACTIVE__TEXT_STRING__DATA_CODING_SCHEME__GSM_DEFAULT_ALPHABET_8BIT,
                                                            .text_string = text,
                                                        },
                                                    .icon_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .immediate_response =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .duration =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_attribute =
                                                        {
                                                            .valid = true,
                                                            .text_formatting_count =
                                                                1,
                                                            .text_formatting =
                                                                text_formatting,
                                                        },
                                                    .frame_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                };

                                            swsim__proactive__command_st const command = {
                                                .command_number = 0,
                                                .command_type =
                                                    SWSIM__PROACTIVE__COMMAND_TYPE__DISPLAY_TEXT,
                                                .command_qualifier =
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__DISPLAY_TEXT__00_PRIORITY_HIGH |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__DISPLAY_TEXT__16_RFU |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__DISPLAY_TEXT__77_WAIT_FOR_USER_TO_CLEAR_MESSAGE,
                                                .device_identities =
                                                    {
                                                        .valid = true,
                                                        .destination =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__DISPLAY,
                                                        .source =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                                                    },
                                                .display_text =
                                                    command_display_text,
                                            };

                                            swicc_ret_et const ret =
                                                proactive_cmd(
                                                    &command,
                                                    &swsim_state->proactive
                                                         .command,
                                                    &swsim_state->proactive
                                                         .command_length);
                                            if (ret == SWICC_RET_SUCCESS)
                                            {
                                                swsim_state->proactive
                                                    .app_default_response_wait =
                                                    true;
                                            }
                                        }
                                        break;
                                        default:
                                            break;
                                        }
                                        break;
                                    case APP_DEFAULT__SCREEN__SET_UP_MENU:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__HOME;
                                            break;
                                        case 0x02: {
                                            char const *const item_text[] = {
                                                "oooooooo", "ooooooo ",
                                                "oooooo  ", "ooooo   ",
                                                "oooo    ", "ooo     ",
                                                "oo      ", "o       ",
                                                "        ",
                                            };
                                            uint8_t const item_count =
                                                sizeof(item_text) /
                                                sizeof(item_text[0]);

                                            swsim__proactive__tlv__item_st
                                                item[sizeof(item_text) /
                                                     sizeof(item_text[0])];
                                            uint8_t
                                                items_next_action_indicator_list
                                                    [sizeof(item) /
                                                     sizeof(item[0])];
                                            for (uint8_t item_i = 0;
                                                 item_i < item_count; ++item_i)
                                            {
                                                item[item_i].valid = true;
                                                item[item_i].item_identifier =
                                                    item_i + 1;
                                                item[item_i].item_text_string =
                                                    item_text[item_i];
                                                items_next_action_indicator_list
                                                    [item_i] =
                                                        item[item_i]
                                                            .item_identifier;
                                            }

                                            swsim__proactive__tlv__item_next_action_indicator_st const
                                                item_next = {
                                                    .valid = true,
                                                    .item_next_action_indcator_count =
                                                        item_count,
                                                    .item_next_action_indicator_list =
                                                        items_next_action_indicator_list,
                                                };
                                            swsim__proactive__command__set_up_menu_st const
                                                command_set_up_menu = {
                                                    .alpha_identifier =
                                                        {
                                                            .valid = true,
                                                            .alpha_identifier =
                                                                /* clang-format off */ "swSIM Proactive Menu!"/* clang-format on */
                                                            ,
                                                        },
                                                    .item_count = item_count,
                                                    .item = item,
                                                    .item_next_action_indicator =
                                                        item_next,
                                                    .icon_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .item_icon_identifier_list =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_attribute =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .item_text_attribute_list =
                                                        {
                                                            .valid = false,
                                                        },
                                                };
                                            swsim__proactive__command_st const command = {
                                                .command_number = 0,
                                                .command_type =
                                                    SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_MENU,
                                                .command_qualifier =
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__00_SELECTION_PREFERENCE_NONE |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__16_RFU |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__77_NO_HELP_INFORMATION_AVAILABLE,
                                                .device_identities =
                                                    {
                                                        .valid = true,
                                                        .destination =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__TERMINAL,
                                                        .source =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                                                    },
                                                .set_up_menu =
                                                    command_set_up_menu,
                                            };

                                            swicc_ret_et const ret =
                                                proactive_cmd(
                                                    &command,
                                                    &swsim_state->proactive
                                                         .command,
                                                    &swsim_state->proactive
                                                         .command_length);
                                            if (ret == SWICC_RET_SUCCESS)
                                            {
                                                swsim_state->proactive
                                                    .app_default_response_wait =
                                                    true;
                                                swsim_state->proactive
                                                    .app_default
                                                    .select_screen_new =
                                                    APP_DEFAULT__SCREEN__SET_UP_MENU__RUN;
                                                swsim_state->proactive
                                                    .app_default
                                                    .select_screen_last =
                                                    APP_DEFAULT__SCREEN__SET_UP_MENU__RUN;
                                            }
                                            break;
                                        }

                                        default:
                                            break;
                                        }
                                        break;
                                    case APP_DEFAULT__SCREEN__SET_UP_MENU__RUN:
                                        /**
                                         * Any selection in the custom menu
                                         * triggered by the SET UP MENU sub-menu
                                         * returns to the home screen.
                                         */
                                        swsim_state->proactive.app_default
                                            .select_screen_new =
                                            APP_DEFAULT__SCREEN__HOME;
                                        break;
                                    case APP_DEFAULT__SCREEN__PLAY_TONE:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__HOME;
                                            break;
                                        case 0x02: {
                                            swsim__proactive__command__play_tone_st const
                                                command_play_tone = {
                                                    .alpha_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .tone =
                                                        {
                                                            .valid = true,
                                                            .tone =
                                                                SWSIM__PROACTIVE__TONE__TONE__STANDARD_SUPERVISORY_ERROR_SPECIAL_INFORMATION,
                                                        },
                                                    .duration =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .icon_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .text_attribute =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .frame_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                };

                                            swsim__proactive__command_st const command = {
                                                .command_number = 0,
                                                .command_type =
                                                    SWSIM__PROACTIVE__COMMAND_TYPE__PLAY_TONE,
                                                .command_qualifier =
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__PLAY_TONE__00_VIBRATE_OPTIONAL |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__PLAY_TONE__17_RFU,
                                                .device_identities =
                                                    {
                                                        .valid = true,
                                                        .destination =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__TERMINAL,
                                                        .source =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                                                    },
                                                .play_tone = command_play_tone,
                                            };

                                            swicc_ret_et const ret =
                                                proactive_cmd(
                                                    &command,
                                                    &swsim_state->proactive
                                                         .command,
                                                    &swsim_state->proactive
                                                         .command_length);
                                            if (ret == SWICC_RET_SUCCESS)
                                            {
                                                swsim_state->proactive
                                                    .app_default_response_wait =
                                                    true;
                                            }
                                        }
                                        break;
                                        default:
                                            break;
                                        }
                                        break;
                                    case APP_DEFAULT__SCREEN__OPEN_CHANNEL:
                                        switch (item_identifier)
                                        {
                                        case 0x01:
                                            swsim_state->proactive.app_default
                                                .select_screen_new =
                                                APP_DEFAULT__SCREEN__HOME;
                                            break;
                                        case 0x02: {
                                            swsim__proactive__command__open_channel_st const
                                                command_open_channel = {
                                                    .alpha_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .icon_identifier =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .bearer_description =
                                                        {
                                                            .valid = true,
                                                            .bearer_type =
                                                                SWSIM__PROACTIVE__BEARER_DESCRIPTION__BEARER_TYPE__DEFAULT_BEARER_FOR_REQUESTED_TRANSPORT_LAYER,
                                                            .bearer_parameter_count =
                                                                0,
                                                            .bearer_parameter =
                                                                NULL,
                                                        },
                                                    .buffer_size =
                                                        {
                                                            .valid = true,
                                                            .buffer_size = 0x0F,
                                                        },
                                                    .text_string_user_password =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .uicc_terminal_interface_transport_level =
                                                        {
                                                            .valid = true,
                                                            .transport_protocol_type =
                                                                SWSIM__PROACTIVE__UICC_TERMINAL_INTERFACE_TRANSPORT_LEVEL__TRANSPORT_PROTOCOL_TYPE__TCP_CLIENT_MODE_REMOTE_CONNECTION,
                                                            .port_number = 80,
                                                        },
                                                    .data_destination_address =
                                                        {
                                                            .valid = true,
                                                            .null = false,
                                                            .address_type =
                                                                SWSIM__PROACTIVE__OTHER_ADDRESS__ADDRESS_TYPE__IPV4,
                                                            .ipv4 = {127, 0, 0,
                                                                     1},
                                                        },
                                                    .text_attribute =
                                                        {
                                                            .valid = false,
                                                        },
                                                    .frame_identifier =
                                                        {
                                                            .valid = false,
                                                        },

                                                    .type =
                                                        SWSIM__PROACTIVE__COMMAND__OPEN_CHANNEL__TYPE__DEFAULT_NETWORK_BEARER,

                                                    .default_network_bearer =
                                                        {
                                                            .other_address_local_address =
                                                                {
                                                                    .valid =
                                                                        true,
                                                                    .null =
                                                                        true,
                                                                },
                                                            .text_string_user_login =
                                                                {
                                                                    .valid =
                                                                        false,
                                                                },
                                                        },

                                                };

                                            swsim__proactive__command_st const command = {
                                                .command_number = 0,
                                                .command_type =
                                                    SWSIM__PROACTIVE__COMMAND_TYPE__OPEN_CHANNEL,
                                                .command_qualifier =
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__OPEN_CHANNEL_ELSE__00_IMMEDIATE_LINK_ESTABLISHMENT |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__OPEN_CHANNEL_ELSE__11_AUTOMATIC_RECONNECTION |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__OPEN_CHANNEL_ELSE__22_IMMEDIATE_LINK_ESTABLISHMENT_IN_BACKGROUND_MODE |
                                                    SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__OPEN_CHANNEL_ELSE__33_NO_DNS_SERVER_ADDRESSES_REQUESTED,
                                                .device_identities =
                                                    {
                                                        .valid = true,
                                                        .destination =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__TERMINAL,
                                                        .source =
                                                            SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                                                    },
                                                .open_channel =
                                                    command_open_channel,
                                            };

                                            swicc_ret_et const ret =
                                                proactive_cmd(
                                                    &command,
                                                    &swsim_state->proactive
                                                         .command,
                                                    &swsim_state->proactive
                                                         .command_length);
                                            if (ret == SWICC_RET_SUCCESS)
                                            {
                                                swsim_state->proactive
                                                    .app_default_response_wait =
                                                    true;
                                            }
                                        }
                                        break;
                                        default:
                                            break;
                                        }
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
        }

        /**
         * Mark as handled. It gets marked even in the event of a failure to
         * handle the envelope data. We do this to avoid re-handling the same
         * erroneous data and never making space for a new envelope.
         */
        swsim_state->proactive.envelope_length = 0;
    }

    /* Wait until we get response before creating more commands. */
    if (swsim_state->proactive.app_default_response_wait == false)
    {
        swsim__proactive__command__set_up_menu_st command_set_up_menu = {
            .alpha_identifier =
                {
                    .valid = false,
                },
            .item_count = 0,
            .item = NULL,
            .item_next_action_indicator =
                {
                    .valid = false,
                },
            .icon_identifier =
                {
                    .valid = false,
                },
            .item_icon_identifier_list =
                {
                    .valid = false,
                },
            .text_attribute =
                {
                    .valid = false,
                },
            .item_text_attribute_list =
                {
                    .valid = false,
                },
        };
        swsim__proactive__command_st command = {
            .command_number = 0,
            .command_type = SWSIM__PROACTIVE__COMMAND_TYPE__SET_UP_MENU,
            .command_qualifier =
                SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__00_SELECTION_PREFERENCE_SOFT_KEY |
                SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__16_RFU |
                SWSIM__PROACTIVE__COMMAND_DETAILS__COMMAND_QUALIFIER__SET_UP_MENU__77_NO_HELP_INFORMATION_AVAILABLE,
            .device_identities =
                {
                    .valid = true,
                    .destination =
                        SWSIM__PROACTIVE__DEVICE_IDENTITIES__TERMINAL,
                    .source = SWSIM__PROACTIVE__DEVICE_IDENTITIES__UICC,
                },
            .set_up_menu = command_set_up_menu,
        };
        swsim__proactive__tlv__item_st item[10];

        bool command_created = false;

        /**
         * Only re-generate a screen when the newly selected screen
         * differs from the last selected screen.
         */
        if (swsim_state->proactive.app_default.select_screen_new ==
                APP_DEFAULT__SCREEN__HOME &&
            swsim_state->proactive.app_default.select_screen_last !=
                APP_DEFAULT__SCREEN__HOME)
        {
            uint8_t const item_count = 5;
            static char const *const item_text[5] = {
                "C: LAUNCH BROWSER", "C: DISPLAY TEXT", "C: SET UP MENU",
                "C: PLAY TONE",      "C: OPEN CHANNEL",
            };
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "swSIM Menu";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }
        else if (swsim_state->proactive.app_default.select_screen_new ==
                     APP_DEFAULT__SCREEN__LAUNCH_BROWSER &&
                 swsim_state->proactive.app_default.select_screen_last !=
                     APP_DEFAULT__SCREEN__LAUNCH_BROWSER)
        {
            uint8_t const item_count = 2;
            static char const *const item_text[2] = {"Back", "Run"};
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "C: LAUNCH BROWSER";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }
        else if (swsim_state->proactive.app_default.select_screen_new ==
                     APP_DEFAULT__SCREEN__DISPLAY_TEXT &&
                 swsim_state->proactive.app_default.select_screen_last !=
                     APP_DEFAULT__SCREEN__DISPLAY_TEXT)
        {
            uint8_t const item_count = 2;
            static char const *const item_text[2] = {"Back", "Run"};
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "C: DISPLAY TEXT";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }
        else if (swsim_state->proactive.app_default.select_screen_new ==
                     APP_DEFAULT__SCREEN__SET_UP_MENU &&
                 swsim_state->proactive.app_default.select_screen_last !=
                     APP_DEFAULT__SCREEN__SET_UP_MENU)
        {
            uint8_t const item_count = 2;
            static char const *const item_text[2] = {"Back", "Run"};
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "C: SET UP MENU";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }
        else if (swsim_state->proactive.app_default.select_screen_new ==
                     APP_DEFAULT__SCREEN__PLAY_TONE &&
                 swsim_state->proactive.app_default.select_screen_last !=
                     APP_DEFAULT__SCREEN__PLAY_TONE)
        {
            uint8_t const item_count = 2;
            static char const *const item_text[2] = {"Back", "Run"};
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "C: PLAY TONE";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }
        else if (swsim_state->proactive.app_default.select_screen_new ==
                     APP_DEFAULT__SCREEN__OPEN_CHANNEL &&
                 swsim_state->proactive.app_default.select_screen_last !=
                     APP_DEFAULT__SCREEN__OPEN_CHANNEL)
        {
            uint8_t const item_count = 2;
            static char const *const item_text[2] = {"Back", "Run"};
            for (uint8_t item_i = 0; item_i < item_count; ++item_i)
            {
                item[item_i].valid = true;
                item[item_i].item_identifier = item_i + 1;
                item[item_i].item_text_string = item_text[item_i];
            }

            command.set_up_menu.alpha_identifier.valid = true;
            command.set_up_menu.alpha_identifier.alpha_identifier =
                "C: OPEN CHANNEL";
            command.set_up_menu.item_count = item_count;
            command.set_up_menu.item = item;
            command_created = true;
        }

        if (command_created)
        {
            swicc_ret_et const ret =
                proactive_cmd(&command, &swsim_state->proactive.command,
                              &swsim_state->proactive.command_length);
            if (ret == SWICC_RET_SUCCESS)
            {
                // Screen has been selected.
                swsim_state->proactive.app_default.select_screen_last =
                    swsim_state->proactive.app_default.select_screen_new;

                swsim_state->proactive.app_default_response_wait = true;
                swsim_state->proactive.command_count += 1;
            }
        }
    }
    return SWICC_RET_SUCCESS;
}

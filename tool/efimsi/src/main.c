#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(char const *const arg0)
{
    fprintf(
        stderr,
        "\nUsage: %s <MCC MNC MSIN | IMSI_encoded>"
        "\nThis tool is used to encode or decode an IMSI into/from a format that is used"
        "\ndirectly in the filesystem of an ICC."
        "\n- MCC is the mobile country code (3 digits)."
        "\n- MNC is the mobile network code (2 to 3 digits)."
        "\n- MSIN is the mobile subscriber identity number (at most 10 digits)."
        "\n- IMSI is the international mobile subscriber identity (at most 15 digits but when encoded can be as long as 8 bytes)."
        "\n",
        arg0);
}

static int32_t encode(char const *const mcc, char const *const mnc,
                      char const *const msin)
{
    /* 15 digits is the maximum length per ITU-T E.212 09/2016 clause.6.1. */
    char imsi[2 + 15 + 1];

    uint64_t const mcc_len = strlen(mcc);
    if (mcc_len != 3)
    {
        fprintf(stderr, "MCC has invalid length, expected 3, got %lu.",
                mcc_len);
        return EXIT_FAILURE;
    }
    uint64_t const mnc_len = strlen(mnc);
    if (!(mnc_len == 2 || mnc_len == 3))
    {
        fprintf(stderr, "MNC has invalid length, expected 2 or 3, got %lu.",
                mnc_len);
        return EXIT_FAILURE;
    }
    uint64_t const msin_len = strlen(msin);
    if (msin_len > 10)
    {
        fprintf(stderr, "MSIN has invalid length, expected <=10, got %lu.",
                msin_len);
        return EXIT_FAILURE;
    }

    char msin_bcd[12];
    memset(msin_bcd, '0', sizeof(msin_bcd));
    memcpy(msin_bcd, msin, msin_len);
    for (uint8_t msin_i = 0; msin_i < sizeof(msin_bcd) / 2; ++msin_i)
    {
        char const tmp = msin_bcd[msin_i * 2];
        msin_bcd[msin_i * 2] = msin_bcd[(msin_i * 2) + 1];
        msin_bcd[(msin_i * 2) + 1] = tmp;
    }
    uint64_t const msin_bcd_len = msin_len + (msin_len % 2);

    uint64_t i = 2; /* Skip over length nibbles. */
    imsi[i++] = mcc[0];
    imsi[i++] = '9';
    imsi[i++] = mcc[2];
    imsi[i++] = mcc[1];
    imsi[i++] = mnc[1];
    imsi[i++] = mnc[0];
    if (mnc_len == 3)
    {
        imsi[i++] = mnc[2];
    }
    memcpy(&imsi[i], msin_bcd, msin_bcd_len);
    i += msin_bcd_len;

    uint8_t const length = (uint8_t)((i + 1U) / 2U) - 1;
    fprintf(stderr, "IMSI length is %u bytes.\n", length);

    imsi[0] = '0';
    imsi[1] = (char)(length + '0' + (length >= 9 ? '9' - 'A' : 0));

    fprintf(stdout, "%.*s\n", (uint8_t)i, imsi);
    return EXIT_SUCCESS;
}

static int32_t decode(char const *const imsi_encoded)
{
    uint64_t const imsi_encoded_len = strlen(imsi_encoded);
    if (imsi_encoded_len > 16 + 2 || imsi_encoded_len < 5 + 2 ||
        imsi_encoded_len % 2 != 0)
    {
        fprintf(
            stderr,
            "IMSI length must be between 5 and 16 characters and must be even, got %lu.\n",
            imsi_encoded_len - 2);
        return EXIT_FAILURE;
    }
    for (uint64_t i = 0; i < imsi_encoded_len; ++i)
    {
        if ((imsi_encoded[i] >= '0' && imsi_encoded[i] <= '9') ||
            (imsi_encoded[i] >= 'A' && imsi_encoded[i] <= 'F'))
        {
        }
        else
        {
            fprintf(
                stderr,
                "Expected an upper case hex string but got an unexpected character '%c'.\n",
                imsi_encoded[i]);
            return EXIT_FAILURE;
        }
    }

    uint8_t length_nibble[2] = {(uint8_t)imsi_encoded[0],
                                (uint8_t)imsi_encoded[1]};
    for (uint8_t i = 0; i < 2; ++i)
    {
        length_nibble[i] = length_nibble[i] >= 'A' ? length_nibble[i] - 'A'
                                                   : length_nibble[i] - '0';
    }
    uint8_t const length =
        (uint8_t)((uint8_t)(length_nibble[0] << 4) | (length_nibble[1] & 0x0F));
    if (length != (imsi_encoded_len - 2) / 2)
    {
        fprintf(
            stderr,
            "IMSI length field does not match the actual encoded IMSI length, expected %lu, got %u.\n",
            (imsi_encoded_len - 2) / 2, length);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "IMSI byte length is %u.\n", length);

    if (imsi_encoded[3] != '9')
    {
        fprintf(stderr, "The first BCD shall be 9, got %u.\n", imsi_encoded[3]);
        return EXIT_FAILURE;
    }

    uint8_t imsi_decoded[16];
    for (uint8_t i = 0; i < length; ++i)
    {
        /**
         * +2 to skip the length byte and another +2 because first IMSI byte is
         * already parsed.
         */
        imsi_decoded[i * 2] = (uint8_t)imsi_encoded[2 + ((i * 2) + 1)];
        imsi_decoded[(i * 2) + 1] = (uint8_t)imsi_encoded[2 + (i * 2)];
    }
    fprintf(
        stdout, "%.*s\n", (length * 2) - 1,
        &imsi_decoded
            [1] /* Skip over the initial '9' that is not part of the IMSI. */);
    return EXIT_SUCCESS;
}

int32_t main(int32_t const argc, char const *const argv[argc])
{
    if (argc != 4 && argc != 2)
    {
        fprintf(stderr,
                "Invalid number of arguments, expected 4 or 2, got %u.\n",
                argc);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 4)
    {
        fprintf(stderr, "Encoding...\n");
        return encode(argv[1], argv[2], argv[3]);
    }
    else if (argc == 2)
    {
        fprintf(stderr, "Decoding...\n");
        return decode(argv[1]);
    }
}

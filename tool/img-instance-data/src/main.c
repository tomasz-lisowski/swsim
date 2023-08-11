#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ico_s
{
    uint64_t width;
    uint64_t height;
    uint8_t bits_per_pixel;
    bool valid;
    uint64_t data_length;
    uint8_t *data;
} ico_st;

typedef struct ico_icondir_s
{
    uint16_t reserved;
    uint16_t image_type;
    uint16_t image_count;
} __attribute__((packed)) ico_icondir_st;

typedef struct ico_icondirentry_s
{
    uint8_t width;
    uint8_t height;
    uint8_t color_count;
    uint8_t reserved;
    uint16_t color_planes;
    uint16_t bits_per_pixel;
    uint32_t bitmap_data_size;
    uint32_t file_offset;
} __attribute__((packed)) ico_icondirentry_st;

typedef struct ico_bitmapinfoheader_s
{
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t color_planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_in_color_table;
    uint32_t important_color_count;
} __attribute__((packed)) ico_bitmapinfoheader_st;

static void print_usage(char const *const arg0)
{
    fprintf(
        stderr,
        "\nUsage: %s"
        " </path/to/image.ico>"
        "\n"
        "\nThis expects to get an image in a ICO format, which will be converted into"
        "\nimage instance data per 3GPP TS 31.102 V17.5.0 section 4.6.1.2."
        "\n",
        arg0);
}

static uint8_t *parse_bitmap(uint8_t const bits_per_pixel, uint64_t const width,
                             uint64_t const height,
                             uint64_t const pixel_data_len,
                             uint8_t const *const pixel_data,
                             uint64_t *const parsed_length)
{
    switch (bits_per_pixel)
    {
    case 8:
        fprintf(stderr, "Parsing image as %lux%lu@8bps.\n", width, height);

        uint64_t const pixel_data_length = width * height;
        uint64_t const data_length =
            ((pixel_data_length - (pixel_data_length % 8)) + 8) / 8;
        uint8_t *const data = malloc(data_length);

        for (uint64_t data_i = 0; data_i < data_length; ++data_i)
        {
            data[data_i] = 0;
            for (uint8_t shift = 0; shift < 8; ++shift)
            {
                uint64_t const offset = (8 * data_i) + shift;
                uint8_t const bit = (uint8_t)(pixel_data[offset] << shift);
                uint8_t const mask = (uint8_t)(1 << shift);
                fprintf(stderr, "data[%lu]: %02X & %02X.\n", offset, bit, mask);
                data[data_i] |= bit & mask;
            }
        }
        *parsed_length = data_length;
        return data;
    default:
        fprintf(stderr,
                "Image color depth is not supported: got= support=8.\n");
        return NULL;
    }
}

static ico_st parse_ico(uint64_t const ico_data_size,
                        uint8_t const *const ico_data)
{
    ico_st ico = {
        .width = 0,
        .height = 0,
        .valid = false,
        .data_length = 0,
        .data = NULL,
    };

    if (sizeof(ico_icondir_st) + sizeof(ico_icondirentry_st) > ico_data_size)
    {
        fprintf(
            stderr,
            "Image data is too short to contain a icondir and icondirentry: value=%lu expected>=%lu.",
            ico_data_size,
            sizeof(ico_icondir_st) + sizeof(ico_icondirentry_st));
        return ico;
    }

    ico_icondir_st icondir;
    memcpy(&icondir, &ico_data[0], sizeof(ico_icondir_st));

    ico_icondirentry_st icondirentry;
    memcpy(&icondirentry, &ico_data[sizeof(ico_icondir_st)],
           sizeof(ico_icondirentry_st));

    if (icondir.reserved == 0 && icondir.image_type == 1 &&
        icondir.image_count == 1 && icondirentry.reserved == 0)
    {
        fprintf(stderr, "ICONDIR: image_type=.ICO image_count=%u.\n",
                icondir.image_count);
        fprintf(
            stderr,
            "ICONDIRENTRY: width=%u height=%u color_count=%u color_planes=%u bits_per_pixel=%u bitmap_data_size=%u file_offset=%u.\n",
            icondirentry.width, icondirentry.height, icondirentry.color_count,
            icondirentry.color_planes, icondirentry.bits_per_pixel,
            icondirentry.bitmap_data_size, icondirentry.file_offset);

        if (icondirentry.file_offset >=
                sizeof(ico_icondir_st) + sizeof(ico_icondirentry_st) &&
            icondirentry.file_offset + sizeof(ico_bitmapinfoheader_st) <=
                ico_data_size)
        {
            ico_bitmapinfoheader_st bitmapinfoheader;
            memcpy(&bitmapinfoheader, &ico_data[icondirentry.file_offset],
                   sizeof(ico_bitmapinfoheader_st));
            fprintf(
                stderr,
                "BITMAPINFOHEADER: header_size=%u width=%u height=%u color_planes=%u bits_per_pixel=%u compression=%u image_size=%u x_pixel_per_meter=%i y_pixel_per_meter=%i colors_in_color_table=%u important_color_count=%u.\n",
                bitmapinfoheader.header_size, bitmapinfoheader.width,
                bitmapinfoheader.height, bitmapinfoheader.color_planes,
                bitmapinfoheader.bits_per_pixel, bitmapinfoheader.compression,
                bitmapinfoheader.image_size,
                bitmapinfoheader.x_pixels_per_meter,
                bitmapinfoheader.y_pixels_per_meter,
                bitmapinfoheader.colors_in_color_table,
                bitmapinfoheader.important_color_count);

            /* Validate bitmap info header by comparing it to the icon directory
             * entry for the same image. */
            if (bitmapinfoheader.header_size ==
                    sizeof(ico_bitmapinfoheader_st) &&
                bitmapinfoheader.width == icondirentry.width &&
                bitmapinfoheader.height == icondirentry.height * 2 &&
                bitmapinfoheader.color_planes == icondirentry.color_planes &&
                bitmapinfoheader.bits_per_pixel ==
                    icondirentry.bits_per_pixel &&
                bitmapinfoheader.compression == 0 /* NONE */)
            {
                uint64_t const color_table_length =
                    (1 << icondirentry.bits_per_pixel) * 4;
                uint64_t const pixel_data_length =
                    icondirentry.width * icondirentry.height;
                uint64_t const pixel_data_offset =
                    icondirentry.file_offset + sizeof(ico_bitmapinfoheader_st) +
                    color_table_length;

                if (pixel_data_offset + pixel_data_length <= ico_data_size)
                {
                    ico.bits_per_pixel = 8;
                    ico.width = icondirentry.width;
                    ico.height = icondirentry.height;

                    ico.data = parse_bitmap(
                        ico.bits_per_pixel, ico.width, ico.height,
                        icondirentry.bitmap_data_size,
                        &ico_data[pixel_data_offset], &ico.data_length);

                    if (ico.data_length <= 255)
                    {
                        ico.valid = true;
                    }
                    else
                    {
                        fprintf(
                            stderr,
                            "Image will not fit in a transparent EF: got=%lu expected<=255.\n",
                            ico.data_length);
                    }
                }
                else
                {
                    fprintf(
                        stderr,
                        "Image file length unexpected: got=%lu expected>=%lu (pixel_data_offset=%lu pixel_data_length=%lu file_offset=%u color_table_length=%lu).\n",
                        ico_data_size, pixel_data_offset + pixel_data_length,
                        pixel_data_offset, pixel_data_length,
                        icondirentry.file_offset, color_table_length);
                }
            }
            else
            {
                fprintf(stderr,
                        "BITMAPINFOHEADER header_size: got=%u expected=%lu.\n",
                        bitmapinfoheader.header_size,
                        sizeof(ico_bitmapinfoheader_st));
                fprintf(stderr, "BITMAPINFOHEADER width: got=%u expected=%u.\n",
                        bitmapinfoheader.width, icondirentry.width);
                fprintf(stderr,
                        "BITMAPINFOHEADER height: got=%u expected=%u*2=%u.\n",
                        bitmapinfoheader.height, icondirentry.height,
                        icondirentry.height * 2);
                fprintf(stderr,
                        "BITMAPINFOHEADER color_planes: got=%u expected=%u.\n",
                        bitmapinfoheader.color_planes,
                        icondirentry.color_planes);
                fprintf(
                    stderr,
                    "BITMAPINFOHEADER bits_per_pixel: got=%u expected=%u.\n",
                    bitmapinfoheader.bits_per_pixel,
                    icondirentry.bits_per_pixel);
                fprintf(
                    stderr,
                    "BITMAPINFOHEADER compression: got=%u expected=%u=NONE.\n",
                    bitmapinfoheader.compression, 0);
            }
        }
        else
        {
            fprintf(
                stderr,
                "ICONDIRENTRY malformed: file_offset + sizeof(BITMAPINFOHEADER) >= ico_file_size (%u + %lu >= %lu) and/or file_offset < sizeof(ICONDIR) + sizeof(ICONDIRENTRY) (%u < %lu + %lu).\n",
                icondirentry.file_offset, sizeof(ico_bitmapinfoheader_st),
                ico_data_size, icondirentry.file_offset, sizeof(ico_icondir_st),
                sizeof(ico_icondirentry_st));
        }
    }
    else
    {
        fprintf(stderr,
                "ICONDIRENTRY reserved: offset=%lu got=%04X expected=%04X.",
                sizeof(icondir) + offsetof(ico_icondirentry_st, reserved),
                icondirentry.reserved, 0);
        fprintf(stderr,
                "ICONDIR image_count: offset=%lu got=%04X expected=%04X.\n",
                offsetof(ico_icondir_st, image_count), icondir.image_count, 1);
        fprintf(stderr,
                "ICONDIR image_type: offset=%lu got=%04X expected=%04X.\n",
                offsetof(ico_icondir_st, image_type), icondir.image_type, 1);
        fprintf(stderr,
                "ICONDIR reserved: offset=%lu got=%04X expected=%04X.\n",
                offsetof(ico_icondir_st, reserved), icondir.reserved, 0);
    }

    return ico;
}

int32_t main(int32_t const argc, char const *const argv[argc])
{
    if (argc != 2U)
    {
        switch (argc)
        {
        case 1U:
            fprintf(
                stderr,
                "Missing 1st argument which is the path to an icon file that will be converted to image instance data per 3GPP TS 31.102 V17.5.0.\n");
            break;
        case 0U:
            __builtin_unreachable();
        }
        print_usage(argv[0U]);
        return -1;
    }

    char const *const file_path = argv[1U];

    uint8_t *file_data = NULL;
    uint64_t file_size = 0;
    FILE *file;

    file = fopen(file_path, "r");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        int64_t file_size_raw = ftell(file);
        if (file_size_raw > 0)
        {
            file_size = (uint64_t)file_size_raw;
            rewind(file);

            /* Safe casts since we check if size is greater than 0 in the if. */
            file_data = malloc(sizeof(uint8_t) * file_size);
            fread(file_data, 1, file_size, file);
        }
        else
        {
            fprintf(stderr, "Got invalid file size: %li.\n", file_size_raw);
        }
        fclose(file);
    }
    else
    {
        fprintf(stderr, "Failed to open file: \"%s\".", file_path);
        return -1;
    }

    for (uint64_t file_i = 0; file_i < file_size; ++file_i)
    {
        fprintf(stderr, "%02X", file_data[file_i]);
        if (file_i + 1 >= file_size)
        {
            fprintf(stderr, "\n");
        }
    }
    fprintf(stderr, "Image size: %lu.\n", file_size);

    ico_st ico = parse_ico(file_size, file_data);
    if (ico.data != NULL)
    {
        fprintf(
            stderr,
            "Image instance data: valid=%u width=%lu height=%lu bits_per_pixel=%u data_length=%lu data=",
            ico.valid, ico.width, ico.height, ico.bits_per_pixel,
            ico.data_length);
        for (uint64_t ico_data_i = 0; ico_data_i < ico.data_length;
             ++ico_data_i)
        {
            fprintf(stderr, "%02X", ico.data[ico_data_i]);
            if (ico_data_i + 1 >= ico.data_length)
            {
                fprintf(stderr, "\n");
            }
        }
    }
    else
    {
        fprintf(stderr, "Failed to parse image.\n");
    }

    free(ico.data);
    free(file_data);
    return 0;
}

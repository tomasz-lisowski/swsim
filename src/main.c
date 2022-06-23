#include "sim.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <swicc/swicc.h>

swicc_net_client_st client_ctx = {0U};

static void sig_exit_handler(__attribute__((unused)) int signum)
{
    printf("Shutting down...\n");
    swicc_net_client_destroy(&client_ctx);
    exit(0);
}

static void print_usage(char const *const arg0)
{
    // clang-format off
    printf("Usage: %s"
        "\n ["CLR_KND("--help")" | "CLR_KND("-h")"]"
        "\n <"CLR_KND("--ip")" "CLR_VAL("ip")" | "CLR_KND("-i")" "CLR_VAL("ip")">"
        "\n <"CLR_KND("--port")" "CLR_VAL("port")" | "CLR_KND("-p")" "CLR_VAL("port")">"
        "\n <"CLR_KND("--fs-load")" "CLR_VAL("path")" | "CLR_KND("-l")" "CLR_VAL("path")">"
        "\n ["CLR_KND("--fs-save")" "CLR_VAL("path")" | "CLR_KND("-s")" "CLR_VAL("path")"]"
        "\n"
        "\n - IP and port form the address of the server that swSIM will connect to."
        "\n - FS load is for loading the swICC JSON definition of a FS."
        "\n - FS save is for saving the generated swICC FS to disk."
        "\n",
        arg0);
    // clang-format on
}

int32_t main(int32_t const argc, char *const argv[argc])
{
    static struct option const options_long[] = {
        {"help", no_argument, 0, 'h'},
        {"ip", required_argument, 0, 'i'},
        {"fs-load", required_argument, 0, 'l'},
        {"fs-save", required_argument, 0, 's'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0},
    };

    char const *server_ip = NULL;
    char const *server_port = NULL;
    char const *filesystem_path = NULL;
    char const *swiccfs_path = NULL;

    int32_t ch;
    while (1)
    {
        int32_t opt_idx = 0;
        ch = getopt_long(argc, argv, "hi:p:l:s:", options_long, &opt_idx);
        if (ch == -1)
        {
            break;
        }

        switch (ch)
        {
        case 'h':
            print_usage(argv[0U]);
            exit(EXIT_SUCCESS);
        case 'i':
            server_ip = optarg;
            break;
        case 'p':
            server_port = optarg;
            break;
        case 'l':
            filesystem_path = optarg;
            break;
        case 's':
            swiccfs_path = optarg;
            break;
        case '?':
            break;
        }
    }
    if (optind < argc)
    {
        printf("%s: invalid arguments -- '", argv[0U]);
        while (optind < argc)
        {
            printf("%s%c", argv[optind], optind + 1 == argc ? '\0' : ' ');
            optind++;
        }
        printf("'\n");
        print_usage(argv[0U]);
        exit(EXIT_FAILURE);
    }
    if (server_ip == NULL)
    {
        printf("Server IP is mandatory.\n");
        print_usage(argv[0U]);
        exit(EXIT_FAILURE);
    }
    if (server_port == NULL)
    {
        printf("Server port is mandatory.\n");
        print_usage(argv[0U]);
        exit(EXIT_FAILURE);
    }
    if (filesystem_path == NULL)
    {
        printf("File system path is mandatory.\n");
        print_usage(argv[0U]);
        exit(EXIT_FAILURE);
    }
    printf("swSIM:"
           "\n  Load JSON FS from '%s'."
           "\n  Save swICC FS at '%s'."
           "\n  Connect to %s:%s.\n",
           filesystem_path, swiccfs_path == NULL ? "nowhere" : swiccfs_path,
           server_ip, server_port);

    sim_st sim_ctx = {0U};
    if (sim_init(&sim_ctx, filesystem_path, swiccfs_path) == 0)
    {
        swicc_ret_et ret = swicc_net_client_sig_register(sig_exit_handler);
        if (ret == SWICC_RET_SUCCESS)
        {
            ret = swicc_net_client_create(&client_ctx, server_ip, server_port);
            if (ret == SWICC_RET_SUCCESS)
            {
                printf("Press ctrl-c to exit.\n");
                ret = swicc_net_client(&sim_ctx.swicc, &client_ctx);
                if (ret != SWICC_RET_SUCCESS)
                {
                    printf("Failed to run network client.\n");
                }
                swicc_net_client_destroy(&client_ctx);
            }
            else
            {
                printf("Failed to create a client.\n");
            }
        }
        else
        {
            printf("Failed to register signal handler.\n");
        }
        swicc_terminate(&sim_ctx.swicc);
    }
    return EXIT_SUCCESS;
}

/* Use color in the help commands no matter what. */
#ifndef DEBUG_CLR
#define DEBUG_CLR
#endif

#define SERVER_IP_DEF "127.0.0.1"
#define SERVER_PORT_DEF "37324"

#include "pin.h"
#include "swsim.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <swicc/swicc.h>

swicc_net_client_st client_ctx = {0U};

static void sig_exit_handler(__attribute__((unused)) int signum)
{
    fprintf(stderr, "Shutting down...\n");
    swicc_net_client_destroy(&client_ctx);
    exit(0);
}

static void print_usage(char const *const arg0)
{
    // clang-format off
    fprintf(stderr, "Usage: %s"
        "\n["CLR_KND("--help")" | "CLR_KND("-h")"]"
        "\n<"CLR_KND("--ip")" "CLR_VAL("ip")" | "CLR_KND("-i")" "CLR_VAL("ip")">"
        "\n<"CLR_KND("--port")" "CLR_VAL("port")" | "CLR_KND("-p")" "CLR_VAL("port")">"
        "\n<"CLR_KND("--fs")" "CLR_VAL("path")" | "CLR_KND("-f")" "CLR_VAL("path")">"
        "\n["CLR_KND("--fs-gen")" "CLR_VAL("path")" | "CLR_KND("-g")" "CLR_VAL("path")"]"
        "\n"
        "\n- IP and port form the address of the server that swSIM will connect to (by default "CLR_TXT(CLR_YEL, SERVER_IP_DEF":"SERVER_PORT_DEF)")."
        "\n- FS path is a location for loading and saving the swICC FS file."
        "\n- FS gen path is the JSON FS definition location for generating a swICC FS file."
        "\n- Note that if the FS gen path is given, the swICC FS file at the given path will be overwritten with the generated one."
        "\n- The file extension for swICC FS files is '.swiccfs'."
        "\n",
        arg0);
    // clang-format on
}

int32_t main(int32_t const argc, char *const argv[argc])
{
    static struct option const options_long[] = {
        {"help", no_argument, 0, 'h'},
        {"ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"fs", required_argument, 0, 'f'},
        {"fs-gen", required_argument, 0, 'g'},
        {0, 0, 0, 0},
    };

    char const *server_ip = NULL;
    char const *server_port = NULL;
    char const *path_swiccfs = NULL;
    char const *path_fsjson_load = NULL;

    int32_t ch;
    while (1)
    {
        int32_t opt_idx = 0;
        ch = getopt_long(argc, argv, "hi:p:f:g:", options_long, &opt_idx);
        if (ch == -1)
        {
            break;
        }

        switch (ch)
        {
        case 'h':
            print_usage(argv[0U]);
            return EXIT_SUCCESS;
        case 'i':
            server_ip = optarg;
            break;
        case 'p':
            server_port = optarg;
            break;
        case 'f':
            path_swiccfs = optarg;
            break;
        case 'g':
            path_fsjson_load = optarg;
            break;
        case '?':
            break;
        }
    }
    if (optind < argc)
    {
        fprintf(stderr, "%s: invalid arguments -- '", argv[0U]);
        while (optind < argc)
        {
            fprintf(stderr, "%s%c", argv[optind],
                    optind + 1 == argc ? '\0' : ' ');
            optind++;
        }
        fprintf(stderr, "'\n");
        print_usage(argv[0U]);
        return EXIT_FAILURE;
    }
    if (server_ip == NULL)
    {
        server_ip = SERVER_IP_DEF;
        fprintf(stderr, "Using default server IP: '%s'.\n", server_ip);
    }
    if (server_port == NULL)
    {
        server_port = SERVER_PORT_DEF;
        fprintf(stderr, "Using default server port: '%s'.\n", server_port);
    }
    if (path_swiccfs == NULL)
    {
        fprintf(stderr, CLR_TXT(CLR_RED, "File system path is mandatory.\n"));
        print_usage(argv[0U]);
        return EXIT_FAILURE;
    }
    fprintf(stderr,
            "swSIM:"
            "\n  swICC FS at '%s'."
            "\n  FS JSON at  '%s'."
            "\n  Connect to   %s:%s.\n\n",
            path_swiccfs, path_fsjson_load == NULL ? "?" : path_fsjson_load,
            server_ip, server_port);

    swsim_st swsim_state = {0U};
    swicc_st swicc_state = {0U};
    swicc_ret_et ret = SWICC_RET_ERROR;

    if (swsim_init(&swsim_state, &swicc_state, path_fsjson_load,
                   path_swiccfs) == 0)
    {
        ret = swicc_net_client_sig_register(sig_exit_handler);
        if (ret == SWICC_RET_SUCCESS)
        {
            ret = swicc_net_client_create(&client_ctx, server_ip, server_port);
            if (ret == SWICC_RET_SUCCESS)
            {
                fprintf(stderr, "Press ctrl-c to exit.\n");
                ret = swicc_net_client(&swicc_state, &client_ctx);
                if (ret != SWICC_RET_SUCCESS)
                {
                    if (ret != SWICC_RET_NET_DISCONNECTED)
                    {
                        fprintf(stderr, "Failed to run network client.\n");
                    }
                    else
                    {
                        fprintf(stderr,
                                "Client was disconnected from server.\n");
                    }
                }
                swicc_net_client_destroy(&client_ctx);
            }
            else
            {
                fprintf(stderr, "Failed to create a client.\n");
            }
        }
        else
        {
            fprintf(stderr, "Failed to register signal handler.\n");
        }
        swicc_terminate(&swicc_state);
    }

    if (ret == SWICC_RET_NET_DISCONNECTED)
    {
        /* Special return for when client gets disconnected. */
        return 2;
    }

    if (ret == SWICC_RET_SUCCESS)
    {
        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }
}

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
    printf("Usage: %s [--help | -h] <--ip server_ip | -i server_ip> "
           "<--port server_port | -p server_port> <--filesystem "
           "path_to_fs_json | -f path_to_fs_json>\n",
           arg0);
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
            print_usage(argv[0U]);
            break;
        }
    }
    if (optind < argc)
    {
        printf("Invalid argv-elements: ");
        while (optind < argc)
        {
            printf("%s ", argv[optind++]);
        }
        printf("\n");
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
           "\n  Connect to %s:%s.\n\n",
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

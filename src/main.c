#include "sim.h"
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

int32_t main(__attribute__((unused)) int32_t const argc,
             __attribute__((unused)) char const *const argv[argc])
{
    sim_st sim_ctx = {0U};
    if (sim_init(&sim_ctx) == 0)
    {
        swicc_ret_et ret = swicc_net_client_sig_register(sig_exit_handler);
        if (ret == SWICC_RET_SUCCESS)
        {
            ret = swicc_net_client_create(&client_ctx, "127.0.0.1", "37324");
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
    return 0;
}

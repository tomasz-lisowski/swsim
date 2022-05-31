#include "sig.h"
#include "io.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sig_exit_handler(__attribute__((unused)) int signum)
{
    printf("Shutting down...\n");
    if (io_fini() == 0)
    {
        exit(0);
    }
    exit(-1);
}

int32_t sig_exit_handler_register()
{
    struct sigaction action_new, action_old;
    action_new.sa_handler = sig_exit_handler;
    if (sigemptyset(&action_new.sa_mask) != 0)
    {
        printf("Call to sigemptyset() failed: %s.\n", strerror(errno));
        return -1;
    }
    action_new.sa_flags = 0;

    if (sigaction(SIGINT, NULL, &action_old) == 0)
    {
        if (action_old.sa_handler != SIG_IGN)
        {
            if (sigaction(SIGINT, &action_new, NULL) == 0)
            {
                if (sigaction(SIGHUP, NULL, &action_old) == 0)
                {
                    if (action_old.sa_handler != SIG_IGN)
                    {
                        if (sigaction(SIGHUP, &action_new, NULL) == 0)
                        {
                            if (sigaction(SIGTERM, NULL, &action_old) == 0)
                            {
                                if (action_old.sa_handler != SIG_IGN)
                                {
                                    if (sigaction(SIGTERM, &action_new, NULL) ==
                                        0)
                                    {
                                        return 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    /**
     * Very difficult to recover here such that the state before and after call
     * (in terms of signal handler registration) so better to exit here.
     */
    printf("Failed to register signal handler for one or more signals\n");
    exit(-1);
}

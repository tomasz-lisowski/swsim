#pragma once

#include <stdint.h>

/**
 * @brief Register a handler for SIGINT, SIGTERM, and SIGHUP, i.e. all signals
 * requesting the program to exit.
 * @return 0 on success, -1 on failure.
 * @note May exit if gets itself to an unrecoverable state.
 */
int32_t sig_exit_handler_register();

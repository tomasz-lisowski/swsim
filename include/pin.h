#pragma once
/**
 * For managing the application local PIN.
 * @note The PIN lengths used here are byte counts not digit counts. PINs are
 * stored using a binary coded decimal (BCD) encoding so one byte can hold two
 * digits.
 */

#include <stdbool.h>
#include <stdint.h>

#define PIN_LEN_MAX 4U /* bytes (8 digits) */
#define PIN_RETRIES 10U
#define PIN_COUNT_MAX 5U

typedef struct pin_s
{
    uint8_t val[PIN_LEN_MAX];
    uint8_t len;
    uint8_t retries;
    bool enabled;
    bool verified;
    bool blocked;
} pin_st;

#pragma once

#include <stdint.h>
#include <swicc/swicc.h>

typedef struct msg_hdr_s
{
    uint32_t size;
} __attribute__((packed)) msg_hdr_st;

typedef struct msg_data_s
{
    uint32_t cont_state;

    /**
     * When sent to SWSIM, this is unused, when being received by a client, this
     * indicates how many bytes to read from the interface (i.e. the expected
     * buffer length).
     */
    uint32_t buf_len_exp;

    uint8_t buf[SWICC_DATA_MAX];
} __attribute__((packed)) msg_data_st;

/**
 * Expected message format as seen coming from the client.
 * @note Might want to create a raw and internal representation of the message
 * if using sizeof(msg.data) feels unsafe.
 */
typedef struct msg_s
{
    msg_hdr_st hdr;
    msg_data_st data;
} __attribute__((packed)) msg_st;

/**
 * @brief Initialize IO before receiving data for SIM.
 * @param port_str Port that clients will be able to connect to for sending
 * data.
 * @return 0 on success, -1 on failure.
 */
int32_t io_init(char const *const port_str);

/**
 * @brief Receive a message.
 * @param msg Where the received message will be written.
 * @param init If the message contains some data that should be sent before
 * receiving.
 * @return 0 on success, 1 if a retry is needed, -1 on failure.
 */
int32_t io_recv(msg_st *const msg, bool const init);

/**
 * @brief Send a message.
 * @param msg Message to send.
 * @return 0 on success, -1 on failure.
 */
int32_t io_send(msg_st const *const msg);

/**
 * @brief De-initialize the IO. On success, a call to any of the IO functions is
 * undefined behavior.
 * @return 0 on success, -1 on failure.
 */
int32_t io_fini();

void io_dbg_msg_print(msg_st const *const msg);

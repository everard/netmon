// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#include <stdbool.h>
#include <threads.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Types and constants.
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    char const* ptr;
    size_t size;
} lnm_byte_sequence;

typedef struct {
    lnm_byte_sequence if_name;
    uint64_t rx[2], tx[2];
    uint64_t d_rx[2], d_tx[2];
    bool update_succeeded[2];
} lnm_state;

static char const* const lnm_program_name = "netmon";
enum { lnm_line_buf_size = 2048 };

////////////////////////////////////////////////////////////////////////////////
// State-update-related functions.
////////////////////////////////////////////////////////////////////////////////

static bool
lnm_skip_line(FILE* stream);

static bool
lnm_read_line(FILE* stream, char line_buf[static lnm_line_buf_size]);

static lnm_byte_sequence
lnm_read_word(lnm_byte_sequence* buf);

static bool
lnm_convert_to_u64(lnm_byte_sequence src, uint64_t* dst);

static void
lnm_update_state(lnm_state* state);

////////////////////////////////////////////////////////////////////////////////
// Output functions.
////////////////////////////////////////////////////////////////////////////////

static void
lnm_print_speed(char dst[static 4], uint64_t x);

static void
lnm_print_state(lnm_state const* state);

static void
lnm_print_usage(FILE* stream);

static void
lnm_print_program_description(FILE* stream);

static void
lnm_print_error_message_and_exit(char const* message);

////////////////////////////////////////////////////////////////////////////////
// Implementation of the state-update-related functions.
////////////////////////////////////////////////////////////////////////////////

bool
lnm_skip_line(FILE* stream) {
    static int const last_char_i = lnm_line_buf_size - 1;
    char line_buf[lnm_line_buf_size];

    while(true) {
        if(fgets(line_buf, lnm_line_buf_size, stream) == NULL) {
            return false;
        }

        line_buf[last_char_i] = '\0';

        if((strlen(line_buf) == (size_t)(last_char_i)) &&
           (line_buf[last_char_i - 1] != '\n')) {
            continue;
        }

        break;
    }

    return true;
}

bool
lnm_read_line(FILE* stream, char line_buf[static lnm_line_buf_size]) {
    return (fgets(line_buf, lnm_line_buf_size, stream) != NULL);
}

lnm_byte_sequence
lnm_read_word(lnm_byte_sequence* buf) {
    if(buf->size == 0) {
        return (lnm_byte_sequence){.ptr = buf->ptr, .size = 0};
    }

    size_t i = 0;

    // Skip spaces and tabs.
    for(; (i != buf->size) && ((buf->ptr[i] == ' ') || (buf->ptr[i] == '\t'));
        ++i) {
    }

    lnm_byte_sequence result = {.ptr = &(buf->ptr[i]), .size = 0};

    // Read the word.
    for(; (i != buf->size) && !((buf->ptr[i] == ' ') || (buf->ptr[i] == '\t'));
        ++i, ++(result.size)) {
    }

    // Shrink source buffer.
    buf->size -= i;
    buf->ptr += i;

    return result;
}

bool
lnm_convert_to_u64(lnm_byte_sequence src, uint64_t* dst) {
    if(src.size == 0) {
        return false;
    }

    *dst = 0;
    uint64_t m = 1;

    for(ptrdiff_t i = (ptrdiff_t)(src.size - 1); i >= 0; --i) {
        if(!((src.ptr[i] >= '0') && (src.ptr[i] <= '9'))) {
            return false;
        }

        *dst += ((uint64_t)(src.ptr[i] - '0')) * m;
        m *= 10;
    }

    return true;
}

void
lnm_update_state(lnm_state* state) {
    bool update_succeeded = false;

    // Open the device.
    FILE* dev = fopen("/proc/net/dev", "r");
    if(dev == NULL) {
        goto cleanup;
    }

    // Skip header.
    if(!(lnm_skip_line(dev) && lnm_skip_line(dev))) {
        goto cleanup;
    }

    char line_buf[lnm_line_buf_size] = {};
    lnm_byte_sequence if_name = state->if_name;
    lnm_byte_sequence rx_val_str = {}, tx_val_str = {};

    // Read line-by-line.
    for(; lnm_read_line(dev, line_buf);) {
        lnm_byte_sequence seq = {line_buf, strlen(line_buf)};
        lnm_byte_sequence word = lnm_read_word(&seq);

        if((word.size == 0) || (word.ptr[word.size - 1] != ':')) {
            continue;
        }

        // Skip all interfaces except for the one specified.
        if(((word.size - 1) != if_name.size) ||
           (memcmp(word.ptr, if_name.ptr, if_name.size) != 0)) {
            continue;
        }

        // Read rx and tx fields.
        rx_val_str = lnm_read_word(&seq);

        for(size_t i = 0; i < 7; ++i) {
            (void)lnm_read_word(&seq);
        }

        tx_val_str = lnm_read_word(&seq);

        break;
    }

    uint64_t rx = 0, tx = 0;
    if(lnm_convert_to_u64(rx_val_str, &rx) &&
       lnm_convert_to_u64(tx_val_str, &tx)) {
        update_succeeded = true;

        state->rx[0] = state->rx[1];
        state->rx[1] = rx;

        state->tx[0] = state->tx[1];
        state->tx[1] = tx;

        uint64_t d_rx =
            (state->rx[1] >= state->rx[0])
                ? (state->rx[1] - state->rx[0])
                : (state->rx[1] + (0xFFFFFFFFFFFFFFFFULL - state->rx[0]));
        uint64_t d_tx =
            (state->tx[1] >= state->tx[0])
                ? (state->tx[1] - state->tx[0])
                : (state->tx[1] + (0xFFFFFFFFFFFFFFFFULL - state->tx[0]));

        d_rx = (d_rx >> 1) + (uint64_t)(d_rx != 0);
        d_tx = (d_tx >> 1) + (uint64_t)(d_tx != 0);

        state->d_rx[0] = state->d_rx[1];
        state->d_rx[1] = d_rx;

        state->d_tx[0] = state->d_tx[1];
        state->d_tx[1] = d_tx;
    }

cleanup:
    if(dev != NULL) {
        fclose(dev);
    }

    state->update_succeeded[0] = state->update_succeeded[1];
    state->update_succeeded[1] = update_succeeded;
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of the output functions.
////////////////////////////////////////////////////////////////////////////////

void
lnm_print_speed(char dst[static 4], uint64_t x) {
    static char const prefixes[] = {
        'B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};

    char c = '0';
    for(size_t i = 0; i != sizeof(prefixes); ++i) {
        if(x < 10) {
            dst[0] = (char)('0' + x);
            dst[1] = '.';
            dst[2] = c;
            dst[3] = prefixes[i];
            break;
        } else {
            uint16_t x_r = (uint32_t)(x & 0x3FF);
            c = ((x_r == 0) ? '0' : (char)('1' + (x_r / 114)));
        }

        x >>= 10;
    }
}

void
lnm_print_state(lnm_state const* state) {
    bool must_print =
        ((!state->update_succeeded[1]) ||
         (state->update_succeeded[0] != state->update_succeeded[1]) ||
         (state->d_rx[0] != state->d_rx[1]) ||
         (state->d_tx[0] != state->d_tx[1]));

    if(!must_print) {
        return;
    }

    char buf[17] = {};
    lnm_print_speed(&buf[0], state->d_rx[1]);
    lnm_print_speed(&buf[8], state->d_tx[1]);

    buf[4] = 0xE2;
    buf[5] = 0x96;
    buf[6] = 0xBC;
    buf[7] = ' ';

    buf[12] = 0xE2;
    buf[13] = 0x96;
    buf[14] = 0xB2;

    buf[15] = '\n';
    buf[16] = '\0';

    if(!state->update_succeeded[1]) {
        buf[3] = '?';
        buf[11] = '?';
    }

    fputs(buf, stdout);
    fflush(stdout);
}

void
lnm_print_usage(FILE* stream) {
    fprintf(
        stream, "usage: %s [-h] [-c NETWORK_INTERFACE]\n", lnm_program_name);
}

void
lnm_print_program_description(FILE* stream) {
    fprintf(stream, "Monitors the state of the given network interface\n\n");
    lnm_print_usage(stream);

    fprintf(stream,
            "optional arguments:\n"
            "  -h        show this help message and exit\n"
            "  -c NETWORK_INTERFACE\n"
            "            monitor the state of the given network interface\n");
}

void
lnm_print_error_message_and_exit(char const* message) {
    fprintf(stderr, "%s: error: %s\n", lnm_program_name, message);
    lnm_print_usage(stderr);

    exit(EXIT_FAILURE);
}

////////////////////////////////////////////////////////////////////////////////
// Program entry point.
////////////////////////////////////////////////////////////////////////////////

int
main(int argc, char* argv[]) {
    lnm_state state = {};

    if(argc == 2) {
        if(strcmp(argv[1], "-h") == 0) {
            lnm_print_program_description(stdout);
            return EXIT_SUCCESS;
        } else {
            lnm_print_error_message_and_exit("wrong number of arguments");
        }
    } else if(argc == 3) {
        if(strcmp(argv[1], "-c") == 0) {
            state.if_name =
                (lnm_byte_sequence){.ptr = argv[2], .size = strlen(argv[2])};
        } else {
            lnm_print_error_message_and_exit("unknown argument");
        }
    } else {
        lnm_print_error_message_and_exit("wrong number of arguments");
    }

    struct timespec duration = {.tv_sec = 2, .tv_nsec = 0}, remaining = {};
    for(lnm_update_state(&state); true;) {
        if((remaining.tv_sec != 0) || (remaining.tv_nsec != 0)) {
            thrd_sleep(&remaining, &remaining);
            continue;
        }

        lnm_update_state(&state);
        lnm_print_state(&state);

        remaining = (struct timespec){};
        thrd_sleep(&duration, &remaining);
    }

    return EXIT_SUCCESS;
}

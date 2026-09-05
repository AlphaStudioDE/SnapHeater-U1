#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    char bytes[16384];
    size_t used, frame_start, frame_received, frame_length;
    bool active;
} shu1_ws_buffer_t;

// ESP-IDF may split one frame into events; WebSocket may split a message into frames.
static inline bool shu1_ws_accumulate(shu1_ws_buffer_t *b, uint8_t opcode, bool fin,
                                      int payload_length, int offset, const char *data,
                                      int length, size_t *complete_length) {
    *complete_length = 0;
    if (opcode != 0 && opcode != 1) return false; // control frames may interleave
    if (offset == 0) {
        if (opcode == 1) { b->used = 0; b->active = true; }
        else if (!b->active || b->frame_received != b->frame_length) goto invalid;
        b->frame_start = b->used;
        b->frame_received = 0;
        b->frame_length = payload_length >= 0 ? (size_t)payload_length : 0;
    }
    if (!b->active || length < 0 || offset < 0 || payload_length < 0 ||
        (length && !data) || (size_t)offset != b->frame_received ||
        (size_t)payload_length != b->frame_length ||
        (size_t)offset > b->frame_length || (size_t)length > b->frame_length - (size_t)offset ||
        b->frame_length >= sizeof(b->bytes) - b->frame_start) goto invalid;
    if (length) memcpy(b->bytes + b->used, data, (size_t)length);
    b->used += (size_t)length;
    b->frame_received += (size_t)length;
    if (fin && b->frame_received == b->frame_length) {
        b->bytes[b->used] = '\0';
        *complete_length = b->used;
        b->active = false;
        return true;
    }
    return false;
invalid:
    b->active = false;
    b->used = 0;
    return false;
}

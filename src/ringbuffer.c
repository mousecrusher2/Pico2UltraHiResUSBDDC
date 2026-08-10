/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include "ringbuffer.h"
#include <stdint.h>
#include <string.h>

void initialize_ringbuffer(
    int32_t *const storage,
    uint32_t size,
    bool no_spinlock,
    RINGBUFFER *const ringbuffer
) {
    ringbuffer->no_spinlock = no_spinlock;
    if (!no_spinlock) {
        ringbuffer->spinlock = spin_lock_init(spin_lock_claim_unused(true));
    }

    ringbuffer->size_buffer = size;
    ringbuffer->write_point = 0;
    ringbuffer->read_point = 0;
    ringbuffer->size_using = 0;
    ringbuffer->buffer = storage;
}

void clear_ringbuffer(RINGBUFFER *const ringbuffer) {
    ringbuffer->write_point = 0;
    ringbuffer->read_point = 0;
    ringbuffer->size_using = 0;
}

bool __not_in_flash_func(ringbuffer_is_full)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->write_point - ringbuffer->read_point >= ringbuffer->size_buffer;
}

int64_t __not_in_flash_func(get_size_using)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->size_using;
}

int64_t __not_in_flash_func(get_size_remain)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->size_buffer - ringbuffer->size_using;
}

uint32_t __not_in_flash_func(get_read_point)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->read_point;
}

uint32_t __not_in_flash_func(get_write_point)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->write_point;
}

int16_t __not_in_flash_func(ringbuf_write_spinlock)(int32_t input, RINGBUFFER *const ringbuffer) {
    if (ringbuffer->size_using == ringbuffer->size_buffer) {
        return -1; // buffer is full
    }

    *(ringbuffer->buffer + (ringbuffer->write_point)) = input;
    const volatile uint32_t owner = spin_lock_blocking(ringbuffer->spinlock); // for spinlock
    ringbuffer->write_point++;
    if (ringbuffer->write_point >= ringbuffer->size_buffer) {
        ringbuffer->write_point = 0;
    }
    ringbuffer->size_using++;
    spin_unlock(ringbuffer->spinlock, owner);
    return 1;
}

int16_t __not_in_flash_func(ringbuf_write_no_spinlock)(
    int32_t input,
    RINGBUFFER *const ringbuffer
) {
    if (ringbuffer->size_using == ringbuffer->size_buffer) {
        return -1; // buffer is full
    }

    *(ringbuffer->buffer + (ringbuffer->write_point)) = input;
    ringbuffer->write_point++;
    if (ringbuffer->write_point >= ringbuffer->size_buffer) {
        ringbuffer->write_point = 0;
    }
    ringbuffer->size_using++;
    return 1;
}

int16_t __not_in_flash_func(ringbuf_read_spinlock)(
    int32_t *const output,
    RINGBUFFER *const ringbuffer
) {
    if (ringbuffer->size_using == 0) {
        return -1; // buffer is empty
    }

    *output = *(ringbuffer->buffer + (ringbuffer->read_point));

    const volatile uint32_t owner = spin_lock_blocking(ringbuffer->spinlock); // for spinlock
    ringbuffer->read_point++;
    if (ringbuffer->read_point >= ringbuffer->size_buffer) {
        ringbuffer->read_point = 0;
    }
    ringbuffer->size_using--;
    spin_unlock(ringbuffer->spinlock, owner);
    return 1;
}

int16_t __not_in_flash_func(ringbuf_read_no_spinlock)(
    int32_t *const output,
    RINGBUFFER *const ringbuffer
) {
    if (ringbuffer->size_using == 0) {
        return -1; // buffer is empty
    }

    *output = *(ringbuffer->buffer + (ringbuffer->read_point));

    ringbuffer->read_point++;
    if (ringbuffer->read_point >= ringbuffer->size_buffer) {
        ringbuffer->read_point = 0;
    }
    ringbuffer->size_using--;
    return 1;
}

int64_t __not_in_flash_func(ringbuf_read_array_spinlock)(
    int32_t *const output,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (ringbuffer->size_using == 0 || ringbuffer->size_using < size) {
        return -1; // buffer is empty, or now buffer usage is not bigger than requested size
    }

    const volatile uint32_t owner = spin_lock_blocking(ringbuffer->spinlock); // for spinlock

    const uint32_t rx1_size = ((ringbuffer->read_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - ringbuffer->read_point
        : size;
    const uint32_t rx2_size = size - rx1_size;

    memcpy(output, ringbuffer->buffer + ringbuffer->read_point, sizeof(int32_t) * rx1_size);
    if (rx2_size > 0) {
        memcpy(output + rx1_size, ringbuffer->buffer, sizeof(int32_t) * rx2_size);
    }

    uint32_t inner_read_point = ringbuffer->read_point + size;
    if (inner_read_point > ringbuffer->size_buffer) {
        inner_read_point -= ringbuffer->size_buffer;
    }
    ringbuffer->read_point = inner_read_point;

    ringbuffer->size_using -= size;

    spin_unlock(ringbuffer->spinlock, owner);
    return size;
}

int64_t __not_in_flash_func(ringbuf_read_array_no_spinlock)(
    int32_t *const output,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (ringbuffer->size_using == 0 || ringbuffer->size_using < size) {
        return -1; // buffer is empty, or now buffer usage is not bigger than requested size
    }

    const uint32_t rx1_size = ((ringbuffer->read_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - ringbuffer->read_point
        : size;
    const uint32_t rx2_size = size - rx1_size;

    memcpy(output, ringbuffer->buffer + ringbuffer->read_point, sizeof(int32_t) * rx1_size);
    if (rx2_size > 0) {
        memcpy(output + rx1_size, ringbuffer->buffer, sizeof(int32_t) * rx2_size);
    }

    uint32_t inner_read_point = ringbuffer->read_point + size;
    if (inner_read_point > ringbuffer->size_buffer) {
        inner_read_point -= ringbuffer->size_buffer;
    }
    ringbuffer->read_point = inner_read_point;

    ringbuffer->size_using -= size;

    return size;
}

int64_t __not_in_flash_func(ringbuf_write_array_spinlock)(
    const int32_t *const input,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    const volatile int64_t remain_size = ringbuffer->size_buffer - ringbuffer->size_using;

    if (ringbuffer->size_using == ringbuffer->size_buffer || remain_size < size) {
        return -1; // buffer is full
    }

    if (size <= 0) {
        return 0;
    }

    const volatile uint32_t owner = spin_lock_blocking(ringbuffer->spinlock); // for spinlock

    const uint32_t tx1_size = ((ringbuffer->write_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - ringbuffer->write_point
        : size;
    const uint32_t tx2_size = size - tx1_size;

    memcpy(ringbuffer->buffer + ringbuffer->write_point, input, sizeof(int32_t) * tx1_size);
    if (tx2_size > 0) {
        memcpy(ringbuffer->buffer, input + tx1_size, sizeof(int32_t) * tx2_size);
    }

    uint32_t inner_write_point = ringbuffer->write_point + size;
    if (inner_write_point > ringbuffer->size_buffer) {
        inner_write_point -= ringbuffer->size_buffer;
    }
    ringbuffer->write_point = inner_write_point;

    ringbuffer->size_using += size;

    spin_unlock(ringbuffer->spinlock, owner);
    return size;
}

int64_t __not_in_flash_func(ringbuf_write_array_no_spinlock)(
    const int32_t *const input,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    const volatile int64_t remain_size = ringbuffer->size_buffer - ringbuffer->size_using;

    if (ringbuffer->size_using == ringbuffer->size_buffer || remain_size < size) {
        return -1; // buffer is full
    }

    const uint32_t tx1_size = ((ringbuffer->write_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - ringbuffer->write_point
        : size;
    const uint32_t tx2_size = size - tx1_size;

    memcpy(ringbuffer->buffer + ringbuffer->write_point, input, sizeof(int32_t) * tx1_size);
    if (tx2_size > 0) {
        memcpy(ringbuffer->buffer, input + tx1_size, sizeof(int32_t) * tx2_size);
    }

    uint32_t inner_write_point = ringbuffer->write_point + size;
    if (inner_write_point > ringbuffer->size_buffer) {
        inner_write_point -= ringbuffer->size_buffer;
    }
    ringbuffer->write_point = inner_write_point;

    ringbuffer->size_using += size;

    return size;
}

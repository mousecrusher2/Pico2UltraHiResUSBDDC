/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include "ringbuffer.h"
#include <stdatomic.h>
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
    atomic_init(&ringbuffer->write_point, 0);
    atomic_init(&ringbuffer->read_point, 0);
    atomic_init(&ringbuffer->size_using, 0);
    ringbuffer->buffer = storage;
}

void clear_ringbuffer(RINGBUFFER *const ringbuffer) {
    if (ringbuffer->no_spinlock) {
        const uint32_t interrupt_state = save_and_disable_interrupts();
        atomic_store_explicit(&ringbuffer->write_point, 0, memory_order_relaxed);
        atomic_store_explicit(&ringbuffer->read_point, 0, memory_order_relaxed);
        atomic_store_explicit(&ringbuffer->size_using, 0, memory_order_release);
        restore_interrupts(interrupt_state);
        return;
    }

    const uint32_t owner = spin_lock_blocking(ringbuffer->spinlock);
    atomic_store_explicit(&ringbuffer->write_point, 0, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->read_point, 0, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, 0, memory_order_release);
    spin_unlock(ringbuffer->spinlock, owner);
}

bool __not_in_flash_func(ringbuffer_is_full)(const RINGBUFFER *const ringbuffer) {
    return atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire)
        >= ringbuffer->size_buffer;
}

int64_t __not_in_flash_func(get_size_using)(const RINGBUFFER *const ringbuffer) {
    return atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire);
}

int64_t __not_in_flash_func(get_size_remain)(const RINGBUFFER *const ringbuffer) {
    return ringbuffer->size_buffer
        - atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire);
}

int16_t __not_in_flash_func(ringbuf_write_spinlock)(int32_t input, RINGBUFFER *const ringbuffer) {
    const uint32_t owner = spin_lock_blocking(ringbuffer->spinlock);
    const uint32_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (size_using >= ringbuffer->size_buffer) {
        spin_unlock(ringbuffer->spinlock, owner);
        return -1; // buffer is full
    }

    uint32_t write_point = atomic_load_explicit(&ringbuffer->write_point, memory_order_relaxed);
    ringbuffer->buffer[write_point] = input;
    write_point++;
    if (write_point >= ringbuffer->size_buffer) {
        write_point = 0;
    }
    atomic_store_explicit(&ringbuffer->write_point, write_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, size_using + 1, memory_order_release);
    spin_unlock(ringbuffer->spinlock, owner);
    return 1;
}

int16_t __not_in_flash_func(ringbuf_write_no_spinlock)(
    int32_t input,
    RINGBUFFER *const ringbuffer
) {
    const uint32_t interrupt_state = save_and_disable_interrupts();
    if (atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire)
        >= ringbuffer->size_buffer) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is full
    }

    uint32_t write_point = atomic_load_explicit(&ringbuffer->write_point, memory_order_relaxed);
    ringbuffer->buffer[write_point] = input;
    write_point++;
    if (write_point >= ringbuffer->size_buffer) {
        write_point = 0;
    }
    atomic_store_explicit(&ringbuffer->write_point, write_point, memory_order_relaxed);
    atomic_fetch_add_explicit(&ringbuffer->size_using, 1, memory_order_release);
    restore_interrupts(interrupt_state);
    return 1;
}

int16_t __not_in_flash_func(ringbuf_read_spinlock)(
    int32_t *const output,
    RINGBUFFER *const ringbuffer
) {
    const uint32_t owner = spin_lock_blocking(ringbuffer->spinlock);
    const uint32_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (size_using == 0) {
        spin_unlock(ringbuffer->spinlock, owner);
        return -1; // buffer is empty
    }

    uint32_t read_point = atomic_load_explicit(&ringbuffer->read_point, memory_order_relaxed);
    *output = ringbuffer->buffer[read_point];
    read_point++;
    if (read_point >= ringbuffer->size_buffer) {
        read_point = 0;
    }
    atomic_store_explicit(&ringbuffer->read_point, read_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, size_using - 1, memory_order_release);
    spin_unlock(ringbuffer->spinlock, owner);
    return 1;
}

int16_t __not_in_flash_func(ringbuf_read_no_spinlock)(
    int32_t *const output,
    RINGBUFFER *const ringbuffer
) {
    const uint32_t interrupt_state = save_and_disable_interrupts();
    if (atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire) == 0) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is empty
    }

    uint32_t read_point = atomic_load_explicit(&ringbuffer->read_point, memory_order_relaxed);
    *output = ringbuffer->buffer[read_point];
    read_point++;
    if (read_point >= ringbuffer->size_buffer) {
        read_point = 0;
    }
    atomic_store_explicit(&ringbuffer->read_point, read_point, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ringbuffer->size_using, 1, memory_order_release);
    restore_interrupts(interrupt_state);
    return 1;
}

int64_t __not_in_flash_func(ringbuf_read_array_spinlock)(
    int32_t *const output,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (size == 0) {
        return 0;
    }

    const uint32_t owner = spin_lock_blocking(ringbuffer->spinlock);
    const uint32_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (size_using < size) {
        spin_unlock(ringbuffer->spinlock, owner);
        return -1; // buffer is empty, or now buffer usage is not bigger than requested size
    }

    const uint32_t read_point = atomic_load_explicit(&ringbuffer->read_point, memory_order_relaxed);
    const uint32_t rx1_size = ((read_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - read_point
        : size;
    const uint32_t rx2_size = size - rx1_size;

    memcpy(output, ringbuffer->buffer + read_point, sizeof(int32_t) * rx1_size);
    if (rx2_size > 0) {
        memcpy(output + rx1_size, ringbuffer->buffer, sizeof(int32_t) * rx2_size);
    }

    uint32_t inner_read_point = read_point + size;
    if (inner_read_point >= ringbuffer->size_buffer) {
        inner_read_point -= ringbuffer->size_buffer;
    }
    atomic_store_explicit(&ringbuffer->read_point, inner_read_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, size_using - size, memory_order_release);

    spin_unlock(ringbuffer->spinlock, owner);
    return size;
}

int64_t __not_in_flash_func(ringbuf_read_array_no_spinlock)(
    int32_t *const output,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (size == 0) {
        return 0;
    }

    const uint32_t interrupt_state = save_and_disable_interrupts();
    if (atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire) < size) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is empty, or now buffer usage is not bigger than requested size
    }

    const uint32_t read_point = atomic_load_explicit(&ringbuffer->read_point, memory_order_relaxed);
    const uint32_t rx1_size = ((read_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - read_point
        : size;
    const uint32_t rx2_size = size - rx1_size;

    memcpy(output, ringbuffer->buffer + read_point, sizeof(int32_t) * rx1_size);
    if (rx2_size > 0) {
        memcpy(output + rx1_size, ringbuffer->buffer, sizeof(int32_t) * rx2_size);
    }

    uint32_t inner_read_point = read_point + size;
    if (inner_read_point >= ringbuffer->size_buffer) {
        inner_read_point -= ringbuffer->size_buffer;
    }
    atomic_store_explicit(&ringbuffer->read_point, inner_read_point, memory_order_relaxed);
    atomic_fetch_sub_explicit(&ringbuffer->size_using, size, memory_order_release);

    restore_interrupts(interrupt_state);
    return size;
}

int64_t __not_in_flash_func(ringbuf_write_array_spinlock)(
    const int32_t *const input,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (size == 0) {
        return 0;
    }

    const uint32_t owner = spin_lock_blocking(ringbuffer->spinlock);
    const uint32_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (ringbuffer->size_buffer - size_using < size) {
        spin_unlock(ringbuffer->spinlock, owner);
        return -1; // buffer is full
    }

    const uint32_t write_point =
        atomic_load_explicit(&ringbuffer->write_point, memory_order_relaxed);
    const uint32_t tx1_size = ((write_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - write_point
        : size;
    const uint32_t tx2_size = size - tx1_size;

    memcpy(ringbuffer->buffer + write_point, input, sizeof(int32_t) * tx1_size);
    if (tx2_size > 0) {
        memcpy(ringbuffer->buffer, input + tx1_size, sizeof(int32_t) * tx2_size);
    }

    uint32_t inner_write_point = write_point + size;
    if (inner_write_point >= ringbuffer->size_buffer) {
        inner_write_point -= ringbuffer->size_buffer;
    }
    atomic_store_explicit(&ringbuffer->write_point, inner_write_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, size_using + size, memory_order_release);

    spin_unlock(ringbuffer->spinlock, owner);
    return size;
}

int64_t __not_in_flash_func(ringbuf_write_array_no_spinlock)(
    const int32_t *const input,
    uint32_t size,
    RINGBUFFER *const ringbuffer
) {
    if (size == 0) {
        return 0;
    }

    const uint32_t interrupt_state = save_and_disable_interrupts();
    const uint32_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire);
    if (ringbuffer->size_buffer - size_using < size) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is full
    }

    const uint32_t write_point =
        atomic_load_explicit(&ringbuffer->write_point, memory_order_relaxed);
    const uint32_t tx1_size = ((write_point + size) > ringbuffer->size_buffer)
        ? ringbuffer->size_buffer - write_point
        : size;
    const uint32_t tx2_size = size - tx1_size;

    memcpy(ringbuffer->buffer + write_point, input, sizeof(int32_t) * tx1_size);
    if (tx2_size > 0) {
        memcpy(ringbuffer->buffer, input + tx1_size, sizeof(int32_t) * tx2_size);
    }

    uint32_t inner_write_point = write_point + size;
    if (inner_write_point >= ringbuffer->size_buffer) {
        inner_write_point -= ringbuffer->size_buffer;
    }
    atomic_store_explicit(&ringbuffer->write_point, inner_write_point, memory_order_relaxed);
    atomic_fetch_add_explicit(&ringbuffer->size_using, size, memory_order_release);

    restore_interrupts(interrupt_state);
    return size;
}

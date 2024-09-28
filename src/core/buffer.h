/*
 * SPDX-FileCopyrightText: 2024 Ben Jarvis
 *
 * SPDX-License-Identifier: LGPL
 */

#pragma once

#include <sys/uio.h>

#include "core/evpl.h"
#include "core/internal.h"

struct evpl_buffer {
    void               *data;
    int                 refcnt;
    unsigned int        used;
    unsigned int        size;

    void               *framework_private[EVPL_NUM_FRAMEWORK];
    struct evpl_buffer *next;
};

struct evpl_bvec_ring {
    struct evpl_bvec *bvec;
    int               size;
    int               mask;
    int               alignment;
    int               head;
    int               tail;
    uint64_t          length;
};

void evpl_buffer_release(
    struct evpl        *evpl,
    struct evpl_buffer *buffer);

/*
 * Copy 'length' bytes of data from 'buffer' into
 * an array of byte vectors 'bvecs'.
 * Sufficient vectors or space is not checked.
 */

static inline void
evpl_bvec_memcpy(
    struct evpl_bvec *bvecs,
    const void       *buffer,
    unsigned int      length)
{
    struct evpl_bvec *bvec = bvecs;
    const void       *ptr = buffer;
    unsigned int      left = length, chunk;

    while (left) {

        chunk = left;

        if (bvec->length < chunk) {
            chunk = bvec->length;
        }

        memcpy(bvec->data, ptr, chunk);

        ptr  += chunk;
        left -= chunk;
        bvec++;
    }

} // evpl_bvec_memcpy

static inline void
evpl_bvec_decref(
    struct evpl      *evpl,
    struct evpl_bvec *bvec)
{
    struct evpl_buffer *buffer = bvec->buffer;

    evpl_core_abort_if(buffer->refcnt == 0,
                       "Released bvec %p with zero refcnt", bvec);


    evpl_buffer_release(evpl, buffer);

} // evpl_bvec_decref

static inline void
evpl_bvec_incref(
    struct evpl      *evpl,
    struct evpl_bvec *bvec)
{
    struct evpl_buffer *buffer = bvec->buffer;

    ++buffer->refcnt;

} // evpl_bvec_incref



static inline unsigned int
evpl_buffer_left(struct evpl_buffer *buffer)
{
    return buffer->size - buffer->used;
} // evpl_buffer_left

static inline unsigned int
evpl_buffer_pad(
    struct evpl_buffer *buffer,
    unsigned int        alignment)
{
    return (alignment - (buffer->used & (alignment - 1))) & (alignment - 1);
} // evpl_buffer_pad

static inline void
evpl_bvec_ring_alloc(
    struct evpl_bvec_ring *ring,
    int                    size,
    int                    alignment)
{
    ring->bvec = evpl_valloc(size * sizeof(struct evpl_bvec), 64);

    ring->size      = size;
    ring->mask      = size - 1;
    ring->alignment = alignment;
    ring->head      = 0;
    ring->tail      = 0;

} // evpl_bvec_ring_alloc

static inline void
evpl_bvec_ring_free(struct evpl_bvec_ring *ring)
{
    evpl_free(ring->bvec);
} // evpl_bvec_ring_free

static inline void
evpl_bvec_ring_resize(struct evpl_bvec_ring *ring)
{
    int               new_size = ring->size << 1;
    struct evpl_bvec *new_bvec = evpl_valloc(
        new_size * sizeof(struct evpl_bvec), ring->alignment);

    if (ring->head > ring->tail) {
        memcpy(new_bvec, &ring->bvec[ring->tail], (ring->head - ring->tail) *
               sizeof(struct evpl_bvec));
    } else {
        memcpy(new_bvec, &ring->bvec[ring->tail], (ring->size - ring->tail) *
               sizeof(struct evpl_bvec));
        memcpy(&new_bvec[ring->size - ring->tail], ring->bvec, ring->head *
               sizeof(struct evpl_bvec));
    }

    ring->head = ring->size - 1;
    ring->tail = 0;

    evpl_free(ring->bvec);

    ring->bvec = new_bvec;
    ring->size = new_size;
    ring->mask = new_size - 1;
} // evpl_bvec_ring_resize

static inline int
evpl_bvec_ring_is_empty(const struct evpl_bvec_ring *ring)
{
    return ring->head == ring->tail;
} // evpl_bvec_ring_is_empty

static inline int
evpl_bvec_ring_is_full(const struct evpl_bvec_ring *ring)
{
    return ((ring->head + 1) & ring->mask) == ring->tail;
} // evpl_bvec_ring_is_full

static inline struct evpl_bvec *
evpl_bvec_ring_head(struct evpl_bvec_ring *ring)
{
    if (ring->head == ring->tail) {
        return NULL;
    } else {
        return &ring->bvec[ring->head];
    }
} // evpl_bvec_ring_head

static inline struct evpl_bvec *
evpl_bvec_ring_tail(struct evpl_bvec_ring *ring)
{
    if (ring->head == ring->tail) {
        return NULL;
    } else {
        return &ring->bvec[ring->tail];
    }
} // evpl_bvec_ring_tail

static inline struct evpl_bvec *
evpl_bvec_ring_next(
    struct evpl_bvec_ring *ring,
    struct evpl_bvec      *cur)
{
    int index = ((cur - ring->bvec) + 1) & ring->mask;

    if (index == ring->tail) {
        return NULL;
    }

    return &ring->bvec[index];
} // evpl_bvec_ring_next

static inline struct evpl_bvec *
evpl_bvec_ring_add(
    struct evpl_bvec_ring  *ring,
    const struct evpl_bvec *bvec,
    int                     eom)
{
    struct evpl_bvec *res;

    if (evpl_bvec_ring_is_full(ring)) {
        evpl_bvec_ring_resize(ring);
    }

    res = &ring->bvec[ring->head];

    ring->bvec[ring->head]     = *bvec;
    ring->bvec[ring->head].eom = eom;
    ring->head                 = (ring->head + 1) & ring->mask;

    return res;
} // evpl_bvec_ring_add

static inline void
evpl_bvec_ring_remove(struct evpl_bvec_ring *ring)
{
    ring->tail = (ring->tail + 1) & ring->mask;
} // evpl_bvec_ring_remove

static inline void
evpl_bvec_ring_clear(
    struct evpl           *evpl,
    struct evpl_bvec_ring *ring)
{
    struct evpl_bvec *bvec;

    while (ring->tail != ring->head) {
        bvec = &ring->bvec[ring->tail];
        evpl_bvec_release(evpl, bvec);
        ring->tail = (ring->tail + 1) & ring->mask;
    }

    ring->head = 0;
    ring->tail = 0;
} // evpl_bvec_ring_clear


static inline int
evpl_bvec_ring_iov(
    ssize_t               *r_total,
    struct iovec          *iov,
    int                    max_iov,
    int                    stop_on_eom,
    struct evpl_bvec_ring *ring)
{
    struct evpl_bvec *bvec;
    int               niov  = 0;
    int               pos   = ring->tail;
    int               total = 0;

    while (niov < max_iov && pos != ring->head) {
        bvec = &ring->bvec[pos];

        iov[niov].iov_base = bvec->data;
        iov[niov].iov_len  = bvec->length;
        niov++;
        total += bvec->length;

        if (stop_on_eom && bvec->eom) break;

        pos = (pos + 1) & ring->mask;
    }

    *r_total = total;

    return niov;
} // evpl_bvec_ring_iov

static inline void
evpl_bvec_ring_consume(
    struct evpl           *evpl,
    struct evpl_bvec_ring *ring,
    size_t                 length)
{
    struct evpl_bvec *bvec;

    while (length > 0 && ring->tail != ring->head) {

        bvec = &ring->bvec[ring->tail];

        if (bvec->length <= length) {
            evpl_bvec_release(evpl, bvec);
            length    -= bvec->length;
            ring->tail = (ring->tail + 1) & ring->mask;
        } else {
            bvec->data   += length;
            bvec->length -= length;

            length = 0;
        }
    }
} // evpl_bvec_ring_consume
static inline void
evpl_bvec_ring_append(
    struct evpl           *evpl,
    struct evpl_bvec_ring *ring,
    struct evpl_bvec      *append,
    int                    length,
    unsigned int           eom)
{
    struct evpl_bvec *head;

    head = evpl_bvec_ring_head(ring);

    if (head && !head->eom && head->data + head->length == append->data) {
        head->length += length;
    } else {
        evpl_bvec_incref(evpl, append);
        head         = evpl_bvec_ring_add(ring, append, eom);
        head->length = length;
    }

    append->data   += length;
    append->length -= length;

    if (append->length == 0) {
        evpl_bvec_decref(evpl, append);
    }
} // evpl_bvec_ring_append

static inline void *
evpl_buffer_private(
    struct evpl_buffer *buffer,
    int                 id)
{
    return buffer->framework_private[id];
} // evpl_buffer_private


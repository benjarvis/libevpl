/*
 * SPDX-FileCopyrightText: 2024 Ben Jarvis
 *
 * SPDX-License-Identifier: LGPL
 */

#pragma once

#include "core/evpl.h"

struct evpl_listener;
struct evpl_conn;
struct evpl_event;

typedef void (*evpl_event_read_callback_t)(
    struct evpl       *evpl,
    struct evpl_event *event);
typedef void (*evpl_event_write_callback_t)(
    struct evpl       *evpl,
    struct evpl_event *event);
typedef void (*evpl_event_error_callback_t)(
    struct evpl       *evpl,
    struct evpl_event *event);

#define EVPL_READABLE       0x01
#define EVPL_WRITABLE       0x02
#define EVPL_ERROR          0x04
#define EVPL_ACTIVE         0x08
#define EVPL_READ_INTEREST  0x10
#define EVPL_WRITE_INTEREST 0x20

#define EVPL_READ_READY     (EVPL_READABLE | EVPL_READ_INTEREST)
#define EVPL_WRITE_READY    (EVPL_WRITABLE | EVPL_WRITE_INTEREST)

struct evpl_event {
    int                         fd;
    unsigned int                flags;
    evpl_event_read_callback_t  read_callback;
    evpl_event_write_callback_t write_callback;
    evpl_event_error_callback_t error_callback;
};

void evpl_event_read_interest(
    struct evpl       *evpl,
    struct evpl_event *event);
void evpl_event_read_disinterest(
    struct evpl_event *event);
void evpl_event_write_interest(
    struct evpl       *evpl,
    struct evpl_event *event);
void evpl_event_write_disinterest(
    struct evpl_event *event);


void evpl_event_mark_readable(
    struct evpl       *evpl,
    struct evpl_event *event);

void evpl_event_mark_unreadable(
    struct evpl_event *event);

void evpl_event_mark_writable(
    struct evpl       *evpl,
    struct evpl_event *event);

void evpl_event_mark_unwritable(
    struct evpl_event *event);

void evpl_event_mark_error(
    struct evpl       *evpl,
    struct evpl_event *event);

void evpl_accept(
    struct evpl          *evpl,
    struct evpl_bind     *bind,
    struct evpl_bind     *new_bind);


void
evpl_add_event(
    struct evpl       *evpl,
    struct evpl_event *event);

/*
 * The evpl_core is always the first member of evpl,
 * so we can cast between them
 */

#define evpl_from_core(core) ((struct evpl *) core)

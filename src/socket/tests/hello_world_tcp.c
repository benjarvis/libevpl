/*
 * SPDX-FileCopyrightText: 2024 Ben Jarvis
 *
 * SPDX-License-Identifier: LGPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/uio.h>

#include "core/evpl.h"
#include "core/test_log.h"

const char hello[] = "Hello World!";
const int  hellolen = strlen(hello) + 1;


int
client_callback(
    struct evpl *evpl,
    struct evpl_bind *bind,
    unsigned int notify_type,
    unsigned int notify_code,
    void *private_data)
{
    struct evpl_bvec bvec;
    int nbvecs;
    int *run = private_data;

    switch (notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        evpl_test_info("client connected");
        break;
    case EVPL_NOTIFY_RECEIVED:

        nbvecs = evpl_recvv(evpl, bind, &bvec, 1, hellolen);

        if (nbvecs) {

            evpl_test_info("client received '%s'", bvec.data);

            evpl_bvec_release(evpl, &bvec);

        }

        break;

    case EVPL_NOTIFY_DISCONNECTED:
        evpl_test_info("client disconnected");
        *run = 0;
        break;
    }

    return 0;
}

void *
client_thread(void *arg)
{
    struct evpl *evpl;
    struct evpl_endpoint *ep;
    struct evpl_bind *bind;
    struct evpl_bvec bvec;
    int run = 1;

    evpl = evpl_create();

    ep = evpl_endpoint_create(evpl, "127.0.0.1", 8000);

    bind = evpl_connect(evpl, EVPL_SOCKET_TCP, ep, client_callback, &run);

    evpl_bvec_alloc(evpl, hellolen, 0, 1, &bvec);

    memcpy(evpl_bvec_data(&bvec), hello, hellolen);

    evpl_sendv(evpl, bind, &bvec, 1, hellolen);

    while (run) {
    
        evpl_wait(evpl, -1);
    }

    evpl_endpoint_close(evpl, ep);

    evpl_destroy(evpl);

    return NULL;
}

int server_callback(
    struct evpl *evpl,
    struct evpl_bind *bind,
    unsigned int notify_type,
    unsigned int notify_code,
    void *private_data)
{
    struct evpl_bvec bvec;
    int nbvecs;
    int *run = private_data;

    switch (notify_type) {
    case EVPL_NOTIFY_CONNECTED:
        evpl_test_info("server connected");
        break;
    case EVPL_NOTIFY_DISCONNECTED:
        evpl_test_info("server disconnected");
        *run = 0;
        break;
    case EVPL_NOTIFY_RECEIVED:

        nbvecs = evpl_recvv(evpl, bind, &bvec, 1, hellolen);

        if (nbvecs) {

            evpl_test_info("server received '%s'", bvec.data);

            evpl_bvec_release(evpl, &bvec);

            evpl_bvec_alloc(evpl, hellolen, 0, 1, &bvec);

            memcpy(evpl_bvec_data(&bvec), hello, hellolen);

            evpl_sendv(evpl, bind, &bvec, 1, hellolen);

            evpl_finish(evpl, bind);
        }
        break;
    }

    return 0;
}

void accept_callback(
    struct evpl_bind *bind,
    evpl_notify_callback_t *callback,
    void **conn_private_data,
    void       *private_data)
{
    const struct evpl_endpoint *ep = evpl_bind_endpoint(bind);

    evpl_test_info("Received connection from %s:%d",
        evpl_endpoint_address(ep),
        evpl_endpoint_port(ep));

    *callback = server_callback;
    *conn_private_data = private_data;
}
int
main(int argc, char *argv[])
{
    pthread_t thr;
    struct evpl *evpl;
    int run = 1;
    struct evpl_endpoint *ep;

    evpl_init(NULL);

    evpl = evpl_create();

    ep = evpl_endpoint_create(evpl, "0.0.0.0", 8000);

    evpl_listen(evpl, EVPL_SOCKET_TCP, ep, accept_callback, &run);

    pthread_create(&thr, NULL, client_thread, NULL);

    while (run) {
        evpl_wait(evpl, -1);
    }

    pthread_join(thr, NULL);

    evpl_endpoint_close(evpl, ep);

    evpl_destroy(evpl);

    evpl_cleanup();

    return 0;
}

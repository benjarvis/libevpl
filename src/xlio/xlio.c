#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>


#include "xlio.h"

#include "core/evpl.h"
#include "core/internal.h"
#include "core/protocol.h"

#include "common.h"
#include "epoll.h"

#define XLIO_DL_FN(xlio, name) { \
            xlio->name = dlsym(xlio->hdl, #name); \
            evpl_xlio_abort_if(!xlio->name, "No " #name \
                               " symbol found in XLIO library"); \
}

void *
evpl_xlio_init()
{
    struct evpl_xlio_api *api;
    int                   err;
    socklen_t             len;
    unsigned int          needed_caps;

    api = evpl_zalloc(sizeof(*api));

    setenv("XLIO_TRACELEVEL", "2", 0);
    setenv("XLIO_FORK", "0", 0);
    setenv("XLIO_MEM_ALLOC_TYPE", "ANON", 0);
    setenv("XLIO_SOCKETXTREME", "1", 1);

    api->hdl = dlopen("/opt/nvidia/lib/libxlio.so", RTLD_LAZY);

    evpl_xlio_abort_if(!api->hdl, "Failed to dynamically load XLIO library");

    XLIO_DL_FN(api, xlio_exit);
    XLIO_DL_FN(api, socket);
    XLIO_DL_FN(api, fcntl);
    XLIO_DL_FN(api, bind);
    XLIO_DL_FN(api, close);
    XLIO_DL_FN(api, recvmmsg);
    XLIO_DL_FN(api, sendmmsg);
    XLIO_DL_FN(api, accept);
    XLIO_DL_FN(api, listen);
    XLIO_DL_FN(api, connect);
    XLIO_DL_FN(api, getsockopt);
    XLIO_DL_FN(api, setsockopt);
    XLIO_DL_FN(api, readv);
    XLIO_DL_FN(api, writev);
    XLIO_DL_FN(api, epoll_create);
    XLIO_DL_FN(api, epoll_ctl);
    XLIO_DL_FN(api, epoll_wait);

    len = sizeof(api->extra);
    err = api->getsockopt(-2, SOL_SOCKET, SO_XLIO_GET_API, &api->extra, &len);

    evpl_xlio_abort_if(err < 0, "Failed to get XLIO extra API");

    evpl_xlio_abort_if(len < sizeof(struct xlio_api_t *) ||
                       api->extra == NULL ||
                       api->extra->magic != XLIO_MAGIC_NUMBER,
                       "XLIO xEtra API does not match header");

    needed_caps = XLIO_EXTRA_API_SOCKETXTREME_POLL |
        XLIO_EXTRA_API_GET_SOCKET_RINGS_NUM;

    evpl_xlio_abort_if((api->extra->cap_mask & needed_caps) != needed_caps,
                       "XLIO is missing socketxtreme capabilities");

    return api;
} /* evpl_xlio_init */

void
evpl_xlio_cleanup(void *private_data)
{
    struct evpl_xlio_api *api = private_data;

    api->xlio_exit();

    evpl_free(api);
} /* evpl_xlio_cleanup */

void *
evpl_xlio_create(
    struct evpl *evpl,
    void        *private_data)
{
    struct evpl_xlio *xlio;

    xlio = evpl_zalloc(sizeof(*xlio));

    xlio->api = private_data;

    xlio->num_ring_fds = 0;
    xlio->max_ring_fds = 256;

    xlio->ring_fds = evpl_zalloc(sizeof(struct evpl_xlio_ring_fd) * xlio->
                                 max_ring_fds);

    xlio->num_active_sockets = 0;
    xlio->max_active_sockets = 256;

    xlio->active_sockets = evpl_zalloc(sizeof(struct evpl_socket *) * xlio->
                                       max_active_sockets);


    return xlio;
} /* evpl_xlio_create */

void
evpl_xlio_destroy(
    struct evpl *evpl,
    void        *private_data)
{
    struct evpl_xlio *xlio = private_data;

    if (xlio->poll) {
        evpl_remove_poll(evpl, xlio->poll);
    }

    evpl_free(xlio->ring_fds);
    evpl_free(xlio->active_sockets);
    evpl_free(xlio);
} /* evpl_xlio_destroy */

struct evpl_framework evpl_framework_xlio = {
    .id      = EVPL_FRAMEWORK_XLIO,
    .name    = "XLIO",
    .init    = evpl_xlio_init,
    .cleanup = evpl_xlio_cleanup,
    .create  = evpl_xlio_create,
    .destroy = evpl_xlio_destroy,
};

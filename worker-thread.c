#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <assert.h>

#include "worker.h"
#include "worker-thread.h"

void*
worker_thread (void *arg)
{
    // TODO: obtain a pointer to a worker structure
    worker* wrk = NULL;
    assert (wrk != NULL);

    // TODO: initialize a first element with a control FD

    for (;;) {
        struct kevent received;

        int ret = kevent (wrk->kq, wrk->sets.events, wrk->sets.length, &received, 1, NULL);
        (void) ret;
    }
    return NULL;
}

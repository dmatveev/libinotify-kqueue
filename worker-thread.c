#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <assert.h>
#include <unistd.h> /* write */
#include <stdlib.h> /* calloc */
#include <stdio.h>  /* perror */

#include "inotify.h"
#include "worker.h"
#include "worker-thread.h"

static uint32_t
kqueue_to_inotify (uint32_t flags)
{
    uint32_t result = 0;

    if (flags & NOTE_ATTRIB)
        result |= IN_ATTRIB;
    // TODO: context-specific flags

    return result;
}

static void
process_command (worker *wrk)
{
    assert (wrk != NULL);

    // read a byte
    char unused;
    read (wrk->io[KQUEUE_FD], &unused, 1);

    if (wrk->cmd.type == WCMD_ADD) {
        wrk->cmd.retval = worker_add_or_modify (wrk,
                                                wrk->cmd.add.filename,
                                                wrk->cmd.add.mask);
    } else if (wrk->cmd.type == WCMD_REMOVE) {
        wrk->cmd.retval = worker_remove (wrk, wrk->cmd.rm_id);
    } else {
        // TODO: signal error
    }

    // TODO: is the situation when nobody else waits on a barrier possible?
    pthread_barrier_wait (&wrk->cmd.sync);
}

void*
worker_thread (void *arg)
{
    assert (arg != NULL);
    worker* wrk = (worker *) arg;

    for (;;) {
        struct kevent received;

        int ret = kevent (wrk->kq,
                          wrk->sets.events,
                          wrk->sets.length,
                          &received,
                          1,
                          NULL);
        if (ret == -1) {
            perror ("kevent failed");
            continue;
        }
        if (received.ident == wrk->io[KQUEUE_FD]) {
            process_command (wrk);
        } else {
            // TODO: write also name
            struct inotify_event *event = calloc (1, sizeof (struct inotify_event));
            event->wd = received.ident; /* remember that watch id is a fd? */
            event->mask = kqueue_to_inotify (received.fflags);
            write (wrk->io[KQUEUE_FD], event, sizeof (struct inotify_event));
            free (event);
        }
    }
    return NULL;
}

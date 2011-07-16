#include <sys/event.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* open() */
#include <assert.h>
#include <stdio.h>

#include "worker-thread.h"
#include "worker.h"


worker*
worker_create ()
{
    worker* wrk = calloc (1, sizeof (worker));

    if (wrk == NULL) {
        perror ("Failed to create a new worker");
        goto failure;
    }

    wrk->kq = kqueue ();
    if (wrk->kq == -1) {
        perror ("Failed to create a new kqueue");
        goto failure;
    }

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, wrk->io) == -1) {
        perror ("Failed to create a socket pair");
        goto failure;
    }

    worker_sets_init (&wrk->sets, wrk->io[1]);

    SIMPLEQ_INIT (&wrk->queue);
    pthread_mutex_init (&wrk->queue_mutex, NULL);

    /* create a run a worker thread */
    if (pthread_create (&wrk->thread, NULL, worker_thread, wrk) != 0) {
        perror ("Failed to start a new worker thread");
        goto failure;
    }

    return wrk;
    
    failure:
    worker_free (wrk);
    return NULL;
}


void
worker_free (worker *wrk)
{
    if (wrk == NULL)
        return;

    // TODO: implementation
}


int
worker_add_or_modify (worker     *wrk,
                      const char *path,
                      uint32_t    flags)
{
    assert (path != NULL);
    assert (wrk != NULL);
    // TODO: a pointer to sets?
    assert (wrk->sets.events != NULL);
    assert (wrk->sets.filenames != NULL);

    int i = 0;
    int fd = -1;

    for (i = 0; i < wrk->sets.length; i++) {
        if (wrk->sets.filenames[i] != NULL &&
            strcmp (path, wrk->sets.filenames[i]) == 0) {
            wrk->sets.events[i].fflags = flags;
            return i;
        }
    }

    // add a new entry if path is not found
    fd = open (path, O_RDONLY);
    if (fd != -1) {
        struct kevent *pevent; /* = NULL; ? */

        worker_sets_extend (&wrk->sets, 1);
        i = wrk->sets.length++;
        pevent = &wrk->sets.events[i];

        EV_SET (pevent,
                fd,
                EVFILT_VNODE,
                EV_ADD | EV_ENABLE | EV_ONESHOT,
                flags,
                0,
                0);
        wrk->sets.filenames[i] = strdup (path);

        /* The return value (opened file descriptor) will be used
         * as a inoty watch identifier.
         */
        return fd;
    } else
        return -1;
}


void
worker_remove (worker *wrk,
               int     id)
{
    // TODO: implementation
}

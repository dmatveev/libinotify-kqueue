#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <assert.h>
#include <stdio.h> /* perror */

#include "worker.h"
#include "worker-thread.h"

static void
process_commands (worker *wrk)
{
    assert (wrk != NULL);

    pthread_mutex_lock (&wrk->queue_mutex);

    // TODO: pointer to queue?
    while (!SIMPLEQ_EMPTY (&wrk->queue)) {
        worker_cmd *cmd = SIMPLEQ_FIRST (&wrk->queue);
        assert (cmd != NULL);

        if (cmd->type == WCMD_ADD) {
            *cmd->feedback.retval
                = worker_add_or_modify (wrk,cmd->add.filename, cmd->add.mask);
        }
        else if (cmd->type == WCMD_REMOVE) {
        }

        SIMPLEQ_REMOVE_HEAD (&wrk->queue, entries);
        pthread_barrier_wait (&cmd->sync);
    }

    pthread_mutex_unlock (&wrk->queue_mutex);
}

void*
worker_thread (void *arg)
{
    assert (arg != NULL);
    worker* wrk = (worker *) arg;
    worker_sets *sets = &wrk->sets;

    // TODO: initialize a first element with a control FD
    EV_SET (&sets->events[0],
            wrk->io[KQUEUE_FD],
            EVFILT_READ,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            NOTE_LOWAT,
            1,
            0);
    sets->length = 1;

    for (;;) {
        struct kevent received;

        int ret = kevent (wrk->kq, wrk->sets.events, wrk->sets.length, &received, 1, NULL);
        if (ret == -1) {
            perror ("kevent failed");
            continue;
        }

        if (received.ident == wrk->io[KQUEUE_FD]) {
            /* got a notification, process pending commands now */
            process_commands (wrk);
        }
    }
    return NULL;
}

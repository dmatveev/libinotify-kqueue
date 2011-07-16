#include <stddef.h> /* NULL */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "worker.h"
#include "inotify.h"


#define WORKER_SZ 100
static worker* workers[WORKER_SZ];
static pthread_mutex_t workers_mutex = PTHREAD_MUTEX_INITIALIZER;

int
inotify_init (void) __THROW
{
    // TODO: a dynamic structure here
    pthread_mutex_lock (&workers_mutex);
    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        if (workers[i] == NULL) {
            worker *wrk = worker_create ();
            if (wrk) {
                workers[i] = wrk;
                pthread_mutex_unlock (&workers_mutex);
                return wrk->io[INOTIFY_FD];
            }
        }
    }

    // TODO: errno is set when an original inotify_init fails
    pthread_mutex_unlock (&workers_mutex);
    return -1;
}

int
inotify_init1 (int flags) __THROW
{
    // TODO: implementation
    return 0;
}

int
inotify_add_watch (int         fd,
                   const char *name,
                   uint32_t    mask) __THROW
{
    /* look up for an appropriate thread */
    // TODO: lock workers when looking up
    pthread_mutex_lock (&workers_mutex);
    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        if (workers[i]->io[INOTIFY_FD] == fd) {
            worker *wrk = workers[i];
            worker_cmd *cmd = calloc (1, sizeof (worker_cmd));
            // TODO: check allocation
            int retval = -1; // TODO: no magic numbers here
            int error = 0;   // TODO: and here

            // TODO: hide these details
            cmd->type = WCMD_ADD;
            cmd->feedback.retval = &retval;
            cmd->feedback.error  = &error;
            cmd->add.filename = strdup (name);
            cmd->add.mask = mask;
            pthread_barrier_init (&cmd->sync, NULL, 2);

            pthread_mutex_lock (&wrk->queue_mutex);
            SIMPLEQ_INSERT_TAIL (&wrk->queue, cmd, entries);
            pthread_mutex_unlock (&wrk->queue_mutex);

            write (wrk->io[INOTIFY_FD], "*", 1); // TODO: EINTR
            pthread_barrier_wait (&cmd->sync);

            // TODO: hide these details too
            free (cmd->add.filename);
            free (cmd);

            // TODO: check error here
            pthread_mutex_unlock (&workers_mutex);
            return retval;
        }
    }

    pthread_mutex_unlock (&workers_mutex);
    return -1;
}

int
inotify_rm_watch (int fd,
                  int wd) __THROW
{
    // TODO: implementation
    return 0;
}

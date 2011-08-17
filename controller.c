#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h> /* printf */
#include <errno.h>

#include "worker.h"
#include "utils.h"
#include "inotify.h"


#define WORKER_SZ 100
static worker* volatile workers[WORKER_SZ] = {NULL};
static pthread_mutex_t workers_mutex = PTHREAD_MUTEX_INITIALIZER;

INO_EXPORT int
inotify_init (void) __THROW
{
    pthread_mutex_lock (&workers_mutex);

    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        if (workers[i] == NULL) {
            worker *wrk = worker_create ();
            if (wrk != NULL) {
                workers[i] = wrk;
                pthread_mutex_unlock (&workers_mutex);
                return wrk->io[INOTIFY_FD];
            }
        }
    }

    pthread_mutex_unlock (&workers_mutex);
    return -1;
}

/* INO_EXPORT int */
/* inotify_init1 (int flags) __THROW */
/* { */
/*     return 0; */
/* } */

INO_EXPORT int
inotify_add_watch (int         fd,
                   const char *name,
                   uint32_t    mask) __THROW
{
    pthread_mutex_lock (&workers_mutex);

    /* look up for an appropriate worker */
    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        worker *wrk = workers[i];
        if (wrk != NULL && wrk->io[INOTIFY_FD] == fd) {
            pthread_mutex_lock (&wrk->mutex);

            if (wrk->closed) {
                worker_free (wrk);
                pthread_mutex_unlock (&wrk->mutex);
                free (wrk);

                pthread_mutex_unlock (&workers_mutex);
                return -1;
            }

            worker_cmd_add (&wrk->cmd, name, mask);
            safe_write (wrk->io[INOTIFY_FD], "*", 1);

            worker_cmd_wait (&wrk->cmd);

            int retval = wrk->cmd.retval;
            pthread_mutex_unlock (&wrk->mutex);

            if (wrk->closed) {
                worker_free (wrk);
                free (wrk);
            }

            pthread_mutex_unlock (&workers_mutex);
            return retval;
        }
    }

    pthread_mutex_unlock (&workers_mutex);
    return -1;
}

INO_EXPORT int
inotify_rm_watch (int fd,
                  int wd) __THROW
{
    assert (fd != -1);
    assert (wd != -1);
    
    pthread_mutex_lock (&workers_mutex);

    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        worker *wrk = workers[i];
        if (wrk != NULL && wrk->io[INOTIFY_FD] == fd) {
            pthread_mutex_lock (&wrk->mutex);

            if (wrk->closed) {
                worker_free (wrk);
                pthread_mutex_unlock (&wrk->mutex);
                free (wrk);

                pthread_mutex_unlock (&workers_mutex);
                return -1;
            }

            worker_cmd_remove (&wrk->cmd, wd);
            safe_write (wrk->io[INOTIFY_FD], "*", 1);
            worker_cmd_wait (&wrk->cmd);

            int retval = wrk->cmd.retval;
            pthread_mutex_unlock (&wrk->mutex);

            if (wrk->closed) {
                worker_free (wrk);
                free (wrk);
            }

            pthread_mutex_unlock (&workers_mutex);
            return retval;
        }
    }
    
    pthread_mutex_unlock (&workers_mutex);
    return 0;
}

void
worker_erase (worker *wrk)
{
    assert (wrk != NULL);

    int i;
    for (i = 0; i < WORKER_SZ; i++) {
        if (workers[i] == wrk) {
            workers[i] = NULL;
            break;
        }
    }
}

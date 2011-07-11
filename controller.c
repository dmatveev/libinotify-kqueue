#include <stddef.h> /* NULL */
#include "worker.h"
#include "inotify.h"

int
inotify_init (void) __THROW
{
    // TODO: errno is set when an original inotify_init fails
    worker *wrk = worker_create ();
    return (wrk == NULL) ? -1 : wrk->io[INOTIFY_FD];
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
    /* here we will allocate a placeholder to receive a watch id
       from the worker thread. */
    return 0;
}

int
inotify_rm_watch (int fd,
                  int wd) __THROW
{
    // TODO: implementation
    return 0;
}

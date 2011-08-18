#include <assert.h>
#include <stdlib.h> /* realloc */
#include <string.h> /* memset */
#include <stddef.h> /* NULL */
#include <fcntl.h>  /* open, fstat */
#include <dirent.h> /* opendir, readdir, closedir */
#include <sys/event.h>

#include "sys/inotify.h"

#include "utils.h"
#include "worker-sets.h"


#define WS_RESERVED 10

int
worker_sets_init (worker_sets *ws,
                  int          fd)
{
    assert (ws != NULL);

    memset (ws, 0, sizeof (worker_sets));
    if (worker_sets_extend (ws, 1) == -1) {
        perror_msg ("Failed to initialize worker sets");
        return -1;
    }

    EV_SET (&ws->events[0],
            fd,
            EVFILT_READ,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            NOTE_LOWAT,
            1,
            0);
    ws->length = 1;
    return 0;
}

int
worker_sets_extend (worker_sets *ws,
                    int          count)
{
    assert (ws != NULL);

    if (ws->length + count > ws->allocated) {
        long to_allocate = ws->allocated + count + WS_RESERVED;

        void *ptr = NULL;
        ptr = realloc (ws->events, sizeof (struct kevent) * to_allocate);
        if (ptr == NULL) {
            perror_msg ("Failed to extend events memory in the worker sets");
            return -1;
        }
        ws->events = ptr;

        ptr = realloc (ws->watches, sizeof (struct watch *) * to_allocate);
        if (ptr == NULL) {
            perror_msg ("Failed to extend watches memory in the worker sets");
            return -1;
        }
        ws->watches = ptr;
        ws->watches[0] = NULL;

        ws->allocated = to_allocate;
    }
    return 0;
}

void
worker_sets_free (worker_sets *ws)
{
    assert (ws != NULL);
    assert (ws->events != NULL);
    assert (ws->watches != NULL);

    int i;
    for (i = 0; i < ws->length; i++) {
        if (ws->watches[i] != NULL) {
            watch_free (ws->watches[i]);
        }
    }

    free (ws->events);
    free (ws->watches);
    memset (ws, 0, sizeof (worker_sets));
}

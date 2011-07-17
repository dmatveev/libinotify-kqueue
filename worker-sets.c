#include <assert.h>
#include <stddef.h> /* NULL */
#include <sys/event.h>
#include "worker-sets.h"

void
worker_sets_init (worker_sets *ws,
                  int          fd)
{
    assert (ws != NULL);
    // TODO: allocate memory here

    EV_SET (&ws->events[0],
            fd,
            EVFILT_READ,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            NOTE_LOWAT,
            1,
            0);
    ws->length = 1;
}

int
worker_sets_extend (worker_sets *ws,
                    int          count)
{
    // TODO: implementation
    return -1;
}

void
worker_sets_free (worker_sets *ws)
{
    // TODO: implementation
}

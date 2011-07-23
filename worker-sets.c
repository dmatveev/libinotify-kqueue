#include <assert.h>
#include <stdlib.h> /* realloc */
#include <string.h> /* memset */
#include <stddef.h> /* NULL */
#include <fcntl.h>  /* open, fstat */
#include <stdio.h>  /* perror */
#include <sys/event.h>

#include "inotify.h"
#include "worker-sets.h"

static uint32_t
inotify_flags_to_kqueue (uint32_t flags, int is_directory)
{
    uint32_t result = 0;
    static const uint32_t NOTE_MODIFIED = (NOTE_WRITE | NOTE_EXTEND);

    if (flags & IN_ATTRIB)
        result |= NOTE_ATTRIB;
    if (flags & IN_MODIFY)
        result |= NOTE_MODIFIED;
    if (flags & IN_MOVED_FROM && is_directory)
        result |= NOTE_MODIFIED;
    if (flags & IN_MOVED_TO && is_directory)
        result |= NOTE_MODIFIED;
    if (flags & IN_CREATE && is_directory)
        result |= NOTE_MODIFIED;
    if (flags & IN_DELETE && is_directory)
        result |= NOTE_MODIFIED;
    if (flags & IN_DELETE_SELF)
        result |= NOTE_DELETE;
    if (flags & IN_MOVE_SELF)
        result |= NOTE_RENAME;

    return result;
}


static int
_is_directory (int fd)
{
    assert (fd != -1);

    struct stat st;
    memset (&st, 0, sizeof (struct stat));

    if (fstat (fd, &st) == -1) {
        perror ("fstat failed, assuming it is just a file");
        return 0;
    }

    return (st.st_mode & S_IFDIR) == S_IFDIR;
}

int
watch_init_user (watch *w, struct kevent *kv, const char *path, uint32_t flags, int index)
{
    assert (w != NULL);
    assert (kv != NULL);
    assert (path != NULL);

    memset (w, 0, sizeof (watch));
    memset (kv, 0, sizeof (struct kevent));

    int fd = open (path, O_RDONLY);
    if (fd == -1) {
        // TODO: error
        return -1;
    }

    w->type = WATCH_USER;
    w->flags = flags;
    w->is_directory = _is_directory (fd);
    w->filename = strdup (path);
    w->fd = fd;
    EV_SET (kv,
            fd,
            EVFILT_VNODE,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            inotify_flags_to_kqueue (flags, w->is_directory),
            0,
            index);

    return 0;
}

int
watch_init_dependency (watch *w, struct kevent *kv, const char *path, uint32_t flags, int index)
{
    assert (w != NULL);
    assert (kv != NULL);
    assert (path != NULL);

    memset (w, 0, sizeof (watch));
    memset (kv, 0, sizeof (struct kevent));

    int fd = open (path, O_RDONLY);
    if (fd == -1) {
        // TODO: error
        return -1;
    }

    w->type = WATCH_DEPENDENCY;
    w->flags = flags;
    w->filename = strdup (path);
    w->fd = fd;

    // TODO: drop some flags from flags.
    // They are actual for a parent watch, but should be modified
    // for dependant ones
    EV_SET (kv,
            fd,
            EVFILT_VNODE,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            inotify_flags_to_kqueue (flags, 0),
            0,
            index);

    return 0;
}


#define WS_RESERVED 10

void
worker_sets_init (worker_sets *ws,
                  int          fd)
{
    assert (ws != NULL);

    memset (ws, 0, sizeof (worker_sets));
    worker_sets_extend (ws, 1);

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
    assert (ws != NULL);

    if (ws->length + count > ws->allocated) {
        ws->allocated =+ (count + WS_RESERVED);
        ws->events = realloc (ws->events, sizeof (struct kevent) * ws->allocated);
        ws->watches = realloc (ws->watches, sizeof (struct watch) * ws->allocated);
        // TODO: check realloc fails
    }
    return 0;
}

void
worker_sets_free (worker_sets *ws)
{
    assert (ws != NULL);

    /* int i; */
    /* for (i = 0; i < ws->allocated; i++) { */
    /*     free (ws->filenames[i]); */
    /* } */
    /* free (ws->is_user); */
    /* free (ws->is_directory); */
    /* free (ws->events); */
    /* free (ws); */
}

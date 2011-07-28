#include <fcntl.h>  /* open */
#include <string.h> /* strdup */
#include <stdio.h>  /* perror */
#include <assert.h>

#include "conversions.h"
#include "watch.h"


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
            inotify_to_kqueue (flags, w->is_directory),
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
            inotify_to_kqueue (flags, 0),
            0,
            index);

    return 0;
}


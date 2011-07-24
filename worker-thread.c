#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <assert.h>
#include <unistd.h> /* write */
#include <stdlib.h> /* calloc */
#include <stdio.h>  /* perror */
#include <string.h> /* memset */

#include "inotify.h"
#include "worker.h"
#include "worker-sets.h"
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

void
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

// TODO: drop unnecessary arguments
void
produce_directory_moves (worker         *wrk,
                         watch          *w,
                         struct kevent  *event,
                         dep_list      **was, // TODO: removed
                         dep_list      **now) // TODO: added
{
    assert (wrk != NULL);
    assert (w != NULL);
    assert (event != NULL);
    assert (was != NULL);
    assert (now != NULL);

    dep_list *was_iter = *was;
    dep_list *was_prev = NULL;

    assert (was_iter != NULL);

    while (was_iter != NULL) {
        dep_list *now_iter = *now;
        dep_list *now_prev = NULL;

        int matched = 0;
        while (now_iter != NULL) {
            if (was_iter->inode == now_iter->inode) {
                matched = 1;
                printf ("%s was renamed to %s\n", was_iter->path, now_iter->path);

                if (was_prev) {
                    was_prev->next = was_iter->next;
                } else {
                    *was = was_iter->next;
                }

                if (now_prev) {
                    now_prev->next = now_iter->next;
                } else {
                    *now = now_iter->next;
                }
                // TODO: free smt
                break;
            }
        }

        dep_list *oldptr = was_iter;
        was_iter = was_iter->next;
        if (matched == 0) {
            was_prev = oldptr;
        } else {
            free (oldptr); // TODO: dl_free?
        }
    }
}

// TODO: drop unnecessary arguments
void
produce_directory_diff (worker *wrk, watch *w, struct kevent *event)
{
    assert (wrk != NULL);
    assert (w != NULL);
    assert (event != NULL);

    assert (w->type == WATCH_USER);
    assert (w->is_directory);

    dep_list *was = NULL, *now = NULL;
    was = dl_shallow_copy (w->deps);
    dl_shallow_free (w->deps);

    w->deps = dl_listing (w->filename);
    now = dl_shallow_copy (w->deps);

    dl_diff (&was, &now);

    produce_directory_moves (wrk, w, event, &was, &now);

    printf ("Removed:\n");
    dl_print (was);
    printf ("Added:\n");
    dl_print (now);

    dl_shallow_free (now);
    dl_free (was);
}

void
produce_notifications (worker *wrk, struct kevent *event)
{
    assert (wrk != NULL);
    assert (event != NULL);

    struct inotify_event ie;
    memset (&ie, 0, sizeof (struct inotify_event));

    if (wrk->sets.watches[event->udata].type == WATCH_USER) {
        if (wrk->sets.watches[event->udata].is_directory
            && (event->fflags & (NOTE_WRITE | NOTE_EXTEND))) {
            produce_directory_diff (wrk, &wrk->sets.watches[event->udata], event);
        }
        // TODO: write also name
        ie.wd = event->ident; /* remember that watch id is a fd? */
    } else {
        ie.wd = wrk->sets.watches[event->udata].parent->fd;
    }
    ie.mask = kqueue_to_inotify (event->fflags);

    // TODO: EINTR
    write (wrk->io[KQUEUE_FD], &ie, sizeof (struct inotify_event));
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
            produce_notifications (wrk, &received);
        }
    }
    return NULL;
}

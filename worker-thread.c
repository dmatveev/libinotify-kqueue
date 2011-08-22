/*******************************************************************************
  Copyright (c) 2011 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include <sys/event.h>
#include <stddef.h> /* NULL */
#include <assert.h>
#include <unistd.h> /* write */
#include <stdlib.h> /* calloc, realloc */
#include <string.h> /* memset */
#include <errno.h>

#include "sys/inotify.h"

#include "utils.h"
#include "conversions.h"
#include "worker.h"
#include "worker-sets.h"
#include "worker-thread.h"

typedef struct bulk_events {
    void *memory;
    size_t size;
} bulk_events;

int
bulk_write (bulk_events *be, void *mem, size_t size)
{
    assert (be != NULL);
    assert (mem != NULL);

    void *ptr = realloc (be->memory, be->size + size);
    if (ptr == NULL) {
        perror_msg ("Failed to extend the bulk events buffer");
        return -1;
    }

    be->memory = ptr;
    memcpy (be->memory + be->size, mem, size);
    be->size += size;
    return 0;
}

void
process_command (worker *wrk)
{
    assert (wrk != NULL);

    /* read a byte */
    char unused;
    safe_read (wrk->io[KQUEUE_FD], &unused, 1);

    if (wrk->cmd.type == WCMD_ADD) {
        wrk->cmd.retval = worker_add_or_modify (wrk,
                                                wrk->cmd.add.filename,
                                                wrk->cmd.add.mask);
    } else if (wrk->cmd.type == WCMD_REMOVE) {
        wrk->cmd.retval = worker_remove (wrk, wrk->cmd.rm_id);
    }

    /* TODO: is the situation when nobody else waits on a barrier possible */
    pthread_barrier_wait (&wrk->cmd.sync);
}

#define EXCLUDE_SIMILAR(removed_list, added_list, match_expr, matched_code) \
    assert (removed_list != NULL);                                      \
    assert (added_list != NULL);                                        \
                                                                        \
    dep_list *removed_list##_iter = *removed_list;                      \
    dep_list *removed_list##_prev = NULL;                               \
                                                                        \
    int productive = 0;                                                 \
                                                                        \
    while (removed_list##_iter != NULL) {                               \
        dep_list *added_list##_iter = *added_list;                      \
        dep_list *added_list##_prev = NULL;                             \
                                                                        \
        int matched = 0;                                                \
        while (added_list##_iter != NULL) {                             \
            if (match_expr) {                                           \
                matched = 1;                                            \
                matched_code;                                           \
                                                                        \
                ++productive;                                           \
                                                                        \
                if (removed_list##_prev) {                              \
                    removed_list##_prev->next = removed_list##_iter->next; \
                } else {                                                \
                    *removed_list = removed_list##_iter->next;          \
                }                                                       \
                if (added_list##_prev) {                                \
                    added_list##_prev->next = added_list##_iter->next;  \
                } else {                                                \
                    *added_list = added_list##_iter->next;              \
                }                                                       \
                break;                                                  \
            }                                                           \
            added_list##_iter = added_list##_iter->next;                \
        }                                                               \
        dep_list *oldptr = removed_list##_iter;                         \
        removed_list##_iter = removed_list##_iter->next;                \
        if (matched == 0) {                                             \
            removed_list##_prev = oldptr;                               \
        } else {                                                        \
            free (oldptr);                                              \
        }                                                               \
    }                                                                   \
    return (productive > 0);


int
produce_directory_replacements (worker        *wrk,
                                watch         *w,
                                dep_list    **removed,
                                dep_list    **current,
                                bulk_events  *be)
{
    assert (wrk != NULL);
    assert (w != NULL);
    assert (be != NULL);

    EXCLUDE_SIMILAR
        (removed, current,
         (removed_iter->inode == current_iter->inode),
         {
             uint32_t cookie = removed_iter->inode & 0x00000000FFFFFFFF;
             int event_len = 0;
             struct inotify_event *ev;

             ev = create_inotify_event (w->fd, IN_MOVED_FROM, cookie,
                                        removed_iter->path,
                                        &event_len);
             if (ev != NULL) {
                 bulk_write (be, ev, event_len);
                 free (ev);
             }  else {
                 perror_msg ("Failed to create a new IN_MOVED_FROM inotify event (*)");
             }

             ev = create_inotify_event (w->fd, IN_MOVED_TO, cookie,
                                        current_iter->path,
                                        &event_len);
             if (ev != NULL) {
                 bulk_write (be, ev, event_len);
                 free (ev);
             } else {
                 perror_msg ("Failed to create a new IN_MOVED_TO inotify event (*)");
             }

             int i;
             for (i = 1; i < wrk->sets.length; i++) {
                 watch *iw = wrk->sets.watches[i];
                 if (iw && iw->parent == w
                     && strcmp (current_iter->path, iw->filename) == 0) {
                     dep_list *dl = dl_create (iw->filename, iw->inode);
                     worker_remove_many (wrk, w, dl, 0);
                     dl_shallow_free (dl);
                     break;
                 }
             }
         });
}

int
produce_directory_overwrites (worker      *wrk,
                              watch       *w,
                              dep_list   **previous,
                              dep_list   **current,
                              bulk_events *be)
{
    assert (wrk != NULL);
    assert (w != NULL);
    assert (be != NULL);

    EXCLUDE_SIMILAR
        (previous, current,
         (strcmp (previous_iter->path, current_iter->path) == 0
          && previous_iter->inode != current_iter->inode),
         {
             int i;
             for (i = 0; i < wrk->sets.length; i++) {
                 watch *wi = wrk->sets.watches[i];
                 if (wi && (strcmp (wi->filename, current_iter->path) == 0)
                     && wi->parent == w) {
                     if (watch_reopen (wi) == -1) {
                         /* I dont know, what to do */
                         /* Not a very beautiful way to remove a single dependency */
                         dep_list *dl = dl_create (wi->filename, wi->inode);
                         worker_remove_many (wrk, w, dl, 0);
                         dl_shallow_free (dl);
                     } else {
                         uint32_t cookie = current_iter->inode & 0x00000000FFFFFFFF;
                         int event_len = 0;
                         struct inotify_event *ev;

                         ev = create_inotify_event (w->fd, IN_DELETE, cookie,
                                                    current_iter->path,
                                                    &event_len);
                         if (ev != NULL) {
                             bulk_write (be, ev, event_len);
                             free (ev);
                         }  else {
                             perror_msg ("Failed to create a new IN_DELETE inotify event (*)");
                         }

                         ev = create_inotify_event (w->fd, IN_CREATE, cookie,
                                                    current_iter->path,
                                                    &event_len);
                         if (ev != NULL) {
                             bulk_write (be, ev, event_len);
                             free (ev);
                         } else {
                             perror_msg ("Failed to create a new IN_CREATE inotify event (*)");
                         }
                     }
                     break;
                 }
             }
         });
}

int
produce_directory_moves (watch        *w,
                         dep_list    **removed,
                         dep_list    **added,
                         bulk_events  *be)
{
    assert (w != NULL);
    assert (be != NULL);

    EXCLUDE_SIMILAR
        (removed, added,
         (removed_iter->inode == added_iter->inode),
         {
             uint32_t cookie = removed_iter->inode & 0x00000000FFFFFFFF;
             int event_len = 0;
             struct inotify_event *ev;
             
             ev = create_inotify_event (w->fd, IN_MOVED_FROM, cookie,
                                        removed_iter->path,
                                        &event_len);
             if (ev != NULL) {   
                 bulk_write (be, ev, event_len);
                 free (ev);
             } else {
                 perror_msg ("Failed to create a new IN_MOVED_FROM inotify event");
             }
             
             ev = create_inotify_event (w->fd, IN_MOVED_TO, cookie,
                                        added_iter->path,
                                        &event_len);
             if (ev != NULL) {   
                 bulk_write (be, ev, event_len);
                 free (ev);
             } else {
                 perror_msg ("Failed to create a new IN_MOVED_TO inotify event");
             }
         });
}


void
produce_directory_changes (watch          *w,
                           dep_list       *list,
                           uint32_t        flag,
                           bulk_events    *be)
{
    assert (w != NULL);
    assert (flag != 0);

    while (list != NULL) {
        struct inotify_event *ie = NULL;
        int ie_len = 0;

        ie = create_inotify_event (w->fd, flag, 0, list->path, &ie_len);
        if (ie != NULL) {
            bulk_write (be, ie, ie_len);
            free (ie);
        } else {
            perror_msg ("Failed to create a new inotify event (directory changes)");
        }

        list = list->next;
    }
}


void
produce_directory_diff (worker *wrk, watch *w, struct kevent *event)
{
    assert (wrk != NULL);
    assert (w != NULL);
    assert (event != NULL);

    assert (w->type == WATCH_USER);
    assert (w->is_directory);

    dep_list *was = NULL, *now = NULL, *ptr = NULL;

    was = dl_shallow_copy (w->deps);
    dep_list *previous = dl_shallow_copy (w->deps);

    ptr = dl_listing (w->filename);
    if (ptr == NULL && errno != ENOENT) {
        /* why skip ENOENT? directory could be already deleted at this point */
        perror_msg ("Failed to create a listing of directory");
        dl_shallow_free (was);
        dl_shallow_free (previous);
        return;
    }
    dl_shallow_free (w->deps);
    w->deps = ptr;

    now = dl_shallow_copy (w->deps);

    dl_diff (&was, &now);

    bulk_events be;
    memset (&be, 0, sizeof (bulk_events));

    int need_upd = 0;
    if (produce_directory_moves (w, &was, &now, &be)) {
        ++need_upd;
    }

    dep_list *listing = dl_shallow_copy (w->deps);
    if (produce_directory_replacements (wrk, w, &was, &listing, &be)) {
        ++need_upd;
    }

    produce_directory_overwrites (wrk, w, &previous, &listing, &be);
    dl_shallow_free (previous);
    dl_shallow_free (listing);
    
    if (need_upd) {
        worker_update_paths (wrk, w);
    }

    produce_directory_changes (w, was, IN_DELETE, &be);
    produce_directory_changes (w, now, IN_CREATE, &be);

    if (be.memory) {
        safe_write (wrk->io[KQUEUE_FD], be.memory, be.size);
        free (be.memory);
    }

    {   dep_list *now_iter = now;
        while (now_iter != NULL) {
            char *path = path_concat (w->filename, now_iter->path);
            if (path != NULL) {
                watch *neww = worker_start_watching (wrk, path, now_iter->path, w->flags, WATCH_DEPENDENCY);
                if (neww == NULL) {
                    perror_msg ("Failed to start watching on a new dependency\n");
                } else {
                    neww->parent = w;
                }
                free (path);
            } else {
                perror_msg ("Failed to allocate a path to start watching a dependency");
            }

            now_iter = now_iter->next;
        }
    }

    worker_remove_many (wrk, w, was, 0);

    dl_shallow_free (now);
    dl_free (was);
}

void
produce_notifications (worker *wrk, struct kevent *event)
{
    assert (wrk != NULL);
    assert (event != NULL);

    watch *w = wrk->sets.watches[event->udata];

    if (w->type == WATCH_USER) {
        uint32_t flags = event->fflags;

        if (w->is_directory
            && (flags & (NOTE_WRITE | NOTE_EXTEND))
            && (w->flags & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO))) {
            produce_directory_diff (wrk, w, event);
            flags &= ~(NOTE_WRITE | NOTE_EXTEND);
        }

        if (flags) {
            struct inotify_event *ie = NULL;
            int ev_len;
            ie = create_inotify_event (w->fd,
                                       kqueue_to_inotify (flags, w->is_directory),
                                       0,
                                       NULL,
                                       &ev_len);
            if (ie != NULL) {
                safe_write (wrk->io[KQUEUE_FD], ie, ev_len);
                free (ie);
            } else {
                perror_msg ("Failed to create a new inotify event");
            }

            if ((flags & NOTE_DELETE) && w->flags & IN_DELETE_SELF) {
                /* TODO: really look on IN_DETELE_SELF? */
                ie = create_inotify_event (w->fd, IN_IGNORED, 0, NULL, &ev_len);
                if (ie != NULL) {
                    safe_write (wrk->io[KQUEUE_FD], ie, ev_len);
                    free (ie);
                } else {
                    perror_msg ("Failed to create a new IN_IGNORED event on remove");
                }
            }
        }
    } else {
        /* for dependency events, ignore some notifications */
        if (event->fflags & (NOTE_ATTRIB | NOTE_LINK | NOTE_WRITE | NOTE_EXTEND)) {
            struct inotify_event *ie = NULL;
            watch *p = w->parent;
            assert (p != NULL);
            int ev_len;
            ie = create_inotify_event
                (p->fd,
                 kqueue_to_inotify (event->fflags, w->is_directory),
                 0,
                 w->filename,
                 &ev_len);

            if (ie != NULL) {
                safe_write (wrk->io[KQUEUE_FD], ie, ev_len);
                free (ie);
            } else {
                perror_msg ("Failed to create a new inotify event for dependency");
            }
        }
    }
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
            perror_msg ("kevent failed");
            continue;
        }

        if (received.ident == wrk->io[KQUEUE_FD]) {
            if (received.flags & EV_EOF) {
                worker_erase (wrk);

                if (pthread_mutex_trylock (&wrk->mutex) == EBUSY) {
                    wrk->closed = 1;
                } else {
                    worker_free (wrk);
                    pthread_mutex_unlock (&wrk->mutex);
                    free (wrk);
                }
                return NULL;
            } else {
                process_command (wrk);
            }
        } else {
            produce_notifications (wrk, &received);
        }
    }
    return NULL;
}

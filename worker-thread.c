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

void
produce_directory_overwrites (worker *wrk, watch *w, dep_list *prev_deps)
{
    assert (w != NULL);

    if (prev_deps == NULL || w->deps == NULL) {
        /* Not our case */
        return;
    }

    dep_list *prev_deps_copy = dl_shallow_copy (prev_deps);
    dep_list *pdc_head = prev_deps_copy;
    dep_list *pdc_prev = NULL;
    dep_list *current_iter = w->deps;

    while (current_iter != NULL) {
        dep_list *pdc_iter = pdc_head;

        while (pdc_iter != NULL && strcmp (pdc_iter->path, current_iter->path) != 0) {
            pdc_iter = pdc_iter->next;
        }

        if (pdc_iter == NULL) {
            break;
        }

        if (pdc_iter->inode != current_iter->inode) {
            /* find an appropriate watch and reopen it */
            int i;
            for (i = 0; i < wrk->sets.length; i++) {
                watch *wi = wrk->sets.watches[i];
                if (wi && strcmp (wi->filename, current_iter->path) == 0) {
                    if (watch_reopen (wi) == -1) {
                        /* I dont know, what to do */
                        /* Not a very beautiful way to remove a single dependency */
                        dep_list *dl = dl_create (wi->filename, wi->inode);
                        worker_remove_many (wrk, w, dl, 0);
                        dl_shallow_free (dl);
                    } else {
                        /* Send a notification. I prefer IN_MOVED_FROM/IN_MOVED_TO.
                         * Linux inotify also sends IN_CREATE in some cases */
                        struct inotify_event *ie = NULL;
                        uint32_t cookie = wi->inode & 0x00000000FFFFFFFF;
                        
                        bulk_events be;
                        memset (&be, 0, sizeof (bulk_events));

                        int ie_len = 0;
                        ie = create_inotify_event (w->fd,
                                                   IN_MOVED_FROM,
                                                   cookie,
                                                   wi->filename,
                                                   &ie_len);
                        if (ie != NULL) {
                            bulk_write (&be, ie, ie_len);
                            free (ie);
                        }

                        ie = create_inotify_event (w->fd,
                                                   IN_MOVED_TO,
                                                   cookie,
                                                   wi->filename,
                                                   &ie_len);
                        if (ie != NULL) {
                            bulk_write (&be, ie, ie_len);
                            free (ie);
                        }

                        if (be.memory) {
                            safe_write (wrk->io[KQUEUE_FD], be.memory, be.size);
                            free (be.memory);
                        }
                    }
                    break;
                }
            }
        }

        /* TODO: Somewhere here I should exclude an item from the list */

        current_iter = current_iter->next;
    }

    dl_shallow_free (prev_deps_copy);
}

int
produce_directory_replacements (worker       *wrk,
                                watch        *w,
                                dep_list    **removed,
                                dep_list    **current,
                                bulk_events  *be)
{
    assert (w != NULL);
    assert (removed != NULL);
    assert (be != NULL);

    dep_list *removed_iter = *removed;
    dep_list *removed_prev = NULL;

    int moves = 0;

    while (removed_iter != NULL) {
        dep_list *current_iter = *current;
        dep_list *current_prev = NULL;

        int matched = 0;
        while (current_iter != NULL) {
            if (removed_iter->inode == current_iter->inode) {
                matched = 1;
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

                ++moves;

                if (current_prev) {
                    current_prev->next = current_iter->next;
                } else {
                    *current = current_iter->next;
                }
                
                if (removed_prev) {
                    removed_prev->next = removed_iter->next;
                } else {
                    *removed = removed_iter->next;
                }

                // TODO: free smt
                break;
            }

            dep_list *oldptr = current_iter;
            current_iter = current_iter->next;
            if (matched == 0) {
                current_prev = oldptr;
            } else {
                free (oldptr);
            }
        }

        dep_list *oldptr = removed_iter;
        removed_iter = removed_iter->next;
        if (matched == 0) {
            removed_prev = oldptr;
        } else {
            free (oldptr); // TODO: dl_free?
        }
    }

    return (moves > 0);
}




int
produce_directory_moves (watch        *w,
                         dep_list    **removed,
                         dep_list    **added,
                         bulk_events  *be)
{
    assert (w != NULL);
    assert (removed != NULL);
    assert (added != NULL);

    dep_list *removed_iter = *removed;
    dep_list *removed_prev = NULL;

    int moves = 0;

    while (removed_iter != NULL) {
        dep_list *added_iter = *added;
        dep_list *added_prev = NULL;

        int matched = 0;
        while (added_iter != NULL) {
            if (removed_iter->inode == added_iter->inode) {
                matched = 1;
                
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

                ++moves;

                if (removed_prev) {
                    removed_prev->next = removed_iter->next;
                } else {
                    *removed = removed_iter->next;
                }

                if (added_prev) {
                    added_prev->next = added_iter->next;
                } else {
                    *added = added_iter->next;
                }
                // TODO: free smt
                break;
            }
        }

        dep_list *oldptr = removed_iter;
        removed_iter = removed_iter->next;
        if (matched == 0) {
            removed_prev = oldptr;
        } else {
            free (oldptr); // TODO: dl_free?
        }
    }

    return (moves > 0);
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

    ptr = dl_listing (w->filename);
    if (ptr == NULL && errno != ENOENT) {
        /* why skip ENOENT? directory could be already deleted at this point */
        perror_msg ("Failed to create a listing of directory");
        dl_shallow_free (was);
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

    dep_list *without_internal_moves = dl_shallow_copy (w->deps);
    if (produce_directory_replacements (wrk, w, &was, &without_internal_moves, &be)) {
        ++need_upd;
    }

    produce_directory_overwrites (wrk, w, without_internal_moves);

    dl_shallow_free (without_internal_moves);
    
    if (need_upd) {
        worker_update_paths (wrk, w);
    }

    printf ("---- was:\n");
    dl_print (was);
    printf ("---- now:\n");
    dl_print (now);
    printf ("---- deps:\n");
    dl_print (w->deps);
    
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

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

#include <stddef.h> /* NULL */
#include <assert.h>
#include <unistd.h> /* write */
#include <stdlib.h> /* calloc, realloc */
#include <string.h> /* memset */
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/event.h>

#include "sys/inotify.h"

#include "utils.h"
#include "conversions.h"
#include "worker.h"
#include "worker-sets.h"
#include "worker-thread.h"

void worker_erase (worker *wrk);

/**
 * This structure represents a sequence of packets.
 * It is used to accumulate inotify events in a single piece
 * of memory and to write it via a single call to write().
 * 
 * Such accumulation is needed when processing directory changes and
 * a lot of events are expected to be sent to the user.
 **/
typedef struct bulk_events {
    void *memory;
    size_t size;
} bulk_events;

/**
 * Write a packet to the buffer. Extend the buffer, if needed.
 *
 * @param[in] be  A pointer to #bulk_events.
 * @param[in] mem A pointer to data to write.
 * @param[in] size The number of bytes to write.
 **/
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
    memcpy ((char *)be->memory + be->size, mem, size);
    be->size += size;
    return 0;
}

/**
 * Process a worker command.
 *
 * @param[in] wrk A pointer to #worker.
 **/
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
    } else {
        perror_msg ("Worker processing a command without a command - "
                    "something went wrong.");
        return;
    }

    /* TODO: is the situation when nobody else waits on a barrier possible */
    pthread_barrier_wait (&wrk->cmd.sync);
}

/**
 * Detect and notify about replacements in the watched directory.
 *
 * Consider you are watching a directory foo with the folloing files
 * insinde:
 *
 *    foo/bar
 *    foo/baz
 *
 * A replacement in a watched directory is what happens when you invoke
 *
 *    mv /foo/bar /foo/bar
 *
 * i.e. when you replace a file in a watched directory with another file
 * from the same directory.
 *
 * @param[in]     wrk     A pointer to #worker.
 * @param[in]     w       A pointer to #watch.
 * @param[in,out] removed A pointer to a pointer to a #dep_list - a list
 *     of items which are considered as deleted in the watched directory.
 * @param[in,out] current A pointer to a pointer to a #dep_list - the
 *     current directory listing.
 * @param[in]     be      A pointer to #bulk_events
 * @return The number of the detected replacemenets.
 **/

/**
 * Detect and notify about overwrites in the watched directory.
 *
 * Consider you are watching a directory foo with a file inside:
 *
 *    foo/bar
 *
 * And you also have a directory tmp with a file 1:
 * 
 *    tmp/1
 *
 * You do not watching directory tmp.
 *
 * A replacement in a watched directory is what happens when you invoke
 *
 *    mv /tmp/1 /foo/bar
 *
 * i.e. when you overwrite a file in a watched directory with another file
 * from the another directory.
 *
 * @param[in]     wrk     A pointer to #worker.
 * @param[in]     w       A pointer to #watch.
 * @param[in,out] removed A pointer to a pointer to a #dep_list -
 *     the listing of the previous contents of a directory.
 * @param[in,out] current A pointer to a pointer to a #dep_list -
 *     the current directory listing (with removed replacements, see above).
 * @param[in]     be      A pointer to #bulk_events
 * @return The number of the detected overwrites.
 **/

/**
 * Detect and notify about moves in the watched directory.
 *
 * A move is what happens when you rename a file in a directory, and
 * a new name is unique, i.e. you didnt overwrite any existing files
 * with this one.
 *
 * @param[in]     w       A pointer to #watch.
 * @param[in,out] removed A pointer to a pointer to #dep_list - the list of
 *     files considered as removed in the watched directory.
 * @param[in,out] added   A pointer to a pointer to #dep_list - the list of
 *     files considered as created in the watched directory.
 * @param[in]     be      A pointer to #bulk_events.
 **/

/**
 * Inform about changes in the watched directory.
 *
 * This function traverses the list of items and for each item
 * it writes inotify notifications with the specified mask.
 *
 * The function is used to notify about IN_CREATE/IN_DELETE events
 *
 * @param[in] w    A pointer to #watch.
 * @param[in] list A list of items to notify about.
 * @param[in] flag A flag to set in the each inotify notification.
 * @param[in] be   A pointer to #bulk_events.
 **/

/**
 * Detect and notify about the changes in the watched directory.
 *
 * This function is top-level and it operates with other specific routines
 * to notify about different sets of events in a different conditions.
 *
 * @param[in] wrk   A pointer to #worker.
 * @param[in] w     A pointer to #watch.
 * @param[in] event A pointer to the received kqueue event.
 **/

typedef struct {
    worker *wrk;
    watch *w;
    bulk_events *be;
} handle_context;

static void
handle_added (void *udata, const char *path, ino_t inode)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);
    assert (ctx->be != NULL);

    struct inotify_event *ie = NULL;
    int ie_len = 0;

    ie = create_inotify_event (ctx->w->fd, IN_CREATE, 0, path, &ie_len);
    if (ie != NULL) {
        bulk_write (ctx->be, ie, ie_len);
        free (ie);
    } else {
        perror_msg ("Failed to create a new inotify event (directory changes)");
    }

    char *npath = path_concat (ctx->w->filename, path);
    if (npath != NULL) {
        watch *neww = worker_start_watching (ctx->wrk, npath, path, ctx->w->flags, WATCH_DEPENDENCY);
        if (neww == NULL) {
            perror_msg ("Failed to start watching on a new dependency\n");
        } else {
            neww->parent = ctx->w;
        }
        free (npath);
    } else {
        perror_msg ("Failed to allocate a path to start watching a dependency");
    }
}

static void
handle_removed (void *udata, const char *path, ino_t inode)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);
    assert (ctx->be != NULL);

    struct inotify_event *ie = NULL;
    int ie_len = 0;

    ie = create_inotify_event (ctx->w->fd, IN_DELETE, 0, path, &ie_len);
    if (ie != NULL) {
        bulk_write (ctx->be, ie, ie_len);
        free (ie);
    } else {
        perror_msg ("Failed to create a new inotify event (directory changes)");
    }
}

static void
handle_replaced (void       *udata,
                 const char *from_path,
                 ino_t       from_inode,
                 const char *to_path,
                 ino_t       to_inode)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);
    assert (ctx->be != NULL);

    uint32_t cookie = from_inode & 0x00000000FFFFFFFF;
    int event_len = 0;
    struct inotify_event *ev;

    ev = create_inotify_event (ctx->w->fd, IN_MOVED_FROM, cookie,
                               from_path,
                               &event_len);
    if (ev != NULL) {
        bulk_write (ctx->be, ev, event_len);
        free (ev);
    }  else {
        perror_msg ("Failed to create a new IN_MOVED_FROM inotify event (*)");
    }

    ev = create_inotify_event (ctx->w->fd, IN_MOVED_TO, cookie,
                               to_path,
                               &event_len);
    if (ev != NULL) {
        bulk_write (ctx->be, ev, event_len);
        free (ev);
    } else {
        perror_msg ("Failed to create a new IN_MOVED_TO inotify event (*)");
    }

    int i;
    for (i = 1; i < ctx->wrk->sets.length; i++) {
        watch *iw = ctx->wrk->sets.watches[i];
        if (iw && iw->parent == ctx->w && strcmp (to_path, iw->filename) == 0) {
            dep_list *dl = dl_create (iw->filename, iw->inode);
            worker_remove_many (ctx->wrk, ctx->w, dl, 0);
            dl_shallow_free (dl);
            break;
        }
    }
}

static void
handle_overwritten (void *udata, const char *path, ino_t inode)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);
    assert (ctx->be != NULL);

   int i;
    for (i = 0; i < ctx->wrk->sets.length; i++) {
        watch *wi = ctx->wrk->sets.watches[i];
        if (wi && (strcmp (wi->filename, path) == 0)
            && wi->parent == ctx->w) {
            if (watch_reopen (wi) == -1) {
                /* I dont know, what to do */
                /* Not a very beautiful way to remove a single dependency */
                dep_list *dl = dl_create (wi->filename, wi->inode);
                worker_remove_many (ctx->wrk, ctx->w, dl, 0);
                dl_shallow_free (dl);
            } else {
                uint32_t cookie = inode & 0x00000000FFFFFFFF;
                int event_len = 0;
                struct inotify_event *ev;

                ev = create_inotify_event (ctx->w->fd, IN_DELETE, cookie,
                                           path,
                                           &event_len);
                if (ev != NULL) {
                    bulk_write (ctx->be, ev, event_len);
                    free (ev);
                }  else {
                    perror_msg ("Failed to create a new IN_DELETE inotify event (*)");
                }

                ev = create_inotify_event (ctx->w->fd, IN_CREATE, cookie,
                                           path,
                                           &event_len);
                if (ev != NULL) {
                    bulk_write (ctx->be, ev, event_len);
                    free (ev);
                } else {
                    perror_msg ("Failed to create a new IN_CREATE inotify event (*)");
                }
            }
            break;
        }
    }
}

static void
handle_moved (void       *udata,
              const char *from_path,
              ino_t       from_inode,
              const char *to_path,
              ino_t       to_inode)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);
    assert (ctx->be != NULL);

    uint32_t cookie = from_inode & 0x00000000FFFFFFFF;
    int event_len = 0;
    struct inotify_event *ev;
    
    ev = create_inotify_event (ctx->w->fd, IN_MOVED_FROM, cookie,
                               from_path,
                               &event_len);
    if (ev != NULL) {   
        bulk_write (ctx->be, ev, event_len);
        free (ev);
    } else {
        perror_msg ("Failed to create a new IN_MOVED_FROM inotify event");
    }
    
    ev = create_inotify_event (ctx->w->fd, IN_MOVED_TO, cookie,
                               to_path,
                               &event_len);
    if (ev != NULL) {   
        bulk_write (ctx->be, ev, event_len);
        free (ev);
    } else {
        perror_msg ("Failed to create a new IN_MOVED_TO inotify event");
    }
}

static void
handle_many_removed (void *udata, const dep_list *list)
{
    assert (udata != NULL);
    handle_context *ctx = (handle_context *) udata;

    if (list) {
        worker_remove_many (ctx->wrk, ctx->w, list, 0);
    }
}

static void
handle_names_updated (void *udata)
{
    assert (udata != NULL);

    handle_context *ctx = (handle_context *) udata;
    assert (ctx->wrk != NULL);
    assert (ctx->w != NULL);

    worker_update_paths (ctx->wrk, ctx->w);
}


static const traverse_cbs cbs = {
    handle_added,
    handle_removed,
    handle_replaced,
    handle_overwritten,
    handle_moved,
    NULL, /* many_added */
    handle_many_removed,
    handle_names_updated,
};

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
    now = dl_listing (w->filename);
    if (now == NULL && errno != ENOENT) {
        /* Why do I skip ENOENT? Because the directory could be deleted at this
         * point */
        perror_msg ("Failed to create a listing for directory");
        dl_shallow_free (was);
        return;
    }

    dl_shallow_free (w->deps);
    w->deps = now;

    bulk_events be;
    memset (&be, 0, sizeof (be));

    handle_context ctx;
    memset (&ctx, 0, sizeof (ctx));
    ctx.wrk = wrk;
    ctx.w = w;
    ctx.be = &be;
    
    dl_calculate (was, now, &cbs, &ctx);
    
    if (be.memory) {
        safe_write (wrk->io[KQUEUE_FD], be.memory, be.size);
        free (be.memory);
    }

    dl_free (was);
}

/**
 * Produce notifications about file system activity observer by a worker.
 *
 * @param[in] wrk   A pointer to #worker.
 * @param[in] event A pointer to the associated received kqueue event.
 **/
void
produce_notifications (worker *wrk, struct kevent *event)
{
    assert (wrk != NULL);
    assert (event != NULL);

    watch *w = wrk->sets.watches[UDATA_TO_INDEX (event->udata)];

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

/**
 * The worker thread command loop.
 *
 * @param[in] arg A pointer to the associated #worker.
 * @return NULL. 
**/
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
                wrk->closed = 1;
                wrk->io[INOTIFY_FD] = -1;
                worker_erase (wrk);

                if (pthread_mutex_trylock (&wrk->mutex) == 0) {
                    worker_free (wrk);
                    pthread_mutex_unlock (&wrk->mutex);
                    free (wrk);
                }
                /* If we could not lock on a worker, it means that an inotify
                 * call (add_watch/rm_watch) has already locked it. In this
                 * case worker will be freed by a caller (caller checks the
                 * `closed' flag. */
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

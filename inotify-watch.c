/*******************************************************************************
  Copyright (c) 2014 Vladimir Kondratiev <wulf@cicgroup.ru>

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

#include <sys/types.h>
#include <sys/stat.h>  /* fstat */

#include <assert.h>    /* assert */
#include <fcntl.h>     /* AT_FDCWD */
#include <stdint.h>    /* uint32_t */
#include <stdlib.h>    /* calloc, free */
#include <string.h>    /* strcmp */
#include <unistd.h>    /* close */

#include "sys/inotify.h"

#include "compat.h"
#include "conversions.h"
#include "inotify-watch.h"
#include "utils.h"
#include "watch.h"
#include "worker-sets.h"

/**
 * Initialize inotify watch.
 *
 * This function creates and initializes additional watches for a directory.
 *
 * @param[in] wrk    A pointer to #worker.
 * @param[in] path   Path to watch.
 * @param[in] flags  A combination of inotify event flags.
 * @return A pointer to a created #i_watch on success NULL otherwise
 **/
i_watch *
iwatch_init (worker     *wrk,
             const char *path,
             uint32_t    flags)
{
    assert (wrk != NULL);
    assert (path != NULL);

    int fd = watch_open (AT_FDCWD, path, flags);
    if (fd == -1) {
        perror_msg ("Failed to open watch %s", path);
        return NULL;
    }

    struct stat st;
    if (fstat (fd, &st) == -1) {
        perror_msg ("fstat failed on %d", fd);
        close (fd);
        return NULL;
    }

    dep_list *deps = NULL;
    if (S_ISDIR (st.st_mode)) {
        deps = dl_listing (fd);
        if (deps == NULL) {
            perror_msg ("Directory listing of %s failed", path);
            close (fd);
            return NULL;
        }
    }

    i_watch *iw = calloc (1, sizeof (i_watch));
    if (iw == NULL) {
        perror_msg ("Failed to allocate inotify watch");
        if (S_ISDIR (st.st_mode)) {
            dl_free (deps);
        }
        close (fd);
        return NULL;
    }
    iw->deps = deps;
    iw->wrk = wrk;
    iw->wd = fd;
    iw->flags = flags;

    if (worker_sets_init (&iw->watches) == -1) {
        if (S_ISDIR (st.st_mode)) {
            dl_free (deps);
        }
        close (fd);
        free (iw);
        return NULL;
    }

    watch *parent = watch_init (WATCH_USER, iw->wrk->kq, path, fd, iw->flags);
    if (parent == NULL) {
        worker_sets_free (&iw->watches);
        if (S_ISDIR (st.st_mode)) {
            dl_free (deps);
        }
        close (fd);
        free (iw);
        return NULL;
    }

    if (worker_sets_insert (&iw->watches, parent)) {
        if (S_ISDIR (st.st_mode)) {
            dl_free (deps);
        }
        watch_free (parent);
        free (iw);
        return NULL;
    }

    if (S_ISDIR (st.st_mode)) {

        dep_node *iter;
        SLIST_FOREACH (iter, &iw->deps->head, next) {
            watch *neww = iwatch_add_subwatch (iw, iter->item);
            if (neww == NULL) {
                perror_msg ("Failed to start watching a dependency %s of %s",
                            iter->item->path,
                            parent->filename);
            }
        }
    }
    return iw;
}

/**
 * Free an inotify watch.
 *
 * @param[in] iw      A pointer to #i_watch to remove.
 **/
void
iwatch_free (i_watch *iw)
{
    assert (iw != NULL);

    worker_sets_free (&iw->watches);
    if (iw->deps != NULL) {
        dl_free (iw->deps);
    }
    free (iw);
}

/**
 * Start watching a file or a directory.
 *
 * @param[in] iw A pointer to #i_watch.
 * @param[in] di A dependency item with relative path to watch.
 * @return A pointer to a created watch.
 **/
watch*
iwatch_add_subwatch (i_watch *iw, const dep_item *di)
{
    assert (iw != NULL);
    assert (iw->deps != NULL);
    assert (di != NULL);

    int fd = watch_open (iw->wd, di->path, IN_DONT_FOLLOW);
    if (fd == -1) {
        perror_msg ("Failed to open file %s", di->path);
        return NULL;
    }

    watch *w = watch_init (WATCH_DEPENDENCY,
                           iw->wrk->kq,
                           di->path,
                           fd,
                           iw->flags);
    if (w == NULL) {
        close (fd);
        return NULL;
    }

    if (worker_sets_insert (&iw->watches, w)) {
        watch_free (w);
        return NULL;
    }

    return w;
}

/**
 * Remove a watch from worker by its path.
 *
 * @param[in] iw A pointer to the #i_watch.
 * @param[in] di A dependency list item to remove watch.
 **/
void
iwatch_del_subwatch (i_watch *iw, const dep_item *di)
{
    assert (iw != NULL);
    assert (di != NULL);

    size_t i;

    for (i = 0; i < iw->watches.length; i++) {
        watch *w = iw->watches.watches[i];

        if ((di->inode == w->inode)
          && (strcmp (di->path, w->filename) == 0)) {
            worker_sets_delete (&iw->watches, i);
            break;
        }
    }
}

/**
 * Update inotify watch flags.
 *
 * When called for a directory watch, update also the flags of all the
 * dependent (child) watches.
 *
 * @param[in] iw    A pointer to #i_watch.
 * @param[in] flags A combination of the inotify watch flags.
 **/
void
iwatch_update_flags (i_watch *iw, uint32_t flags)
{
    assert (iw != NULL);

    iw->flags = flags;

    size_t i;
    for (i = 0; i < iw->watches.length; i++) {
        watch *w = iw->watches.watches[i];
        uint32_t fflags = inotify_to_kqueue (flags,
                                             w->is_really_dir,
                                             w->type != WATCH_USER);
        watch_register_event (w, iw->wrk->kq, fflags);
    }
}

/**
 * Update path of child watch for a specified watch.
 *
 * It is necessary when renames in the watched directory occur.
 *
 * @param[in] iw     A pointer to #i_watch.
 * @param[in] from   A rename from. Must be child of the specified parent.
 * @param[in] to     A rename to. Must be child of the specified parent.
 **/
void
iwatch_rename_subwatch (i_watch *iw, dep_item *from, dep_item *to)
{
    assert (iw != NULL);
    assert (from != NULL);
    assert (to != NULL);

    watch *w = worker_sets_find (&iw->watches, from);

    if (w != NULL) {
        free (w->filename);
        w->filename = strdup (to->path);
    }
}

/**
 * Check if a file under given path is/was a directory. Use worker's
 * cached data (watches) to query file type (this function is called
 * when something happens in a watched directory, so we SHOULD have
 * a watch for its contents
 *
 * @param[in] iw  A inotify watch for which a change has been triggered.
 * @param[in] di  A dependency list item
 *
 * @return 1 if dir (cached), 0 otherwise.
 **/
int
iwatch_subwatch_is_dir (i_watch *iw, const dep_item *di)
{
    watch *w = worker_sets_find (&iw->watches, di);
    if (w != NULL && w->is_really_dir) {
            return 1;
    }
    return 0;
}

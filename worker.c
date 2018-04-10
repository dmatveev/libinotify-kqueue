/*******************************************************************************
  Copyright (c) 2011-2014 Dmitry Matveev <me@dmitrymatveev.co.uk>

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

#include "compat.h"

#include <pthread.h>
#include <signal.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* open() */
#include <unistd.h> /* close() */
#include <assert.h>
#include <stdio.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "sys/inotify.h"

#include "utils.h"
#include "worker-thread.h"
#include "worker.h"

static void
worker_cmd_reset (worker_cmd *cmd);


/**
 * Initialize resources associated with worker command.
 *
 * @param[in] cmd A pointer to #worker_cmd.
 **/
void worker_cmd_init (worker_cmd *cmd)
{
    assert (cmd != NULL);
    memset (cmd, 0, sizeof (worker_cmd));
    pthread_barrier_init (&cmd->sync, NULL, 2);
}

/**
 * Prepare a command with the data of the inotify_add_watch() call.
 *
 * @param[in] cmd      A pointer to #worker_cmd.
 * @param[in] filename A file name of the watched entry.
 * @param[in] mask     A combination of the inotify watch flags.
 **/
void
worker_cmd_add (worker_cmd *cmd, const char *filename, uint32_t mask)
{
    assert (cmd != NULL);
    worker_cmd_reset (cmd);

    cmd->type = WCMD_ADD;
    cmd->add.filename = filename;
    cmd->add.mask = mask;
}


/**
 * Prepare a command with the data of the inotify_rm_watch() call.
 *
 * @param[in] cmd       A pointer to #worker_cmd
 * @param[in] watch_id  The identificator of a watch to remove.
 **/
void
worker_cmd_remove (worker_cmd *cmd, int watch_id)
{
    assert (cmd != NULL);
    worker_cmd_reset (cmd);

    cmd->type = WCMD_REMOVE;
    cmd->rm_id = watch_id;
}

/**
 * Reset the worker command.
 *
 * @param[in] cmd A pointer to #worker_cmd.
 **/
static void
worker_cmd_reset (worker_cmd *cmd)
{
    assert (cmd != NULL);

    cmd->type = 0;
    cmd->retval = 0;
    cmd->add.filename = NULL;
    cmd->add.mask = 0;
    cmd->rm_id = 0;
}

/**
 * Wait on a worker command.
 *
 * This function is used by both user and worker threads for
 * synchronization.
 *
 * @param[in] cmd A pointer to #worker_cmd.
 **/
void
worker_cmd_wait (worker_cmd *cmd)
{
    assert (cmd != NULL);
    pthread_barrier_wait (&cmd->sync);
}

/**
 * Release a worker command.
 *
 * This function releases resources associated with worker command.
 *
 * @param[in] cmd A pointer to #worker_cmd.
 **/
void
worker_cmd_release (worker_cmd *cmd)
{
    assert (cmd != NULL);
    pthread_barrier_destroy (&cmd->sync);
}

/**
 * Create communication pipe
 *
 * @param[in] filedes A pair of descriptors used in referencing the new pipe
 * @param[in] flags   A Pipe flags in inotify(linux) or fcntl.h(BSD) format
 * @return 0 on success, -1 otherwise
 **/
static int
pipe_init (int fildes[2], int flags)
{
    if (socketpair (AF_UNIX, SOCK_NONBLOCK|SOCK_STREAM, 0, fildes) == -1) {
        perror_msg ("Failed to create a socket pair");
        return -1;
    }

    if (set_cloexec_flag (fildes[KQUEUE_FD], 1) == -1) {
        perror_msg ("Failed to set cloexec flag on socket");
        return -1;
    }

    /* Check flags for both linux and BSD CLOEXEC values */
    if (set_cloexec_flag (fildes[INOTIFY_FD],
#ifdef O_CLOEXEC
                          flags & (IN_CLOEXEC|O_CLOEXEC)) == -1) {
#else
                          flags & IN_CLOEXEC) == -1) {
#endif
        perror_msg ("Failed to set cloexec flag on socket");
        return -1;
    }

    /* Check flags for both linux and BSD NONBLOCK values */
    if (set_nonblock_flag (fildes[INOTIFY_FD],
                           flags & (IN_NONBLOCK|O_NONBLOCK)) == -1) {
        perror_msg ("Failed to set socket into nonblocking mode");
        return -1;
    }

    return 0;
}

/**
 * Create a new worker and start its thread.
 *
 * @return A pointer to a new worker.
 **/
worker*
worker_create (int flags)
{
    pthread_attr_t attr;
    struct kevent ev;
    sigset_t set, oset;
    int result;

    worker* wrk = calloc (1, sizeof (worker));

    if (wrk == NULL) {
        perror_msg ("Failed to create a new worker");
        goto failure;
    }

    wrk->iovalloc = 0;
    wrk->iovcnt = 0;
    wrk->iov = NULL;
    wrk->io[INOTIFY_FD] = -1;
    wrk->io[KQUEUE_FD] = -1;

    wrk->kq = kqueue ();
    if (wrk->kq == -1) {
        perror_msg ("Failed to create a new kqueue");
        goto failure;
    }

    if (pipe_init ((int *) wrk->io, flags) == -1) {
        perror_msg ("Failed to create a pipe");
        goto failure;
    }

    SLIST_INIT (&wrk->head);

    EV_SET (&ev,
            wrk->io[KQUEUE_FD],
            EVFILT_READ,
            EV_ADD | EV_ENABLE | EV_CLEAR,
            NOTE_LOWAT,
            1,
            0);

    if (kevent (wrk->kq, &ev, 1, NULL, 0, NULL) == -1) {
        perror_msg ("Failed to register kqueue event on pipe");
        goto failure;
    }

    pthread_mutex_init (&wrk->mutex, NULL);

    worker_cmd_init (&wrk->cmd);

    /* create a run a worker thread */
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

    sigemptyset (&set);
    sigaddset (&set, SIGPIPE);
    pthread_sigmask (SIG_BLOCK, &set, &oset);

    result = pthread_create (&wrk->thread, &attr, worker_thread, wrk);
    
    pthread_attr_destroy (&attr);
    pthread_sigmask (SIG_SETMASK, &oset, NULL);

    if (result != 0) {
        perror_msg ("Failed to start a new worker thread");
        goto failure;
    }

    wrk->closed = 0;
    return wrk;
    
failure:
    if (wrk != NULL) {
        if (wrk->io[INOTIFY_FD] != -1) {
            close (wrk->io[INOTIFY_FD]);
        }
        worker_free (wrk);
    }
    return NULL;
}

/**
 * Free a worker and all the associated memory.
 *
 * @param[in] wrk A pointer to #worker.
 **/
void
worker_free (worker *wrk)
{
    assert (wrk != NULL);

    int i;
    i_watch *iw;

    if (wrk->io[KQUEUE_FD] != -1) {
        close (wrk->io[KQUEUE_FD]);
        wrk->io[KQUEUE_FD] = -1;
    }

    close (wrk->kq);
    wrk->closed = 1;

    worker_cmd_release (&wrk->cmd);
    while (!SLIST_EMPTY (&wrk->head)) {
        iw = SLIST_FIRST (&wrk->head);
        SLIST_REMOVE_HEAD (&wrk->head, next);
        iwatch_free (iw);
    }

    for (i = 0; i < wrk->iovcnt; i++) {
        free (wrk->iov[i].iov_base);
    }
    free (wrk->iov);
    pthread_mutex_destroy (&wrk->mutex);

    free (wrk);
}

/**
 * Add or modify a watch.
 *
 * @param[in] wrk   A pointer to #worker.
 * @param[in] path  A file path to watch.
 * @param[in] flags A combination of inotify watch flags.
 * @return An id of an added watch on success, -1 on failure.
**/
int
worker_add_or_modify (worker     *wrk,
                      const char *path,
                      uint32_t    flags)
{
    assert (path != NULL);
    assert (wrk != NULL);

    /* Open inotify watch descriptor */
    int fd = iwatch_open (path, flags);
    if (fd == -1) {
        return -1;
    }

    struct stat st;
    if (fstat (fd, &st) == -1) {
        perror_msg ("Failed to stat file %s", path);
        close (fd);
        return -1;
    }

    /* look up for an entry with these inode&device numbers */
    i_watch *iw;
    SLIST_FOREACH (iw, &wrk->head, next) {
        if (iw->inode == st.st_ino && iw->dev == st.st_dev) {
            close (fd);
            iwatch_update_flags (iw, flags);
            return iw->wd;
        }
    }

    /* create a new entry if watch is not found */
    iw = iwatch_init (wrk, fd, flags);
    if (iw == NULL) {
        close (fd);
        return -1;
    }

    /* add inotify watch to worker`s watchlist */
    SLIST_INSERT_HEAD (&wrk->head, iw, next);

    return iw->wd;
}

/**
 * Stop and remove a watch.
 *
 * @param[in] wrk A pointer to #worker.
 * @param[in] id  An ID of the watch to remove.
 * @return 0 on success, -1 of failure.
 **/
int
worker_remove (worker *wrk,
               int     id)
{
    assert (wrk != NULL);
    assert (id != -1);

    i_watch *iw;
    SLIST_FOREACH (iw, &wrk->head, next) {

        if (iw->wd == id) {
            enqueue_event (iw, IN_IGNORED, NULL);
            flush_events (wrk);
            SLIST_REMOVE (&wrk->head, iw, i_watch, next);
            iwatch_free (iw);
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}

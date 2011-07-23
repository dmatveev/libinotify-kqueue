#include <sys/event.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* open() */
#include <assert.h>
#include <stdio.h>
#include <dirent.h>

#include "inotify.h"
#include "worker-thread.h"
#include "worker.h"

void
worker_cmd_reset (worker_cmd *cmd)
{
    assert (cmd != NULL);

    free (cmd->add.filename);
    memset (cmd, 0, sizeof (worker_cmd));
}

worker*
worker_create ()
{
    worker* wrk = calloc (1, sizeof (worker));

    if (wrk == NULL) {
        perror ("Failed to create a new worker");
        goto failure;
    }

    wrk->kq = kqueue ();
    if (wrk->kq == -1) {
        perror ("Failed to create a new kqueue");
        goto failure;
    }

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, wrk->io) == -1) {
        perror ("Failed to create a socket pair");
        goto failure;
    }

    worker_sets_init (&wrk->sets, wrk->io[KQUEUE_FD]);
    pthread_mutex_init (&wrk->mutex, NULL);

    /* create a run a worker thread */
    if (pthread_create (&wrk->thread, NULL, worker_thread, wrk) != 0) {
        perror ("Failed to start a new worker thread");
        goto failure;
    }

    return wrk;
    
    failure:
    if (wrk != NULL) {
        worker_free (wrk);
    }
    return NULL;
}


void
worker_free (worker *wrk)
{
    assert (wrk != NULL);

    worker_sets_free (&wrk->sets);
    free (wrk);
}

static int
worker_add_dependencies (worker        *wrk,
                         struct kevent *event,
                         watch         *parent)
{
    assert (wrk != NULL);
    assert (parent != NULL);
    assert (parent->type == WATCH_USER);
    assert (event != NULL);

    // start watching also on its contents
    DIR *dir = opendir((const char*) parent->filename);
    if (dir != NULL) {
        struct dirent *ent;

        while ((ent = readdir(dir)) != NULL) {
            if (!strcmp (ent->d_name, ".") || !strcmp (ent->d_name, "..")) {
                continue;
            }

            int index = wrk->sets.length;
            worker_sets_extend (&wrk->sets, 1);

            int parent_len = strlen (parent->filename);
            int full_len = parent_len + strlen (ent->d_name) + 2;
            char *full_path = malloc (full_len); // TODO: check alloc
            // TODO: strip spaces!!!
            if (full_path[parent_len] == '/') {
                full_path[parent_len] = '\0';
            }
            snprintf (full_path, full_len, "%s/%s", parent->filename, ent->d_name);

            if (watch_init_dependency (&wrk->sets.watches[index],
                                       &wrk->sets.events[index],
                                       full_path,
                                       parent->flags,
                                       index)
                == 0) {
                ++wrk->sets.length;
                wrk->sets.watches[index].parent = parent;

                dep_list *entry = calloc (1, sizeof (dep_list));
                entry->next = parent->deps;
                entry->fd = wrk->sets.events[index].ident;
                parent->deps = entry;
            }
            printf ("Watching also on %s\n", full_path);
            free (full_path);
        }
        closedir(dir);
    } else {
        printf ("Failed to open directory %s\n", parent->filename);
    }
    return 0;
}

int
worker_add_or_modify (worker     *wrk,
                      const char *path,
                      uint32_t    flags)
{
    assert (path != NULL);
    assert (wrk != NULL);
    // TODO: a pointer to sets?
    assert (wrk->sets.events != NULL);
    assert (wrk->sets.watches != NULL);

    int i = 0;
    // look up for an entry with this filename
    for (i = 1; i < wrk->sets.length; i++) {
        const char *evpath = wrk->sets.watches[i].filename;
        assert (evpath != NULL);

        if (wrk->sets.watches[i].type == WATCH_USER &&
            strcmp (path, evpath) == 0) {
            // TODO: update flags
            return i;
        }
    }

    // add a new entry if path is not found
    printf ("Adding a new watch kevent: %s\n", path);
    worker_sets_extend (&wrk->sets, 1);
    i = wrk->sets.length;
    if (watch_init_user (&wrk->sets.watches[i],
                         &wrk->sets.events[i],
                         path,
                         flags,
                         i)
        == -1) {
        perror ("Failed to initialize a user watch\n");
        // TODO: error
        return -1;
    }
    ++wrk->sets.length;

    if (wrk->sets.watches[i].is_directory) {
        printf ("Watched entry is a directory, adding dependencies\n");
        worker_add_dependencies (wrk, &wrk->sets.events[i], &wrk->sets.watches[i]);
    }
    return wrk->sets.events[i].ident;
}


int
worker_remove (worker *wrk,
               int     id)
{
    /* assert (wrk != NULL); */
    /* assert (id != -1); */

    /* int i; */
    /* int last = wrk->sets.length - 1; */
    /* for (i = 0; i < wrk->sets.length; i++) { */
    /*     if (wrk->sets.events[i].ident == id) { */
    /*         free (wrk->sets.filenames[i]); */

    /*         if (i != last) { */
    /*             wrk->sets.events[i] = wrk->sets.events[last]; */
    /*             wrk->sets.filenames[i] = wrk->sets.filenames[last]; */
    /*         } */
    /*         wrk->sets.filenames[last] = NULL; */
    /*         --wrk->sets.length; */

    /*         // TODO: reduce the allocated memory size here */
    /*         return 0; */
    /*     } */
    /* } */
    return -1;
}

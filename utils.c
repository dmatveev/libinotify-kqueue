#include <unistd.h> /* read, write */
#include <errno.h>  /* EINTR */
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */

#include "sys/inotify.h"
#include "utils.h"

char*
path_concat (const char *dir, const char *file)
{
    int dir_len = strlen (dir);
    int file_len = strlen (file);

    char *path = malloc (dir_len + file_len + 2);
    if (path == NULL) {
        perror ("Failed to allocate memory path for concatenation");
        return NULL;
    }

    strcpy (path, dir);

    if (dir[dir_len - 1] != '/') {
        ++dir_len;
        path[dir_len - 1] = '/';
    }

    strcpy (path + dir_len, file);

    return path;
}

struct inotify_event*
create_inotify_event (int         wd,
                      uint32_t    mask,
                      uint32_t    cookie,
                      const char *name,
                      int        *event_len)
{
    struct inotify_event *event = NULL;
    int name_len = name ? strlen (name) + 1 : 0;
    *event_len = sizeof (struct inotify_event) + name_len;
    event = calloc (1, *event_len);

    if (event == NULL) {
        perror ("Failed to allocate a new inotify event");
        return NULL;
    }

    event->wd = wd;
    event->mask = mask;
    event->cookie = cookie;
    event->len = name_len;

    if (name) {
        strcpy (event->name, name);
    }

    return event;
}


#define SAFE_GENERIC_OP(fcn, fd, data, size)    \
    if (fd == -1) {                             \
        return -1;                              \
    }                                           \
    while (size > 0) {                          \
        ssize_t retval = fcn (fd, data, size);  \
        if (retval == -1) {                     \
            if (errno == EINTR) {               \
                continue;                       \
            } else {                            \
                return -1;                      \
            }                                   \
        }                                       \
        size -= retval;                         \
        data += retval;                         \
    }                                           \
    return 0;


int
safe_read  (int fd, void *data, size_t size)
{
    SAFE_GENERIC_OP (read, fd, data, size);
}

int
safe_write (int fd, const void *data, size_t size)
{
    SAFE_GENERIC_OP (write, fd, data, size);
}

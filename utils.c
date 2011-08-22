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

#include <unistd.h> /* read, write */
#include <errno.h>  /* EINTR */
#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */

#include "sys/inotify.h"
#include "utils.h"

#include "config.h"

char*
path_concat (const char *dir, const char *file)
{
    int dir_len = strlen (dir);
    int file_len = strlen (file);

    char *path = malloc (dir_len + file_len + 2);
    if (path == NULL) {
        perror_msg ("Failed to allocate memory path for concatenation");
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
        perror_msg ("Failed to allocate a new inotify event");
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

void
perror_msg (const char *msg)
{
#ifdef ENABLE_PERRORS
    perror (msg);
#else
    (void) msg;
#endif
}

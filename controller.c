#include "inotify.h"

int
inotify_init (void) __THROW
{
    return 0;
}

int
inotify_init1 (int flags) __THROW
{
    return 0;
}

int
inotify_add_watch (int         fd,
                   const char *name,
                   uint32_t    mask) __THROW
{
    return 0;
}

int
inotify_rm_watch (int fd,
                  int wd) __THROW
{
    return 0;
}

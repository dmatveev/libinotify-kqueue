#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h> /* uint32_t */

char* path_concat (const char *dir, const char *file);

struct inotify_event* create_inotify_event (int         wd,
                                            uint32_t    mask,
                                            uint32_t    cookie,
                                            const char *name,
                                            int        *event_len);

int safe_read  (int fd, void *data, size_t size);
int safe_write (int fd, const void *data, size_t size);

void perror_msg (const char *msg);

#endif /* __UTILS_H__ */

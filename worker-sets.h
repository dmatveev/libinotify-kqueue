#ifndef __WORKER_SETS_H__
#define __WORKER_SETS_H__

#include <sys/types.h>


typedef struct worker_sets {
    struct kevent *events;  /* kevent entries */
    const char **filenames; /* file name for each entry */
    size_t length;          /* size of active entries */
} worker_sets;


void worker_sets_init (worker_sets *ws, int fd);
void worker_sets_free (worker_sets *ws);


#endif /* __WORKER_SETS_H__ */

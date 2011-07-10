#ifndef __WORKER_H__
#define __WORKER_H__

#include <stdint.h>

typedef struct worker worker;

worker* worker_create        ();
void    worker_free          (worker *wrk);

int     worker_add_or_modify (worker *wrk, const char *path, uint32_t flags);
void    worker_remove        (worker *wrk, int id);

#endif /* __WORKER_H__ */

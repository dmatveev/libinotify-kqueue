#ifndef __WORKER_THREAD_H__
#define __WORKER_THREAD_H__

#include <sys/queue.h>
#include <stdint.h>
#include <pthread.h>

typedef struct worker_cmd {
    SIMPLEQ_ENTRY(worker_cmd) entries;

    enum {
        WCMD_ADD,
        WCMD_REMOVE
    } type;

    struct {
        int *retval;
        int *error;
    } feedback;

    pthread_barrier_t sync;

    union {
        struct {
            const char *filename;
            uint32_t mask;
        } add;

        int rm_id;
    };
} worker_cmd;

void*       worker_thread (void *arg);

#endif /* __WORKER_THREAD_H__ */

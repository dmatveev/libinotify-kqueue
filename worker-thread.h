#ifndef __WORKER_THREAD_H__
#define __WORKER_THREAD_H__

#include <stdint.h>

typedef struct {
    enum {
        WCMD_ADD,
        WCMD_REMOVE
    } type;

    struct {
        int *retval;
        int *error;
    } feedback;

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

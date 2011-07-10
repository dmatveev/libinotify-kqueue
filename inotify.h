#ifndef __BSD_INOTIFY_H__
#define __BSD_INOTIFY_H__

#include <stdint.h>

#ifndef __THROW
  #ifdef __cplusplus
    #define __THROW throw()
  #else
    #define __THROW
  #endif
#endif


/* Flags for the parameter of inotify_init1. */
enum {
    IN_CLOEXEC = 02000000,
    IN_NONBLOCK = 04000
};  


/* Structure describing an inotify event. */
struct inotify_event
{
    int wd;          /* Watch descriptor.  */
    uint32_t mask;   /* Watch mask.  */
    uint32_t cookie; /* Cookie to synchronize two events.  */
    uint32_t len;    /* Length (including NULs) of name.  */
    char name[];     /* Name.  */
};


/* Supported events suitable for MASK parameter of INOTIFY_ADD_WATCH.  */
#define IN_ACCESS        0x00000001 /* File was accessed.  */
#define IN_MODIFY        0x00000002 /* File was modified.  */
#define IN_ATTRIB        0x00000004 /* Metadata changed.  */
#define IN_CLOSE_WRITE   0x00000008 /* Writtable file was closed.  */
#define IN_CLOSE_NOWRITE 0x00000010 /* Unwrittable file closed.  */
#define IN_CLOSE         (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE) /* Close.  */
#define IN_OPEN          0x00000020 /* File was opened.  */
#define IN_MOVED_FROM    0x00000040 /* File was moved from X.  */
#define IN_MOVED_TO      0x00000080 /* File was moved to Y.  */
#define IN_MOVE          (IN_MOVED_FROM | IN_MOVED_TO) /* Moves.  */
#define IN_CREATE        0x00000100 /* Subfile was created.  */
#define IN_DELETE        0x00000200 /* Subfile was deleted.  */
#define IN_DELETE_SELF   0x00000400 /* Self was deleted.  */
#define IN_MOVE_SELF     0x00000800 /* Self was moved.  */


/* Create and initialize inotify-kqueue instance. */
int inotify_init (void) __THROW;

/* Create and initialize inotify-kqueue instance. */
int inotify_init1 (int flags) __THROW;

/* Add watch of object NAME to inotify-kqueue instance FD. Notify about
   events specified by MASK. */
int inotify_add_watch (int fd, const char *name, uint32_t mask) __THROW;

/* Remove the watch specified by WD from the inotify instance FD. */
int inotify_rm_watch (int fd, int wd) __THROW;


#endif /* __BSD_INOTIFY_H__ */

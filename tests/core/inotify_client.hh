#ifndef __INOTIFY_CLIENT_HH__
#define __INOTIFY_CLIENT_HH__

#include "platform.hh"
#include "event.hh"

class inotify_client {
    int fd;

public:
    inotify_client ();
    ~inotify_client ();
    int watch (const std::string &filename, uint32_t flags);
    void cancel (int wid);
    events receive_during (int timeout) const;

    static long bytes_available (int fd);
};

#endif // __INOTIFY_CLIENT_HH__

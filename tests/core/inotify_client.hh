#ifndef __INOTIFY_CLIENT_HH__
#define __INOTIFY_CLIENT_HH__

#include "platform.hh"
#include "event.hh"

class inotify_client {
    int fd;

public:
    inotify_client ();
    uint32_t watch (const std::string &filename, uint32_t flags);
    void cancel (uint32_t watch_id);
    bool get_next_event (event &ev, int timeout) const;
};

#endif // __INOTIFY_CLIENT_HH__

#include <cassert>
#include <cstring>
#include <poll.h>
#include "inotify_client.hh"
#include "log.hh"

inotify_client::inotify_client ()
: fd (inotify_init())
{
    assert (fd != -1);
}

inotify_client::~inotify_client ()
{
    close (fd);
}

uint32_t inotify_client::watch (const std::string &filename, uint32_t flags)
{
    assert (fd != -1);
    LOG ("INO: Adding " << VAR (filename) << VAR (flags));
    return inotify_add_watch (fd, filename.c_str(), flags);
}

void inotify_client::cancel (uint32_t watch_id)
{
    assert (fd != -1);
    if (inotify_rm_watch (fd, watch_id) != 0) {
        LOG ("INO: rm watch failed " << VAR (fd) << VAR (watch_id));
    }
}

#define IE_BUFSIZE ((sizeof (struct inotify_event) + FILENAME_MAX))

bool inotify_client::get_next_event (event& ev, int timeout) const
{
    struct pollfd pfd;
    memset (&pfd, 0, sizeof (struct pollfd));
    pfd.fd = fd;
    pfd.events = POLLIN;

    poll (&pfd, 1, timeout * 1000);

    if (pfd.revents & POLLIN) {
        char buffer[IE_BUFSIZE];
        read (fd, buffer, IE_BUFSIZE);

        struct inotify_event *ie = (struct inotify_event *) buffer;
        ev.filename = ie->name;
        ev.flags = ie->mask;
        ev.watch = ie->wd;
        bool ignored = ie->mask & 0x00008000;
        LOG ("INO: Got next event! " << VAR (ie->name) << VAR (ie->wd) << VAR(ignored));
        return true;
    }

    return false;
}


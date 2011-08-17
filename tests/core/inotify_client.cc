#include <cassert>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
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

int inotify_client::watch (const std::string &filename, uint32_t flags)
{
    assert (fd != -1);
    LOG ("INO: Adding " << VAR (filename) << VAR (flags));

    int retval = inotify_add_watch (fd, filename.c_str(), flags);
    LOG ("INO: " << VAR (retval));
    return retval;
}

void inotify_client::cancel (int watch_id)
{
    assert (fd != -1);
    if (inotify_rm_watch (fd, watch_id) != 0) {
        LOG ("INO: rm watch failed " << VAR (fd) << VAR (watch_id));
    }
}

#define IE_BUFSIZE (((sizeof (struct inotify_event) + FILENAME_MAX)) * 20)

events inotify_client::receive_during (int timeout) const
{
    events received;
    struct pollfd pfd;

    time_t start = time (NULL);
    time_t elapsed = 0;

    LOG ("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

    while ((elapsed = time (NULL) - start) < timeout) {
        memset (&pfd, 0, sizeof (struct pollfd));
        pfd.fd = fd;
        pfd.events = POLLIN;

        LOG ("INO: Polling with " << VAR (timeout));
        int pollretval = poll (&pfd, 1, timeout * 1000);
        LOG ("INO: Poll returned " << VAR (pollretval) << ", " << VAR(pfd.revents));
        if (pollretval == -1) {
            return events();
        }

        if (pfd.revents & POLLIN) {
            char buffer[IE_BUFSIZE];
            char *ptr = buffer;
            int avail = read (fd, buffer, IE_BUFSIZE);

            /* The construction is probably harmful */
            while (avail >= sizeof (struct inotify_event *)) {
                struct inotify_event *ie = (struct inotify_event *) ptr;
                event ev;

                if (ie->len) {
                        ev.filename = ie->name;
                }
                ev.flags = ie->mask;
                ev.watch = ie->wd;
                ev.cookie = ie->cookie;

                LOG ("INO: Got next event! " << VAR (ev.filename) << VAR (ev.watch) << VAR (ev.flags));
                received.insert (ev);

                int offset = sizeof (struct inotify_event) + ie->len;
                avail -= offset;
                ptr += offset;
            }
        }
    }

    LOG ("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");

    return received;
}

long inotify_client::bytes_available (int fd)
{
    long int avail = 0;
    size_t bytes;

    if (ioctl (fd, FIONREAD, (char *) &bytes) >= 0)
        return avail = (long int) *((int *) &bytes);
}

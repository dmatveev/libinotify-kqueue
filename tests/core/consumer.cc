#include <cassert>
#include <algorithm>
#include "log.hh"
#include "consumer.hh"

consumer::consumer ()
{
    pthread_t pid;
    pthread_create (&pid, NULL, consumer::run_, this);
}

void* consumer::run_ (void *ptr)
{
    assert (ptr != NULL);
    ((consumer *) ptr)->run();
    return NULL;
}

void consumer::register_activity (request::activity activity)
{
    time_t start = time (NULL);
    time_t elapsed = 0;

    while (!activity.expected.empty()
           && (elapsed = time (NULL) - start) < activity.timeout) {
        event ev;
        if (ino.get_next_event (ev, activity.timeout)) {
            event_by_name_and_wid matcher (ev.filename, ev.watch);
            events::iterator it = std::find_if (activity.expected.begin(), activity.expected.end(), matcher); 
            if (it != activity.expected.end()) {
                activity.expected.erase (it);
            }
        }
    }

    LOG ("CONS: Okay, informing producer about results...");
    input.reset ();
    output.setup (activity.expected);
}

void consumer::add_modify_watch (request::add_modify add_modify)
{
    uint32_t id = ino.watch (add_modify.path, add_modify.mask);
    input.reset ();
    output.setup (id);
}

void consumer::remove_watch (request::remove remove)
{
    ino.cancel (remove.watch_id);
    input.reset ();
    output.wait ();
}

void consumer::run ()
{
    while (input.wait ()) {
        switch (input.current_variant ()) {
        case request::REGISTER_ACTIVITY:
            register_activity (input.activity_data ());
            break;

        case request::ADD_MODIFY_WATCH:
            add_modify_watch (input.add_modify_data ());
            break;

        case request::REMOVE_WATCH:
            remove_watch (input.remove_data ());
            break;
        }

        LOG ("CONS: Sleeping on input");
    }
}

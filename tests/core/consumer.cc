#include <cassert>
#include <algorithm>
#include "log.hh"
#include "consumer.hh"

consumer::consumer ()
{
    pthread_create (&self, NULL, consumer::run_, this);
}

consumer::~consumer ()
{
    // It is a trick. The consumer object lives in a separate thread that is created
    // in its constructor. However, the object itself is created in another (parent)
    // thread, so the destructor should work in the same thread (I cound on the
    // static allocation).
    LOG ("CONS: Joining on self");
    pthread_join (self, NULL);
}

void* consumer::run_ (void *ptr)
{
    assert (ptr != NULL);
    ((consumer *) ptr)->run();
    return NULL;
}

void consumer::register_activity (request::activity activity)
{
    events received = ino.receive_during (activity.timeout);
    LOG ("CONS: Okay, informing producer about results...");
    input.reset ();
    output.setup (received);
}

void consumer::add_modify_watch (request::add_modify add_modify)
{
    uint32_t id = ino.watch (add_modify.path, add_modify.mask);
    LOG ("CONS: Added watch");
    input.reset ();
    output.setup (id);
}

void consumer::remove_watch (request::remove remove)
{
    ino.cancel (remove.watch_id);
    LOG ("CONS: Cancelled watch");
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

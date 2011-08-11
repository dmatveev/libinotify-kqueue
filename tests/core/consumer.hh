#ifndef __CONSUMER_HH__
#define __CONSUMER_HH__

#include "platform.hh"
#include "request.hh"
#include "response.hh"
#include "inotify_client.hh"

class consumer {
    std::string path;
    inotify_client ino;

    // yes, these methods take arguments by a value. intentionally
    void register_activity (request::activity activity);
    void add_modify_watch (request::add_modify add_modify);
    void remove_watch (request::remove remove);

public:
    request input;
    response output;

    consumer ();
    static void* run_ (void *ptr);
    void run ();
};


#endif // __CONSUMER_HH__

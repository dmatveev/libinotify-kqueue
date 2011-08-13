#ifndef __EVENT_HH__
#define __EVENT_HH__

#include <set>
#include "platform.hh"

struct event {
    std::string filename;
    int watch;
    uint32_t flags;

    event (const std::string &filename_ = "", int watch_ = 0, uint32_t flags_ = 0);
    bool operator< (const event &ev) const;
};

typedef std::set<event> events;

class event_by_name_and_wid {
    std::string look_for;
    int watch_id;

public:
    event_by_name_and_wid (const std::string &name, int wid);
    bool operator() (const event &ev) const;
};

bool contains (const events &events, const std::string filename, uint32_t flags);

#endif // __EVENT_HH__

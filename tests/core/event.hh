#ifndef __EVENT_HH__
#define __EVENT_HH__

#include <set>
#include "platform.hh"

struct event {
    std::string filename;
    uint32_t watch;
    uint32_t flags;

    event (const std::string &filename_ = "", uint32_t watch_ = 0, uint32_t flags_ = 0);
    bool operator< (const event &ev) const;
};

typedef std::set<event> events;

class event_by_name {
    std::string look_for;

public:
    event_by_name (const std::string &name);
    bool operator() (const event &ev) const;
};

#endif // __EVENT_HH__

#include "event.hh"

event::event (const std::string &filename_, uint32_t watch_, uint32_t flags_)
: filename (filename_)
, watch (watch_)
, flags (flags_)
{
}

bool event::operator< (const event &ev) const
{
    // yes, compare by a filename and watch id only
    return watch != ev.watch ? watch < ev.watch : filename < ev.filename;
}

event_by_name::event_by_name (const std::string &name)
: look_for (name)
{
}

bool event_by_name::operator() (const event &ev) const
{
    return ev.filename == look_for;
}


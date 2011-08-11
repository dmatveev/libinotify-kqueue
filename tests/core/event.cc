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

event_by_name_and_wid::event_by_name_and_wid (const std::string &name, uint32_t wid)
: look_for (name)
, watch_id (wid)
{
}

bool event_by_name_and_wid::operator() (const event &ev) const
{
    return ev.filename == look_for && ev.watch == watch_id;
}


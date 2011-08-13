#include <algorithm>
#include "event.hh"
#include "log.hh"

event::event (const std::string &filename_, int watch_, uint32_t flags_)
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

event_by_name_and_wid::event_by_name_and_wid (const std::string &name, int wid)
: look_for (name)
, watch_id (wid)
{
    LOG ("Created matcher " << look_for << ':' << watch_id);
}

bool event_by_name_and_wid::operator() (const event &ev) const
{
    LOG ("matching " << look_for << ':' << watch_id <<
         " against " << ev.filename << ':' << ev.watch);

    return ev.filename == look_for && ev.watch == watch_id;
}


bool contains (const events &events, const std::string filename, uint32_t flags)
{
    event_by_name_and_wid matcher (filename, flags);
    return std::find_if (events.begin(), events.end(), matcher) != events.end();
}

#include "log.hh"
#include "action.hh"

action::action (const std::string &name_)
: name (name_)
, interrupted (false)
{
    init ();
}

action::~action ()
{
}

void action::init ()
{
    // initialize the barrier for two threads: a producer and a consumer
    LOG (name << ": Initializing");
    pthread_barrier_init (&barrier, NULL, 2);
    interrupted = false;
}

bool action::wait ()
{
    LOG (name << ": Sleeping on a barrier");
    pthread_barrier_wait (&barrier);
    return !interrupted;
}

void action::interrupt ()
{
    LOG (name << ": Marking action interrupted");
    interrupted = true;
    wait ();
}

void action::reset ()
{
    LOG (name << ": Resetting");
    pthread_barrier_destroy (&barrier);
    init ();
}

std::string action::named () const
{
    return name;
}


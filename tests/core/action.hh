#ifndef __ACTION_HH__
#define __ACTION_HH__

#include "platform.hh"

class action {
    pthread_barrier_t barrier;
    void init();

    const std::string name;
    volatile bool interrupted;

public:
    action (const std::string &name_);
    virtual ~action () = 0;

    bool wait ();
    void interrupt ();
    void reset ();

    std::string named () const;
};

#endif // __ACTION_HH__

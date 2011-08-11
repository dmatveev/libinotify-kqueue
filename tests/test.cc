#include <cstddef> // NULL
#include <cstring> // memset
#include <cstdlib> // system
#include <cassert>
#include <ctime>
#include <string>
#include <set>
#include <list>
#include <algorithm> // find_if
#include <pthread.h>
#include <poll.h>
#include <iostream>

#ifdef __linux__
#  include <sys/inotify.h>
#  include <cstdint> // uint32_t, requires -std=c++0x
#elif defined (__NetBSD__)
#  include "inotify.h"
#  include <stdint.h>
#else
#  error Currently unsupported
#endif

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int current_thread ()
{
#ifdef __linux__
    return static_cast<unsigned int>(pthread_self ());
#elif defined (__NetBSD__)
    return reinterpret_cast<unsigned int>(pthread_self ());
#else
    error Currently unsupported
#endif
}

static volatile bool LOGGING_ENABLED = false;

#define LOG(X)                                 \
    do {                                       \
        if (LOGGING_ENABLED) {                 \
            pthread_mutex_lock (&log_mutex);   \
            std::cout                          \
                << current_thread() << "    "  \
                << X                           \
                << std::endl;                  \
            pthread_mutex_unlock (&log_mutex); \
        }                                      \
    } while (0)

struct event {
    std::string filename;
    uint32_t watch;
    uint32_t flags;

    event (const std::string &filename_ = "", uint32_t watch_ = 0, uint32_t flags_ = 0);
    bool operator< (const event &ev) const;
};

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

typedef std::set<event> events;

class event_by_name {
    std::string look_for;

public:
    event_by_name (const std::string &name);
    bool operator() (const event &ev) const;
};

event_by_name::event_by_name (const std::string &name)
: look_for (name)
{
}

bool event_by_name::operator() (const event &ev) const
{
    return ev.filename == look_for;
}

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


class request: public action {
public:
    enum variant {
        REGISTER_ACTIVITY,
        ADD_MODIFY_WATCH,
        REMOVE_WATCH,
    };

    struct activity {
        events expected;
        int timeout;
    };

    struct add_modify {
        std::string path;
        uint32_t mask;
    };

    struct remove {
        int watch_id;
    };

private:
    // Not a union, because its members are non-POD
    struct {
        activity _act;
        add_modify _am;
        remove _rm;
    } variants;

    variant current;
    
public:
    request ();
    void setup (const events &expected, unsigned int timeout = 0);
    void setup (const std::string &path, uint32_t mask);
    void setup (uint32_t rm_id);
    
    variant current_variant () const;
    activity activity_data () const;
    add_modify add_modify_data () const;
    remove remove_data () const;
};

request::request ()
: action ("REQUEST")
{
}

void request::setup (const events &expected, unsigned int timeout)
{
    LOG (named() << ": Setting up to register an activity");
    current = REGISTER_ACTIVITY;
    variants._act.expected = expected;
    variants._act.timeout = timeout;
    wait ();
}

void request::setup (const std::string &path, uint32_t mask)
{
    LOG (named() << ": Setting up to watch a path");
    current = ADD_MODIFY_WATCH;
    variants._am.path = path;
    variants._am.mask = mask;
    wait ();
}

void request::setup (uint32_t rm_id)
{
    LOG (named() << ": Setting up to stop a path");
    current = REMOVE_WATCH;
    variants._rm.watch_id = rm_id;
    wait ();
}

request::variant request::current_variant () const
{
    return current;
}

request::activity request::activity_data () const
{
    assert (current == REGISTER_ACTIVITY);
    return variants._act;
}

request::add_modify request::add_modify_data () const
{
    assert (current == ADD_MODIFY_WATCH);
    return variants._am;
}

request::remove request::remove_data () const
{
    assert (current == REMOVE_WATCH);
    return variants._rm;
}



class response: public action {
public:
    enum variant {
        UNREGISTERED_EVENTS,
        WATCH_ID,
    };

private:
    // Again, not a union
    struct {
        events _left_unreg;
        int _watch_id;
    } variants;

    variant current;

public:
    response ();
    void setup (const events &unregistered);
    void setup (uint32_t watch_id);

    variant current_variant () const;
    events left_unregistered () const;
    int added_watch_id () const;
};

response::response ()
: action ("RESPONSE")
{
}

void response::setup (const events &unregistered)
{
    LOG (named() << ": Passing back unregistered events");
    current = UNREGISTERED_EVENTS;
    variants._left_unreg = unregistered;
    wait ();
}

void response::setup (uint32_t watch_id)
{
    LOG (named() << ": Passing back new watch id");
    current = WATCH_ID;
    variants._watch_id = watch_id;
    wait ();
}

response::variant response::current_variant () const
{
    return current;
}

events response::left_unregistered () const
{
    assert (current == UNREGISTERED_EVENTS);
    return variants._left_unreg;
}

int response::added_watch_id () const
{
    assert (current == WATCH_ID);
    return variants._watch_id;
}


class consumer;

class producer {
    consumer *cons;
    std::string path;

public:
    void assign (consumer *cons_);
};

void producer::assign (consumer *cons_)
{
    assert (cons_ != NULL);
    cons = cons_;
}


class inotify_client {
    int fd;

public:
    inotify_client ();
    uint32_t watch (const std::string &filename, uint32_t flags);
    void cancel (uint32_t watch_id);
    bool get_next_event (event &ev, int timeout) const;
};

inotify_client::inotify_client()
: fd (inotify_init())
{
    assert (fd != -1);
}

uint32_t inotify_client::watch (const std::string &filename, uint32_t flags)
{
    assert (fd != -1);
    return inotify_add_watch (fd, filename.c_str(), flags);
}

void inotify_client::cancel (uint32_t watch_id)
{
    assert (fd != -1);
    inotify_rm_watch (fd, watch_id);
}

#define IE_BUFSIZE ((sizeof (struct inotify_event) + FILENAME_MAX))

bool inotify_client::get_next_event (event& ev, int timeout) const
{
    struct pollfd pfd;
    memset (&pfd, 0, sizeof (struct pollfd));
    pfd.fd = fd;
    pfd.events = POLLIN;

    poll (&pfd, 1, timeout * 1000);

    if (pfd.revents & POLLIN) {
        char buffer[IE_BUFSIZE];
        read (fd, buffer, IE_BUFSIZE);

        struct inotify_event *ie = (struct inotify_event *) buffer;
        ev.filename = ie->name;
        ev.flags = ie->mask;
        ev.watch = ie->wd;
        return true;
    }

    return false;
}


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

void* consumer::run_ (void *ptr)
{
    assert (ptr != NULL);
    ((consumer *) ptr)->run();
    return NULL;
}

consumer::consumer ()
{
    pthread_t pid;
    pthread_create (&pid, NULL, consumer::run_, this);
}

void consumer::register_activity (request::activity activity)
{
    // Now we have a set of events which should occur on a watched entity.
    // Receive events until the timeout will be reached.
    time_t start = time (NULL);
    time_t elapsed = 0;
    
    while (!activity.expected.empty()
           && (elapsed = time (NULL) - start) < activity.timeout) {
        event ev;
        if (ino.get_next_event (ev, activity.timeout)) {
            event_by_name matcher (ev.filename);
            events::iterator it = std::find_if (activity.expected.begin(), activity.expected.end(), matcher); 
            if (it != activity.expected.end()) {
                activity.expected.erase (it);
            }
        }
    }

    // Inform producer about results
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

class test;

class journal {
public:
    struct entry {
        std::string name;
        enum status_e {
            PASSED,
            FAILED
        } status;

        explicit entry (const std::string &name_ = "", status_e status_ = PASSED);
    };

    class channel {
        typedef std::list<entry> entry_list;
        entry_list results;
        std::string name;

    public:
        channel (const std::string &name_);

        void pass (const std::string &test_name);
        void fail (const std::string &test_name);

        void summarize (int &passed, int &failed) const;
    };

    journal ();
    channel& allocate_channel (const std::string &name);
    void summarize () const;

private:
    // It would be better to use shared pointers here, but I dont want to
    // introduce a dependency like Boost. Raw references are harmful anyway
    typedef std::list<channel> channel_list;
    mutable pthread_mutex_t channels_mutex;
    channel_list channels;
};

journal::entry::entry (const std::string &name_, status_e status_)
: name (name_)
, status (status_)
{
}

journal::channel::channel (const std::string &name_)
: name (name_)
{
}

void journal::channel::pass (const std::string &test_name)
{
    results.push_back (entry (test_name, entry::PASSED));
}

void journal::channel::fail (const std::string &test_name)
{
    results.push_back (entry (test_name, entry::FAILED));
}

void journal::channel::summarize (int &passed, int &failed) const
{
    passed = 0;
    failed = 0;

    bool header_printed = false;

    for (entry_list::const_iterator iter = results.begin();
         iter != results.end();
         ++iter) {

        if (iter->status == entry::PASSED) {
            ++passed;
        } else {
            ++failed;

            if (!header_printed) {
                header_printed = true;
                std::cout << "In test \"" << name << "\":" << std::endl;
            }

            std::cout << "    failed: " << iter->name << std::endl;
        }
    }

    if (header_printed) {
        std::cout << std::endl;
    }
}

journal::journal ()
{
    pthread_mutex_init (&channels_mutex, NULL);
}

journal::channel& journal::allocate_channel (const std::string &name)
{
    pthread_mutex_lock (&channels_mutex);
    channels.push_back (journal::channel (name));
    journal::channel &ref = *channels.rbegin();
    pthread_mutex_unlock (&channels_mutex);
    return ref;
}

void journal::summarize () const
{
    pthread_mutex_lock (&channels_mutex);

    int total_passed = 0, total_failed = 0;

    for (channel_list::const_iterator iter = channels.begin();
         iter != channels.end();
         ++iter) {
        int passed = 0, failed = 0;
        iter->summarize (passed, failed);
        total_passed += passed;
        total_failed += failed;
    }

    int total = total_passed + total_failed;
    std::cout << std::endl
              << "--------------------" << std::endl
              << "   Run: " << total << std::endl
              << "Passed: " << total_passed << std::endl
              << "Failed: " << total_failed << std::endl;

    pthread_mutex_unlock (&channels_mutex);
}

class test {
    journal::channel &jc;
    pthread_t thread;

protected:
    static void* run_ (void *ptr);

    virtual void setup () = 0;
    virtual void run () = 0;
    virtual void cleanup () = 0;

public:
    test (const std::string &name_, journal &j);
    virtual ~test ();

    void wait_for_end ();

    bool should (const std::string &test_name, bool exp);
    void pass (const std::string &test_name);
    void fail (const std::string &test_name);
};

test::test (const std::string &name_, journal &j)
: jc (j.allocate_channel (name_))
{
    pthread_create (&thread, NULL, test::run_, this);
}

test::~test ()
{
}

void* test::run_ (void *ptr)
{
    assert (ptr != NULL);
    test *t = static_cast<test *>(ptr);
    t->setup ();
    t->run ();
    t->cleanup ();
    return NULL;
}

void test::wait_for_end ()
{
    pthread_join (thread, NULL);
}

bool test::should (const std::string &test_name, bool exp)
{
    if (exp) {
        pass (test_name);
    } else {
        fail (test_name);
    }
    return exp;
}

void test::pass (const std::string &test_name)
{
    std::cout << ".";
    jc.pass (test_name);
}

void test::fail (const std::string &test_name)
{
    std::cout << "x";
    jc.fail (test_name);
}



class demo_test: public test {
protected:
    virtual void setup ();
    virtual void run ();
    virtual void cleanup ();

public:
    demo_test (journal &j);
};

demo_test::demo_test (journal &j)
: test ("Demo test", j)
{
}

void demo_test::setup ()
{
    // Remove a directory, if any, and create a new one
    system ("rm -rf /tmp/foo");
    system ("mkdir /tmp/foo");

    // Create required files
    system ("touch /tmp/foo/1");
    system ("touch /tmp/foo/2");
}

void demo_test::run ()
{
    consumer cons;
    int wid = 0;

    /* Add a watch and test a result */
    cons.input.setup ("/tmp/foo", IN_ATTRIB);
    cons.output.wait ();
    wid = cons.output.added_watch_id ();

    should ("watch is added successfully", wid != -1);

    /* Produce fs activity and see what will happen */
    events expected;
    expected.insert (event("1", wid, IN_ATTRIB));
    expected.insert (event("2", wid, IN_ATTRIB));
    cons.output.reset ();
    cons.input.setup (expected, 1);

    system ("touch /tmp/foo/1");
    system ("touch /tmp/foo/2");

    cons.output.wait ();
    expected = cons.output.left_unregistered ();

    should ("all events registered", expected.empty());

    cons.input.interrupt ();
}

void demo_test::cleanup ()
{
    system ("rm -rf /tmp/foo");
}

int main (int argc, char *argv[]) {
    journal j;
    demo_test dt (j);
    dt.wait_for_end ();
    j.summarize ();
    return 0;
}

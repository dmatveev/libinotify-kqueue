#include <cstdlib>      // system
#include "core/core.hh"

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

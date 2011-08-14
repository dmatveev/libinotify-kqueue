#include "notifications_test.hh"

/* Always present on Linux (not sure about a concrete 2.6.x release)
 * May be "to be implemented" on BSDs */
#ifndef IN_IGNORED
#  define IN_IGNORED	 0x00008000
#endif

notifications_test::notifications_test (journal &j)
: test ("File notifications", j)
{
}

void notifications_test::setup ()
{
    cleanup ();
    system ("touch ntfst-working");
}

void notifications_test::run ()
{
    consumer cons;
    events received;
    int wid = 0;


    cons.input.setup ("ntfst-working", IN_ALL_EVENTS);
    cons.output.wait ();

    wid = cons.output.added_watch_id ();
    should ("watch is added successfully", wid != -1);


    cons.output.reset ();
    cons.input.receive (2);

    system ("touch ntfst-working");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_OPEN on touch", contains (received, event ("", wid, IN_OPEN)));
    should ("receive IN_ATTRIB on touch", contains (received, event ("", wid, IN_ATTRIB)));
    should ("receive IN_CLOSE_WRITE on touch", contains (received, event ("", wid, IN_CLOSE_WRITE)));


    cons.output.reset ();
    cons.input.receive ();

    system ("cat ntfst-working > /dev/null");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_OPEN on read", contains (received, event ("", wid, IN_OPEN)));
    should ("receive IN_CLOSE_NOWRITE on read", contains (received, event ("", wid, IN_CLOSE_NOWRITE)));


    cons.output.reset ();
    cons.input.receive ();

    system ("echo Hello >> ntfst-working");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_OPEN on write", contains (received, event ("", wid, IN_OPEN)));
    should ("receive IN_MOFIFY on write", contains (received, event ("", wid, IN_MODIFY)));
    should ("receive IN_CLOSE_WRITE on write", contains (received, event ("", wid, IN_CLOSE_WRITE)));


    cons.output.reset ();
    cons.input.receive ();

    system ("mv ntfst-working ntfst-working-2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_MOVE_SELF on move", contains (received, event ("", wid, IN_MOVE_SELF)));

    
    cons.output.reset ();
    cons.input.receive ();

    system ("rm ntfst-working-2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_DELETE_SELF on remove", contains (received, event ("", wid, IN_DELETE_SELF)));
    should ("receive IN_IGNORED on remove", contains (received, event ("", wid, IN_IGNORED)));

    cons.input.interrupt ();
}

void notifications_test::cleanup ()
{
    system ("rm -rf ntfst-working");
}

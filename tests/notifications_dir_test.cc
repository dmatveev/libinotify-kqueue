#include <algorithm>
#include "notifications_dir_test.hh"

/* Always present on Linux (not sure about a concrete 2.6.x release)
 * May be "to be implemented" on BSDs */
#ifndef IN_IGNORED
#  define IN_IGNORED	 0x00008000
#endif

#ifndef IN_ISDIR
#  define IN_ISDIR	     0x40000000
#endif

notifications_dir_test::notifications_dir_test (journal &j)
: test ("Directory notifications", j)
{
}

void notifications_dir_test::setup ()
{
    cleanup ();

    system ("mkdir ntfsdt-working");
    system ("touch ntfsdt-working/foo");
    system ("touch ntfsdt-working/bar");

    system ("mkdir ntfsdt-cache");
    system ("touch ntfsdt-cache/bar");
}

void notifications_dir_test::run ()
{
    consumer cons;
    events received;
    events::iterator iter;
    int wid = 0;

    /* Add a watch */
    cons.input.setup ("ntfsdt-working",
                      IN_ATTRIB | IN_MODIFY
                      | IN_CREATE | IN_DELETE
                      | IN_MOVED_FROM | IN_MOVED_TO
                      | IN_MOVE_SELF | IN_DELETE_SELF);
    cons.output.wait ();

    wid = cons.output.added_watch_id ();
    should ("watch is added successfully", wid != -1);
    

    cons.output.reset ();
    cons.input.receive ();

    system ("touch ntfsdt-working");

    cons.output.wait ();
    received = cons.output.registered ();

    iter = std::find_if (received.begin(),
                         received.end(),
                         event_matcher (event ("", wid, IN_ATTRIB | IN_ISDIR)));
    should ("receive IN_ATTRIB event on touch on a directory",
            iter != received.end() && iter->flags & IN_ATTRIB);
    should ("the touch event for a directory contains IN_ISDIR in the flags",
            iter != received.end() && iter->flags & IN_ISDIR);


    cons.output.reset ();
    cons.input.receive ();

    system ("touch ntfsdt-working/1");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_CREATE event for a new entry in a directory (created with touch)",
            contains (received, event ("1", wid, IN_CREATE)));


    cons.output.reset ();
    cons.input.receive ();

    system ("echo Hello >> ntfsdt-working/2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_CREATE event for a new entry in a directory (created with echo)",
            contains (received, event ("2", wid, IN_CREATE)));


    cons.output.reset ();
    cons.input.receive ();

    system ("rm ntfsdt-working/2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_DELETE event on deleting a file from a directory",
            contains (received, event ("2", wid, IN_DELETE)));

    
#ifdef TESTS_MOVES_TRICKY
    /* And tricky test case to test renames in a directory. */
    cons.output.reset ();
    cons.input.receive (5);

    system ("mv ntfsdt-working/1 ntfsdt-working/one");

    cons.output.wait ();
    received = cons.output.registered ();

    events::iterator iter_from, iter_to;
    iter_from = std::find_if (received.begin(),
                              received.end(),
                              event_matcher (event ("1", wid, IN_MOVED_FROM)));
    iter_to = std::find_if (received.begin(),
                            received.end(),
                            event_matcher (event ("one", wid, IN_MOVED_TO)));

    if (should ("receive both IN_MOVED_FROM and IN_MOVED_TO for rename",
                iter_from != received.end () && iter_to != received.end())) {
        should ("both events for a rename have the same cookie",
                iter_from->cookie == iter_to->cookie);
    }
#else
    /* A simplier and less strict version */
    cons.output.reset ();
    cons.input.receive ();

    system ("mv ntfsdt-working/1 ntfsdt-working/one");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive all events on moves",
            contains (received, event ("1", wid, IN_MOVED_FROM))
            && contains (received, event ("one", wid, IN_MOVED_TO)));
#endif

    cons.output.reset ();
    cons.input.receive ();

    system ("echo Hello >> ntfsdt-working/one");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_MODIFY event on modifying an entry in a directory",
            contains (received, event ("one", wid, IN_MODIFY)));


    cons.output.reset ();
    cons.input.receive ();

    system ("mv ntfsdt-working/foo ntfsdt-working/bar");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive all move events when replaced a file in a directory "
            "with another file from this directory",
            contains (received, event ("foo", wid, IN_MOVED_FROM))
            && contains (received, event ("bar", wid, IN_MOVED_TO)));


    cons.output.reset ();
    cons.input.receive ();

    system ("touch ntfsdt-working/bar");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive events from a file, which has replaced a file in a directory",
            contains (received, event ("bar", wid, IN_ATTRIB)));


    cons.output.reset ();
    cons.input.receive ();

    system ("mv ntfsdt-cache/bar ntfsdt-working/bar");

    /* Interesting test case here.
     * Looks like inotify sends IN_MOVED_TO when overwriting a file with a file from
     * the same partition/fs, and sends a pair of IN_DELETE/IN_CREATE when overwriting
     * a file from a different partition/fs.
     */
    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive events when overwriting a file in a directory"
            " with an external file",
            (contains (received, event ("bar", wid, IN_DELETE))
             && contains (received, event ("bar", wid, IN_CREATE)))
            || (contains (received, event ("bar", wid, IN_MOVED_TO))));


    cons.output.reset ();
    cons.input.receive ();

    system ("touch ntfsdt-working/bar");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive events from a file, which has overwritten a file in a directory",
            contains (received, event ("bar", wid, IN_ATTRIB)));

    

    cons.output.reset ();
    cons.input.receive ();

    system ("mv ntfsdt-working ntfsdt-working-2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive a move event",
            contains (received, event ("", wid, IN_MOVE_SELF)));


    cons.output.reset ();
    cons.input.receive (4);

    system ("rm -rf ntfsdt-working-2");

    cons.output.wait ();
    received = cons.output.registered ();
    should ("receive IN_DELETE for a file \'one\' in a directory on removing a directory",
            contains (received, event ("one", wid, IN_DELETE)));
    should ("receive IN_DELETE for a file \'bar\' in a directory on removing a directory",
            contains (received, event ("bar", wid, IN_DELETE)));
    should ("receive IN_DELETE_SELF on removing a directory",
            contains (received, event ("", wid, IN_DELETE_SELF)));
    should ("receive IN_IGNORED on removing a directory",
            contains (received, event ("", wid, IN_IGNORED)));
    
    cons.input.interrupt ();
}

void notifications_dir_test::cleanup ()
{
    system ("rm -rf ntfsdt-working-2");
    system ("rm -rf ntfsdt-working");
    system ("rm -rf ntfsdt-cache");
}

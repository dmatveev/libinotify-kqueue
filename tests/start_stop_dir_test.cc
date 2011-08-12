#include <cstdlib>
#include "start_stop_dir_test.hh"

/* Always present on Linux (not sure about a concrete 2.6.x release)
 * May be "to be implemented" on BSDs */
#ifndef IN_IGNORED
#  define IN_IGNORED	 0x00008000
#endif

start_stop_dir_test::start_stop_dir_test (journal &j)
: test ("Start-stop directory test", j)
{
}

void start_stop_dir_test::setup ()
{
    system ("rm -rf ssdt-working");

    system ("mkdir ssdt-working");
    system ("touch ssdt-working/1");
    system ("touch ssdt-working/2");
    system ("touch ssdt-working/3");
}

void start_stop_dir_test::run ()
{
    consumer cons;
    int wid = 0;

    /* Add a watch */
    cons.input.setup ("ssdt-working", IN_ATTRIB);
    cons.output.wait ();

    wid = cons.output.added_watch_id ();

    should ("watch is added successfully", wid != -1);

    /* Tell consumer to watch for an IN_ATTRIB event */
    events expected;
    expected.insert (event ("", wid, IN_ATTRIB));
    cons.output.reset ();
    cons.input.setup (expected, 1);

    /* Produce activity */
    system ("touch ssdt-working");

    cons.output.wait ();

    expected = cons.output.left_unregistered ();
    should ("events are registered on a directory", expected.empty ());

    /* Watch should also signal us about activity on files at the watched directory. */
    expected = events ();
    expected.insert (event ("1", wid, IN_ATTRIB));
    expected.insert (event ("2", wid, IN_ATTRIB));
    expected.insert (event ("3", wid, IN_ATTRIB));

    cons.output.reset ();
    cons.input.setup (expected, 1);

    /* This events should be registered */
    system ("touch ssdt-working/1");
    system ("touch ssdt-working/2");
    system ("touch ssdt-working/3");
    
    cons.output.wait ();

    expected = cons.output.left_unregistered ();
    should ("events are registered on the directory contents", expected.empty ());

    /* Now stop watching */
    cons.output.reset ();
    cons.input.setup (wid);
    cons.output.wait ();

    /* Linux inotify sends IN_IGNORED on stop */
    expected = events ();
    expected.insert (event ("", wid, IN_IGNORED));

    cons.output.reset ();
    cons.input.setup (expected, 1);
    cons.output.wait ();

    expected = cons.output.left_unregistered ();
    should ("got IN_IGNORED on watch stop", expected.empty ());
    
    // /* Tell again to consumer to watch for an IN_ATTRIB event  */
    // expected = events();
    // expected.insert (event ("", wid, IN_ATTRIB));
    // cons.output.reset ();
    // cons.input.setup (expected, 1);
    
    // /* Produce activity. Consumer should not see it */
    // system ("touch ssdt-working");

    // cons.output.wait ();
    // expected = cons.output.left_unregistered ();

    // should ("events should not be registered on a removed watch", expected.size() == 1);

    // /* Now start watching again. Everything should work */
    // cons.input.setup ("ssdt-working", IN_ATTRIB);
    // cons.output.wait ();

    // wid = cons.output.added_watch_id ();

    // should ("start watching a file after stop", wid != -1);

    // /* Tell consumer to watch for an IN_ATTRIB event */
    // expected = events ();
    // expected.insert (event ("", wid, IN_ATTRIB));
    // cons.output.reset ();
    // cons.input.setup (expected, 1);

    // /* Produce activity, consumer should watch it */
    // system ("touch ssdt-working");

    // cons.output.wait ();

    // expected = cons.output.left_unregistered ();
    // should ("all produced events are registered after resume", expected.empty ());

    cons.input.interrupt ();
}

void start_stop_dir_test::cleanup ()
{
    system ("rm -rf ssdt-working");
}

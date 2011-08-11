#include <cassert>
#include <iostream>
#include "test.hh"

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

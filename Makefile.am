############################################################
#	The library
#------------------------------------------------------------

lib_LTLIBRARIES = libinotify.la

libinotify_la_SOURCES = \
	utils.c \
	conversions.c \
	dep-list.c \
	watch.c \
	worker-sets.c \
	worker-thread.c \
	worker.c \
	controller.c

libinotify_la_LDFLAGS = -lpthread


############################################################
#	Test suite
#------------------------------------------------------------

EXTRA_PROGRAMS = check_libinotify

test: check_libinotify
	@echo Running test suite...
	@./check_libinotify

.PHONY: test

check_libinotify_SOURCES = \
	tests/core/log.cc \
	tests/core/event.cc \
	tests/core/action.cc \
	tests/core/request.cc \
	tests/core/response.cc \
	tests/core/inotify_client.cc \
	tests/core/consumer.cc \
	tests/core/journal.cc \
	tests/core/test.cc \
	tests/start_stop_test.cc \
	tests/start_stop_dir_test.cc \
	tests/fail_test.cc \
	tests/notifications_test.cc \
	tests/notifications_dir_test.cc \
	tests/update_flags_test.cc \
	tests/update_flags_dir_test.cc \
	tests/open_close_test.cc \
	tests/tests.cc

check_libinotify_LDADD = libinotify.la

noinst_programs = check_libinotify
UNAME=$(shell uname)
APP=tests
CXX=g++
CXXFLAGS=-I.. -O2 -Wall -ggdb
LDFLAGS=-lpthread


# Uncomment this line to enable debug logging.
# The tests suite should be rebuilt completely for that.
CXXFLAGS+=-DENABLE_LOGGING

ifeq ($(UNAME),Linux)
	CXXFLAGS+=-std=c++0x
endif

ifeq ($(UNAME),NetBSD)
	LDFLAGS+=../blob.a
endif

CORE_OBJS=\
	core/log.o \
	core/event.o \
	core/action.o \
	core/request.o \
	core/response.o \
	core/inotify_client.o \
	core/consumer.o \
	core/journal.o \
	core/test.o

TEST_OBJS=\
	start_stop_test.o \
	start_stop_dir_test.o \
	tests.o

tests-app: $(TEST_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(CORE_OBJS) $(LDFLAGS) -o $(APP)

run:
	@./tests

clean:
	rm -f *~ *.o core/*~ core/*.o

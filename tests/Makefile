UNAME=$(shell uname)
APP=test
CXX=g++
CXXFLAGS=-I.. -O2 -Wall -ggdb
LDFLAGS=-lpthread

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

test: test.cc $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) test.cc $(CORE_OBJS) $(LDFLAGS) -o $(APP)

clean:
	rm -f *~ *.o core/*~ core/*.o

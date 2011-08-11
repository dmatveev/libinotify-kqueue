APP=test
CXX=g++
CFLAGS=-I.. -O2 -Wall -ggdb
LDFLAGS=-lpthread

test: test.cc
	$(CXX) test.cc ../blob.a $(LDFLAGS) -o $(APP)

linux: test.cc
	$(CXX) test.cc -std=c++0x $(LDFLAGS) -o $(APP)

clean:
	rm -f *~ *.o

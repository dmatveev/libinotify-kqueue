APP=test
CXX=g++
CFLAGS=-I.. -O2 -Wall -ggdb
LDFLAGS=-lpthread

test: test.o
	$(CXX) test.o ../blob.a $(LDFLAGS) -o $(APP)

clean:
	rm -f *~ *.o

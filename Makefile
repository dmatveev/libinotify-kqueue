APP=test
APP_CFLAGS=-O2 -Wall
APP_LDFLAGS=-lpthread

CC=gcc
COBJ=$(CC) $(APP_CFLAGS) -c

OBJS=worker-sets.o worker.o controller.o test.o

test: $(OBJS)
	$(CC) $(OBJS) $(APP_LDFLAGS) -o $(APP)

worker-sets.o: worker-sets.c
	$(COBJ) worker-sets.c

worker.o: worker.c
	$(COBJ) worker.c

controller.o: controller.c
	$(COBJ) controller.c

test.o: test.c
	$(COBJ) test.c

clean:
	rm -f *~ *.o

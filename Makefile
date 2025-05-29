OBJS = build/jobCommander.o
OBJS_S = build/jobExecutorServer.o build/queue.o
OUT = bin/jobCommander
OUT_S = bin/jobExecutorServer
CC = gcc
CFLAGS = -g -c -Iinclude
LIBS = -pthread

all: $(OUT_S) $(OUT)

$(OUT_S): $(OBJS_S)
	$(CC) -g $(OBJS_S) -o $(OUT_S) $(LIBS)

$(OUT): $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LIBS)

#Separate compilation
build/jobCommander.o: src/jobCommander.c include/queue.h
	$(CC) $(CFLAGS) src/jobCommander.c -o build/jobCommander.o

build/jobExecutorServer.o: src/jobExecutorServer.c include/queue.h
	$(CC) $(CFLAGS) src/jobExecutorServer.c -o build/jobExecutorServer.o

build/queue.o: src/queue.c include/queue.h
	$(CC) $(CFLAGS) src/queue.c -o build/queue.o

clean:
	rm -f $(OUT_S) $(OBJS_S) $(OUT) $(OBJS)

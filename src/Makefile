OBJS=libclient.o TCPBenchmark.o
SRCS=$(OBJS:%.o=%.c)
# CFLAGS=-g3 -Wall -std=c99 -D_GNU_SOURCE
CFLAGS=-O3 -Wall -std=c99 -D_GNU_SOURCE
LDLIBS=-lpthread
TARGET=tcpbenchmark
$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

clean:
	-rm -f ${OBJS} ${TARGET} ${TARGET}.exe


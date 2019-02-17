OBJS=libclient.o TCPBenchmark.o
SRCS=$(OBJS:%.o=%.c)
CFLAGS=-g -Wall
LDLIBS=-lpthread
TARGET=tcpbenchmark
$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

clean:
	-rm -f ${OBJS} ${TARGET} ${TARGET}.exe


CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2
LDFLAGS = -lrt

all: ping pong pingpong_singlenode

ping: ping.c
	$(CC) $(CFLAGS) -o ping ping.c

pong: pong.c
	$(CC) $(CFLAGS) -o pong pong.c

pingpong_singlenode: pingpong_singlenode.c
	$(CC) $(CFLAGS) -o pingpong_singlenode pingpong_singlenode.c

clean:
	rm -f ping pong pingpong_singlenode

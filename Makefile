CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2
LDFLAGS = -lrt

all: ping pong pingpong_singnode_2core

ping: ping.c
	$(CC) $(CFLAGS) -o ping ping.c

pong: pong.c
	$(CC) $(CFLAGS) -o pong pong.c

pingpong_singnode_2core: pingpong_singnode_2core.c
	$(CC) $(CFLAGS) -o pingpong_singnode pingpong_singnode_2core.c

clean:
	rm -f ping pong pingpong_singnode

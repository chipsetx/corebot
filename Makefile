CC=gcc
CFLAGS=-D_POSIX_SOURCE -std=c99 -g -O2 -Wall
LIBS=-ldl

all:
	$(CC) $(CFLAGS) -shared -o modules/server.so modules/server.c
	$(CC) $(CFLAGS) -rdynamic -o corebot bot.c -ldl

clean:
	rm -f modules/*.so corebot

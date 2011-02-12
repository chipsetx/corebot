CC=gcc
CFLAGS=-D_POSIX_SOURCE -std=c99 -g -O2 -Wall
LIBS=-ldl

all:
	$(CC) $(CFLAGS) -shared -o modules/server.so modules/server.c
	$(CC) $(CFLAGS) -shared -o modules/irc.so modules/irc.c
	$(CC) $(CFLAGS) -Wl,--export-dynamic -o corebot bot.c log.c $(LIBS)

clean:
	rm -f modules/*.so corebot

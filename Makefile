#
# GNU Makefile
#

CC=gcc
CFLAGS=-D_POSIX_SOURCE -D_BSD_SOURCE -std=c89 -pedantic-errors -fno-strict-aliasing -g -O2 -Wall
LIBS=-ldl

CFLAGS+=-DGIT_REV="\"$(shell git rev-parse --short HEAD)\""

all:
	$(CC) $(CFLAGS) -shared -o modules/server.so modules/server.c
	$(CC) $(CFLAGS) -shared -o modules/irc.so modules/irc.c
	$(CC) $(CFLAGS) -shared -o modules/uinfo.so modules/uinfo.c
	$(CC) $(CFLAGS) -shared -o modules/pong.so modules/pong.c
	$(CC) $(CFLAGS) -Wl,--export-dynamic -o corebot bot.c log.c config.c $(LIBS)

clean:
	rm -f modules/*.so corebot

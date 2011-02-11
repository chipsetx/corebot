CC=gcc
CFLAGS=-g -O2 -Wall
LIBS=-ldl

all:
	$(CC) $(CFLAGS) -shared -o modules/server.so modules/server.c
	$(CC) $(CFLAGS) -rdynamic -o corebot bot.c -ldl

clean:
	rm -f modules/*.so corebot

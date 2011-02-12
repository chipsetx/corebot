/*
 * Copyright (c) 2011 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "../bot.h"

/* memcpy */
#include <string.h>

/* socket */
#include <sys/types.h>
#include <sys/socket.h>

/* close */
#include <unistd.h>

/* addrinfo */
#include <netdb.h>

CTX server_ctx = NULL;

#define BUF_SIZE 1024

int net_sock = 0;
int connected = 0;

/* XXX: implement a list when more than one raw listener required */
void (*server_cb)(const char *) = NULL;
CTX server_cb_ctx = NULL;

void server_register_cb(void (*cb)(const char *))
{
    server_cb_ctx = bot_get_ctx();

    bot_ctx(server_ctx);
    if (server_cb != NULL)
    {
        log_printf("warning: server callback overridden!\n");
    }
    bot_ctx(server_cb_ctx);

    server_cb = cb;
}

void server_unregister_cb(void (*cb)(const char *))
{
    if (server_cb == cb)
    {
        server_cb = NULL;
    }
}

int server_resolv(char *host, int port, struct sockaddr *net_server, socklen_t *net_addrlen)
{
    struct addrinfo *addr_list = NULL;
    struct addrinfo *addr;

    if (getaddrinfo(host, NULL, NULL, &addr_list) == 0)
    {
        if (addr_list == NULL)
        {
            return -1;
        }

        addr = addr_list;

        while (addr->ai_next)
        {
            if (addr->ai_family == AF_INET)
            {
                *net_addrlen = addr->ai_addrlen;
                memcpy(net_server, addr->ai_addr, addr->ai_addrlen);
                ((struct sockaddr_in *)net_server)->sin_port = htons(port);
                freeaddrinfo(addr_list);
                return 1;
            }
            addr++;
        }

        freeaddrinfo(addr_list);
    }

    return -1;
}

int server_init(CTX ctx)
{
    server_ctx = ctx;

    if (!(net_sock = socket(AF_INET, SOCK_STREAM, 0)))
    {
        log_printf("error creating socket");
        return -1;
    }

    bot_register_fd(net_sock);

    log_printf("socket: %d\n", net_sock);

    return 1;
}

void server_timer()
{
    struct sockaddr net_server;
    socklen_t net_addrlen;

    if (!connected)
    {
        if (!server_resolv("chat.eu.freenode.net", 6667, &net_server, &net_addrlen))
        {
            log_printf("error resolving\n");
            return;
        }

        log_printf("resolved\n");

        if (connect(net_sock, &net_server, net_addrlen) == 0)
        {
            connected = 1;
            log_printf("connected\n");
        }
        else
        {
            perror("connect");
        }
    }
}

void server_read(int read)
{
    static char buf[BUF_SIZE];
    static int off = 0;
    int len;
    memset(buf+off, 0, BUF_SIZE - off);

    if ( (len = recv(read, buf+off, BUF_SIZE - off, 0)) > 0)
    {
        len += off;

        char *line = buf;
        char *ptr = buf;
        char *last = buf;

        while ( (ptr = strstr(ptr, "\r\n")) )
        {
            *ptr++ = '\0';
            *ptr++ = '\0';

            if (server_cb)
            {
                bot_ctx(server_cb_ctx);
                server_cb(line);
                bot_ctx(server_ctx);
            }

            line = ptr;
            last = ptr;
        }

        off = len - (last - buf);
        if (off > BUF_SIZE - 1)
        {
            /* discard invalid buffers */
            off = 0;
        }
        else if (off > 0)
        {
            memcpy(buf, last, off);
        }
    }
    else
    {
        log_printf("disconnected\n");
        close(net_sock);
        bot_unregister_fd();
        connected = 0;
    }
}

void server_free()
{
    if (net_sock)
    {
        bot_unregister_fd();
        close(net_sock);
    }
}

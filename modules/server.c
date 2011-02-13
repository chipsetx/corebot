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
#include "server.h"

/* memcpy */
#include <string.h>

/* socket */
#include <sys/types.h>
#include <sys/socket.h>

/* close */
#include <unistd.h>

/* addrinfo */
#include <netdb.h>

/* sockaddr_in */
#include <netinet/in.h>

/* malloc */
#include <stdlib.h>

CTX server_ctx = NULL;

#define BUF_SIZE 1024

int net_sock = 0;
int connected = 0;

static TAILQ_HEAD(cb_head, cb_entry) cb_h;

struct cb_entry
{
    SERVER_CB cb;
    CTX ctx;
    TAILQ_ENTRY(cb_entry) cb_entries;
};

void server_register_cb(SERVER_CB cb)
{
    struct cb_entry *e;

    TAILQ_FOREACH(e, &cb_h, cb_entries)
    {
        if (e->cb == cb)
        {
            /* already registered */
            return;
        }
    }

    e = malloc(sizeof(struct cb_entry));
    e->ctx = bot_get_ctx();
    e->cb = cb;
    TAILQ_INSERT_TAIL(&cb_h, e, cb_entries);
}

void server_unregister_cb(SERVER_CB cb)
{
    struct cb_entry *e;
    TAILQ_FOREACH(e, &cb_h, cb_entries)
    {
        if (e->cb == cb)
        {
            TAILQ_REMOVE(&cb_h, e, cb_entries);
            free(e);
            break;
        }
    }
}

int server_resolv(const char *host, int port, struct sockaddr *net_server, socklen_t *net_addrlen)
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

    TAILQ_INIT(&cb_h);

    if (!(net_sock = socket(AF_INET, SOCK_STREAM, 0)))
    {
        log_printf("error creating socket");
        return -1;
    }

    bot_register_fd(net_sock);

    log_printf("socket: %d\n", net_sock);

    return 1;
}

void server_send(const char *msg)
{
    send(net_sock, msg, strlen(msg), 0);
}

void server_timer()
{
    struct sockaddr net_server;
    socklen_t net_addrlen;
    const char *host;
    const char *port;

    if (!connected)
    {
        host = config_get("host");
        port = config_get("port");

        if (host == NULL || port == NULL)
        {
            log_printf("host or port missing in config, can't connect\n");
            return;
        }

        log_printf("connecting to %s:%s\n", host, port);

        if (!server_resolv(host, atoi(port), &net_server, &net_addrlen))
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
    struct cb_entry *e;
    char *line;
    char *ptr;
    char *last;

    memset(buf+off, 0, BUF_SIZE - off);

    if ( (len = recv(read, buf+off, BUF_SIZE - off, 0)) > 0)
    {
        len += off;

        line = buf;
        ptr = buf;
        last = buf;

        while ( (ptr = strstr(ptr, "\r\n")) )
        {
            *ptr++ = '\0';
            *ptr++ = '\0';

            TAILQ_FOREACH(e, &cb_h, cb_entries)
            {
                bot_ctx(e->ctx);
                e->cb(line);
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
    struct cb_entry *e;
    while ( (e = TAILQ_FIRST(&cb_h)) )
    {
        TAILQ_REMOVE(&cb_h, e, cb_entries);
        free(e);
    }

    if (net_sock)
    {
        bot_unregister_fd();
        close(net_sock);
    }
}

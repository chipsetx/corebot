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
#include <errno.h>

/* close */
#include <unistd.h>

/* addrinfo */
#include <netdb.h>

/* inet_ntop */
#include <arpa/inet.h>

/* sockaddr_in */
#include <netinet/in.h>

/* malloc */
#include <stdlib.h>

/* time */
#include <time.h>

CTX server_ctx = NULL;

#define BUF_SIZE 1024

static int net_sock = 0;
static int connected = 0;
static int server_last_connect = 0;
static int server_reconnect = 30;

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

int server_resolv(const char *host, const char *port, struct sockaddr_storage *net_server, socklen_t *net_addrlen)
{
    struct addrinfo *addr;
    struct addrinfo hint;
    int ret, s;
    char buf[INET6_ADDRSTRLEN];

    if (STR_TRUE(config_get("v4only")))
    {
        hint.ai_family = AF_INET;
    }
    else if (STR_TRUE(config_get("v6only")))
    {
        hint.ai_family = AF_INET6;
    }
    else
    {
        hint.ai_family = PF_UNSPEC;
    }
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;
    hint.ai_flags = AI_NUMERICSERV;

    if ( (ret = getaddrinfo(host, port, &hint, &addr)) == 0)
    {
        if (addr == NULL)
        {
            return 0;
        }

        if (!(s = socket(addr->ai_family, SOCK_STREAM, 0)))
        {
            log_printf("Error creating socket for family %d", addr->ai_family);
            return 0;
        }

        if (inet_ntop(addr->ai_family, addr->ai_addr, buf, addr->ai_addrlen))
        {
            log_printf("Resolved to %s\n", buf);
        }

        *net_addrlen = addr->ai_addrlen;
        memcpy(net_server, addr->ai_addr, addr->ai_addrlen);

        freeaddrinfo(addr);

        return s;
    }

    log_printf("Error resolving: %s\n", gai_strerror(ret));

    return 0;
}

int server_init(CTX ctx)
{
    server_ctx = ctx;

    TAILQ_INIT(&cb_h);

    return 1;
}

void server_send(const char *msg)
{
    send(net_sock, msg, strlen(msg), 0);
}

void server_timer()
{
    struct sockaddr_storage net_server;
    socklen_t net_addrlen;
    const char *host;
    const char *port;
    int now, ret;

    if (!connected)
    {
        now = time(NULL);

        if (server_last_connect + server_reconnect > now)
        {
            return;
        }

        server_last_connect = now;

        host = config_get("host");
        port = config_get("port");

        if (host == NULL || port == NULL)
        {
            log_printf("Host or port missing in config, can't connect\n");
            return;
        }

        log_printf("Connecting to %s:%s...\n", host, port);

        net_sock = server_resolv(host, port, &net_server, &net_addrlen);

        if (!net_sock)
        {
            return;
        }

        bot_register_fd(net_sock);

        if ( (ret = connect(net_sock, (struct sockaddr *)&net_server, net_addrlen)) == 0)
        {
            connected = 1;
            log_printf("Connected.\n");
        }
        else
        {
            log_printf("Error: %s\n", strerror(errno));
            close(net_sock);
            net_sock = 0;
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
        log_printf("Disconnected.\n");
        close(net_sock);
        bot_unregister_fd();
        net_sock = 0;
        connected = 0;
        server_last_connect = time(NULL);
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

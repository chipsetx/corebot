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

int net_sock = 0;

int server_sock4(char *host, int port, int *net_sock, struct sockaddr *net_server, socklen_t *net_addrlen)
{
    int s = 0;
    struct addrinfo *addr_list = NULL;
    struct addrinfo *addr;

    if (!(s = socket(AF_INET, SOCK_STREAM, 0)))
    {
        return -1;
    }

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
                *net_sock = s;
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

int server_init()
{
    struct sockaddr net_server;
    socklen_t net_addrlen;

    printf("server: init\n");

    if (!server_sock4("chat.eu.freenode.net", 6667, &net_sock, &net_server, &net_addrlen))
    {
        fprintf(stderr, "server: error resolving\n");
        return -1;
    }

    if (connect(net_sock, &net_server, net_addrlen) == 0)
    {
        printf("server: connected\n");
        bot_register_fd(net_sock);
    }
    else
    {
        perror("connect");
    }

    return 1;
}

void server_read(int read)
{
    char net_buf[512];
    memset(net_buf, 0, 512);

    if (recv(net_sock, net_buf, 512, 0) > 0) {
        printf("%s", net_buf);
    } else {
        printf("server: socket closed unexpectedly\n");
        close(net_sock);
        bot_unregister_fd();
        net_sock = 0;
    }
}

void server_free()
{
    printf("server: free\n");
    bot_unregister_fd();

    if (net_sock)
    {
        close(net_sock);
    }
}

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

#include <dlfcn.h>

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include <time.h>

#include "bot.h"

#define EACH_MODULE(a, b) \
    for (struct bot_module *b = a; b->name; b++)

#define EACH_LOADED_MODULE(a, b) \
    for (struct bot_module *b = a; b->dl; b++)

struct bot_module modules[] =
{
    { "server" },
    { "irc" },
    { NULL }
};

struct bot_module *_bot_context = NULL;
#define bot_ctx(a) _bot_context = a

int main(int argc, char **argv)
{
    char str_buf[512];
    fd_set sockets;
    int fd_max;
    time_t last_event;
    time_t now;
    struct timeval tv;

    /* load modules */
    EACH_MODULE(modules, mod)
    {
        sprintf(str_buf, "modules/%s.so", mod->name);

        printf("loading \"%s\"\n", str_buf);

        mod->dl = dlopen(str_buf, RTLD_LAZY|RTLD_GLOBAL);

        if (mod->dl == NULL)
        {
            fprintf(stderr, "%s\n", dlerror());
        }
        else
        {
            printf("  loaded as 0x%08X\n", (int)mod->dl);

            sprintf(str_buf, "%s_init", mod->name);
            mod->init = dlsym(mod->dl, str_buf);
            printf("    init at 0x%08X\n", (int)mod->init);

            sprintf(str_buf, "%s_read", mod->name);
            mod->read = dlsym(mod->dl, str_buf);
            printf("    read at 0x%08X\n", (int)mod->read);

            sprintf(str_buf, "%s_timer", mod->name);
            mod->timer = dlsym(mod->dl, str_buf);
            printf("    timer at 0x%08X\n", (int)mod->timer);

            sprintf(str_buf, "%s_free", mod->name);
            mod->free = dlsym(mod->dl, str_buf);
            printf("    free at 0x%08X\n", (int)mod->free);

            if (mod->init)
            {
                bot_ctx(mod);
                mod->init();
                bot_ctx(NULL);
            }
        }
    }

    last_event = time(NULL);

    /* main fd loop */
    while(1)
    {
        /* XXX: optimize and simplify this by caching fd_set and regenerating it only when the set really changes */
        fd_max = 0;
        FD_ZERO(&sockets);

        EACH_MODULE(modules, mod)
        {
            if (mod->sock)
            {
                FD_SET(mod->sock, &sockets);

                if (mod->sock > fd_max)
                {
                    fd_max = mod->sock;
                }
            }
        }

        /* the only point of exit is when there are no fds to select from anymore */
        if (fd_max == 0)
            break;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(fd_max + 1, &sockets, NULL, NULL, &tv) > 0)
        {
            EACH_MODULE(modules, mod)
            {
                if (mod->sock)
                {
                    if (FD_ISSET(mod->sock, &sockets))
                    {
                        bot_ctx(mod);
                        mod->read(mod->sock);
                        bot_ctx(NULL);
                    }
                }
            }
        }

        /* handle timer */
        now = time(NULL);
        if (now > last_event)
        {
            EACH_MODULE(modules, mod)
            {
                if (mod->timer)
                {
                    bot_ctx(mod);
                    mod->timer();
                    bot_ctx(NULL);
                }
            }
        }
    }

    /* module cleanup */
    EACH_LOADED_MODULE(modules, mod)
    {
        sprintf(str_buf, "modules/%s.so", mod->name);

        printf("unloading \"%s\"\n", str_buf);

        if (mod->free)
        {
            bot_ctx(mod);
            mod->free();
            bot_ctx(NULL);
        }

        if (mod->dl)
        {
            dlclose(mod->dl);
        }
    }

    return 0;
}

void bot_register_fd(int sock)
{
    if (_bot_context && _bot_context->read)
    {
        _bot_context->sock = sock;
    }
}

void bot_unregister_fd()
{
    if (_bot_context)
    {
        _bot_context->sock = 0;
    }
}

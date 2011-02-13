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

#include "bot.h"

#include <dlfcn.h>

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>

#include <time.h>

void parse_config()
{
    struct bot_module *server = malloc(sizeof(struct bot_module));
    struct bot_module *irc = malloc(sizeof(struct bot_module));
    struct bot_module *uinfo = malloc(sizeof(struct bot_module));
    struct bot_module *pong = malloc(sizeof(struct bot_module));

    memset(server, 0, sizeof(struct bot_module));
    memset(irc, 0, sizeof(struct bot_module));
    memset(uinfo, 0, sizeof(struct bot_module));
    memset(pong, 0, sizeof(struct bot_module));

    server->name = "server";
    irc->name = "irc";
    uinfo->name = "uinfo";
    pong->name = "pong";

    TAILQ_INSERT_TAIL(&modules_head, server, bot_modules);
    TAILQ_INSERT_TAIL(&modules_head, irc, bot_modules);
    TAILQ_INSERT_TAIL(&modules_head, uinfo, bot_modules);
    TAILQ_INSERT_TAIL(&modules_head, pong, bot_modules);
}

struct bot_module *_bot_context = NULL;

int main(int argc, char **argv)
{
    fd_set sockets;
    int fd_max;
    time_t last_event = 0;
    time_t now;
    struct timeval tv;
    struct bot_module *mod;

    TAILQ_INIT(&modules_head);

    parse_config();

    /* load modules */
    TAILQ_FOREACH(mod, &modules_head, bot_modules)
    {
        bot_module_load(mod);
    }

    /* main fd loop */
    while(1)
    {
        /* handle timer */
        now = time(NULL);
        if (now > last_event)
        {
            TAILQ_FOREACH(mod, &modules_head, bot_modules)
            {
                if (mod->dl && mod->timer)
                {
                    bot_ctx(mod);
                    mod->timer();
                    bot_ctx(NULL);
                }
            }

            last_event = now;
        }

        /* XXX: optimize and simplify this by caching fd_set and regenerating it only when the set really changes */
        fd_max = 0;
        FD_ZERO(&sockets);

        TAILQ_FOREACH(mod, &modules_head, bot_modules)
        {
            if (mod->dl && mod->sock)
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
            TAILQ_FOREACH(mod, &modules_head, bot_modules)
            {
                if (mod->dl && mod->sock)
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
    }

    /* module cleanup */
    while ( (mod = TAILQ_LAST(&modules_head, _modules_head)) )
    {
        bot_module_free(mod);
        TAILQ_REMOVE(&modules_head, mod, bot_modules);
        free(mod);
    }

    return 0;
}

int bot_module_load(struct bot_module *mod)
{
    char str_buf[512];

    snprintf(str_buf, 512, "modules/%s.so", mod->name);

    log_printf("loading \"%s\"\n", str_buf);

    mod->dl = dlopen(str_buf, RTLD_LAZY|RTLD_GLOBAL);

    if (mod->dl == NULL)
    {
        log_printf("error: %s\n", dlerror());
    }
    else
    {
        log_printf("  loaded as 0x%08X\n", (int)mod->dl);

        snprintf(str_buf, 512, "%s_init", mod->name);
        *(void **)(&mod->init) = dlsym(mod->dl, str_buf);
        log_printf("    init at 0x%08X\n", (int)mod->init);

        snprintf(str_buf, 512, "%s_read", mod->name);
        *(void **)(&mod->read) = dlsym(mod->dl, str_buf);
        log_printf("    read at 0x%08X\n", (int)mod->read);

        snprintf(str_buf, 512, "%s_timer", mod->name);
        *(void **)(&mod->timer) = dlsym(mod->dl, str_buf);
        log_printf("    timer at 0x%08X\n", (int)mod->timer);

        snprintf(str_buf, 512, "%s_free", mod->name);
        *(void **)(&mod->free) = dlsym(mod->dl, str_buf);
        log_printf("    free at 0x%08X\n", (int)mod->free);

        if (mod->init)
        {
            bot_ctx(mod);
            mod->version = mod->init(mod);
            bot_ctx(NULL);

            if (mod->version < 0)
            {
                bot_module_free(mod);
                return 0;
            }
        }
    }

    return 1;
}

void bot_module_free(struct bot_module *mod)
{
    char str_buf[512];

    snprintf(str_buf, 512, "modules/%s.so", mod->name);

    log_printf("unloading \"%s\"\n", str_buf);

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

    mod->dl = NULL;
    mod->version = -1;

    mod->init = NULL;
    mod->read = NULL;
    mod->timer = NULL;
    mod->free = NULL;
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

int bot_require(const char *name, int version)
{
    struct bot_module *mod;

    TAILQ_FOREACH(mod, &modules_head, bot_modules)
    {
        if (mod->dl && strcmp(mod->name, name) == 0)
        {
            if (mod->version < version)
            {
                return 0;
            }
            return 1;
        }
    }

    return -1;
}

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
#include "irc.h"
#include "server.h"

#include <stdarg.h>
#include <string.h>
#include <regex.h>

CTX irc_ctx = NULL;

static regex_t preg;

static TAILQ_HEAD(cb_head, cb_entry) cb_h;

struct cb_entry
{
    IRC_CB cb;
    CTX ctx;
    TAILQ_ENTRY(cb_entry) cb_entries;
};

void irc_register_cb(IRC_CB cb)
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

void irc_unregister_cb(IRC_CB cb)
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

void irc_memcpy(char *dst, const char *src, int len)
{
    memcpy(dst, src, len);
    *(dst + len) = '\0';
}

void irc_process(const char *line)
{
    struct cb_entry *e;
    regmatch_t pmatch[9];

    char prefix[512];
    char command[512];
    char params[512];
    char trail[512];

    char *pprefix = NULL;
    char *pparams = NULL;
    char *ptrail = NULL;

    if (regexec(&preg, line, 9, pmatch, 0) != REG_NOMATCH)
    {
        irc_memcpy(prefix, line + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
        irc_memcpy(command, line + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
        irc_memcpy(params, line + pmatch[5].rm_so, pmatch[5].rm_eo - pmatch[5].rm_so);
        irc_memcpy(trail, line + pmatch[8].rm_so, pmatch[8].rm_eo - pmatch[8].rm_so);

        if (strlen(prefix) > 0)
        {
            pprefix = prefix;
        }

        if (strlen(params) > 0)
        {
            pparams = params;
        }

        if (strlen(trail) > 0)
        {
            ptrail = trail;
        }

        /* log some meaningful messages */
        if (strcmp(command, "001") == 0 || strcmp(command, "002") == 0 || strcmp(command, "003") == 0 || strcmp(command, "020") == 0 ||
                (strcmp(command, "NOTICE") == 0 && pparams && (strcmp(pparams, "*") == 0 || strcmp(pparams, "AUTH") == 0)))
        {
            log_printf("%s\n", trail);
        }

        if (strcmp(command, "MODE") == 0 && pprefix && pparams && strcmp(pprefix, pparams) == 0)
        {
            log_printf("User mode set to %s\n", trail);
        }

        if (strcmp(command, "ERROR") == 0)
        {
            log_printf("Error: %s\n", ptrail);
        }

        #ifdef IRC_DEBUG
        log_printf("-> %s %s %s :%s\n", pprefix, command, pparams, ptrail);
        #endif

        TAILQ_FOREACH(e, &cb_h, cb_entries)
        {
            bot_ctx(e->ctx);
            e->cb(pprefix, command, pparams, ptrail);
            bot_ctx(irc_ctx);
        }
    }
}

int irc_printf(const char *fmt, ...)
{
    va_list args;
    int ret;
    char buf[512];
    CTX caller_ctx = bot_get_ctx();

    va_start(args, fmt);
    ret = vsnprintf(buf, 512, fmt, args);
    va_end(args);

    bot_ctx(irc_ctx);

    #ifdef IRC_DEBUG
    log_printf("<- %s", buf);
    #endif
    server_send(buf);

    bot_ctx(caller_ctx);

    return ret;
}

int irc_init(CTX ctx)
{
    irc_ctx = ctx;

    TAILQ_INIT(&cb_h);

    if (bot_require("server", 1) < 1)
    {
        printf("error: server module required\n");
        return -1;
    }

    /* an edge case where params have colons will break, luckily it is rarely used */
    if (regcomp(&preg, "^(:([^ ]+) )?([^ ]+)( ([^:]+))?( (:(.+)))?$", REG_EXTENDED) != 0)
    {
        printf("error compiling matching regexp\n");
        return -1;
    }

    server_register_cb(irc_process);

    return 1;
}

void irc_free()
{
    struct cb_entry *e;
    while ( (e = TAILQ_FIRST(&cb_h)) )
    {
        TAILQ_REMOVE(&cb_h, e, cb_entries);
        free(e);
    }

    server_unregister_cb(irc_process);
    regfree(&preg);
}

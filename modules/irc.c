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

#include <string.h>
#include <regex.h>

CTX irc_ctx = NULL;

void server_register_cb(void (*cb)(const char *));
void server_unregister_cb(void (*cb)(const char *));

regex_t preg;

void irc_memcpy(char *dst, const char *src, int len)
{
    memcpy(dst, src, len);
    *(dst + len) = '\0';
}

void irc_process(const char *line)
{
    regmatch_t pmatch[9];

    char prefix[512];
    char command[512];
    char params[512];
    char trail[512];

    if (regexec(&preg, line, 9, pmatch, 0) != REG_NOMATCH)
    {
        irc_memcpy(prefix, line + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
        irc_memcpy(command, line + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
        irc_memcpy(command, line + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
        irc_memcpy(params, line + pmatch[5].rm_so, pmatch[5].rm_eo - pmatch[5].rm_so);
        irc_memcpy(trail, line + pmatch[8].rm_so, pmatch[8].rm_eo - pmatch[8].rm_so);

        log_printf("DEBUG <%s> <%s> <%s> <%s>\n", prefix, command, params, trail);
    }
}

int irc_init(CTX ctx)
{
    irc_ctx = ctx;

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
    server_unregister_cb(irc_process);
    regfree(&preg);
}

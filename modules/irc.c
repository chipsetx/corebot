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

CTX irc_ctx = NULL;

void server_register_cb(void (*cb)(const char *));
void server_unregister_cb(void (*cb)(const char *));

void irc_process(const char *line)
{
    log_printf("%s\n", line);
}

int irc_init(CTX ctx)
{
    irc_ctx = ctx;

    if (bot_require("server", 1) < 1)
    {
        printf("error: server module required\n");
        return -1;
    }

    server_register_cb(irc_process);

    return 1;
}

void irc_free()
{
    server_unregister_cb(irc_process);
}

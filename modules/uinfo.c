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

int uinfo_registered = 0;

void uinfo_irc(const char *prefix, const char *command, const char *params, const char *trail)
{
    /* XXX: handle registration fail (nick reserved) */
    if (!uinfo_registered && strcmp(command, "NOTICE") == 0 && strcmp(params, "*") == 0)
    {
        irc_printf("NICK corebot\r\n");
        irc_printf("USER corebot b c :infobot\r\n");
        uinfo_registered = 1;
    }
}

int uinfo_init(CTX ctx)
{
    if (bot_require("irc", 1) < 1)
    {
        log_printf("irc module required\n");
        return -1;
    }

    irc_register_cb(uinfo_irc);

    return 1;
}

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

#include <regex.h>

static TAILQ_HEAD(_config_head, config_entry) config_head;

struct config_entry
{
    char *section;
    char *key;
    char *value;
    TAILQ_ENTRY(config_entry) config_entries;
};

const char *config_get(const char *key)
{
    struct config_entry *e;
    char *section = NULL;
    CTX ctx;

    ctx = bot_get_ctx();
    if (ctx)
    {
        section = ctx->name;
    }

    TAILQ_FOREACH(e, &config_head, config_entries)
    {
        if (((section == NULL && e->section == NULL) || (section && e->section && strcmp(section, e->section) == 0)) && strcmp(key, e->key) == 0)
        {
            return e->value;
        }
    }

    return NULL;
}

void config_regex_load(char **dst, const char *src, regmatch_t *m)
{
    int len;

    len = m->rm_eo - m->rm_so;
    *dst = malloc(len + 1);
    memcpy(*dst, src + m->rm_so, len);
    *(*dst + len) = '\0';
}

void config_load(const char *file)
{
    regex_t preg_section;
    regex_t preg_pair;
    regex_t preg_pair_q;
    FILE *fh;
    char buf[512];
    regmatch_t pmatch[3];
    struct config_entry *e;
    char *section;

    TAILQ_INIT(&config_head);

    if (regcomp(&preg_section, "^\\[([a-z]+)\\]", REG_EXTENDED) != 0)
    {
        log_printf("config: error compiling section regexp\n");
    }

    if (regcomp(&preg_pair, "^[[:space:]]*([a-z_]+)[[:space:]]*=[[:space:]]*([^;[:space:]]+)", REG_EXTENDED) != 0)
    {
        log_printf("config: error compiling pair regexp\n");
    }

    if (regcomp(&preg_pair_q, "^[[:space:]]*([a-z_]+)[[:space:]]*=[[:space:]]*\"([^\"]+)\"", REG_EXTENDED) != 0)
    {
        log_printf("config: error compiling pair regexp\n");
    }

    fh = fopen(file, "r");
    if (!fh)
    {
        log_printf("config: error opening file for reading\n");
        return;
    }

    section = NULL;
    while ( fgets(buf, 512, fh) )
    {
        buf[strcspn(buf, "\r\n")] = '\0';

        e = NULL;

        if (regexec(&preg_pair_q, buf, 3, pmatch, 0) != REG_NOMATCH)
        {
            e = calloc(sizeof(struct config_entry), 1);
            if (section)
            {
                e->section = strdup(section);
            }
            config_regex_load(&e->key, buf, &pmatch[1]);
            config_regex_load(&e->value, buf, &pmatch[2]);
        }
        else if (regexec(&preg_pair, buf, 3, pmatch, 0) != REG_NOMATCH)
        {
            e = calloc(sizeof(struct config_entry), 1);
            if (section)
            {
                e->section = strdup(section);
            }
            config_regex_load(&e->key, buf, &pmatch[1]);
            config_regex_load(&e->value, buf, &pmatch[2]);
        }
        else if (regexec(&preg_section, buf, 3, pmatch, 0) != REG_NOMATCH)
        {
            if (section)
            {
                free(section);
            }
            config_regex_load(&section, buf, &pmatch[1]);
        }

        if (e)
        {
            TAILQ_INSERT_TAIL(&config_head, e, config_entries);
        }
    }

    if (section)
    {
        free(section);
    }

    fclose(fh);

    regfree(&preg_section);
    regfree(&preg_pair);
    regfree(&preg_pair_q);
}

void config_free()
{
    struct config_entry *e;

    while ( (e = TAILQ_LAST(&config_head, _config_head)) )
    {
        if (e->section != NULL)
        {
            free(e->section);
        }

        if (e->key != NULL)
        {
            free(e->key);
        }

        if (e->value != NULL)
        {
            free(e->value);
        }

        TAILQ_REMOVE(&config_head, e, config_entries);
        free(e);
    }
}

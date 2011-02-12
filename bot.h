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

#include <stdio.h>

#include "log.h"

struct bot_module;
typedef struct bot_module * CTX;

struct bot_module
{
    char *name;                 // name of the module, file and namespace
    void *dl;                   // dlopened module
    int version;                // version number
    int sock;                   // optionally registered socket

                                // these are called (if exported)...
    int (*init)(CTX);           //  on module load (returns version)
    void (*read)(int sock);     //  when a registered socket is ready to read data
    void (*timer)(void);        //  approximately once a second
    void (*free)(void);         //  on module unload
};

extern struct bot_module modules[];

extern struct bot_module *_bot_context;
#define bot_ctx(a) _bot_context = a
#define bot_get_ctx() _bot_context

#define EACH_MODULE(a, b) \
    for (struct bot_module *b = a; b->name; b++)

#define EACH_LOADED_MODULE(a, b) \
    for (struct bot_module *b = a; b->dl; b++)

int bot_module_load(struct bot_module *mod);
void bot_module_free(struct bot_module *mod);

void bot_register_fd(int sock);
void bot_unregister_fd();
int bot_require(const char *name, int version);

// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdio.h>
#include "info.h"

int main(void)
{
    // R_InitSpriteDefs scans sprnames until NULL. Its declaration must reserve
    // one entry after all NUMSPRITES names. info.c includes this same header,
    // so a mismatched production definition is a compiler error.
    size_t entries = sizeof(sprnames) / sizeof(sprnames[0]);
    fprintf(stderr, "sprite table entries=%zu; names plus terminator=%d\n",
        entries, NUMSPRITES + 1);
    assert(entries == NUMSPRITES + 1);
    return 0;
}

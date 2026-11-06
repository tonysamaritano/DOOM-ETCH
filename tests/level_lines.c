// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include "p_local.h"
#include "r_state.h"
#include "z_zone.h"
void P_GroupLines(void);
static void *table;
void I_Error(char *format, ...)
{
    va_list args; va_start(args, format); vfprintf(stderr, format, args);
    va_end(args); abort();
}
void *Z_Malloc(int bytes, int tag, void *owner)
{
    assert(tag == PU_LEVEL && owner == NULL);
    fprintf(stderr, "sector line table requests %d bytes, needs %zu\n",
        bytes, 3 * sizeof(line_t *));
    assert(bytes == 3 * sizeof(line_t *));
    table = calloc(1, (size_t)bytes); assert(table); return table;
}
int main(void)
{
    sector_t rooms[2] = {0};
    line_t walls[2] = {0};
    vertex_t points[2] = {{0, 0}, {128 * FRACUNIT, 128 * FRACUNIT}};
    side_t side = {0}; seg_t segment = {0}; subsector_t area = {0};
    sectors = rooms; numsectors = 2; lines = walls; numlines = 2;
    side.sector = &rooms[1]; segment.sidedef = &side;
    segs = &segment; subsectors = &area; numsubsectors = 1;
    bmapwidth = bmapheight = 4; bmaporgx = bmaporgy = 0;

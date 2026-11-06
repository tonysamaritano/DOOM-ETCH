#include "p_local.h"
#include <assert.h>
#include <stdlib.h>

void P_RunThinkers(void);
typedef struct { thinker_t link; int number; thinker_t *remove; } Actor;
static int order[16], count, frees;
void Z_Free(void *memory) { ++frees; free(memory); }
static void tick(void *memory)
{
    Actor *actor = memory;
    order[count++] = actor->number;
    if (actor->remove) P_RemoveThinker(actor->remove);
}
static Actor *spawn(int number)
{
    Actor *actor = calloc(1, sizeof(*actor));
    assert(actor);
    actor->number = number;
    actor->link.function.acp1 = tick;
    P_AddThinker(&actor->link);
    return actor;
}
static void checkLinks(void)
{
    thinker_t *previous = &thinkercap;
    for (thinker_t *p = thinkercap.next; p != &thinkercap; p = p->next) {
        assert(p->prev == previous);
        previous = p;
    }
    assert(thinkercap.prev == previous);
}
int main(void)
{
    P_InitThinkers();

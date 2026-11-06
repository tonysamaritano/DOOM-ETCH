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
    P_RunThinkers();
    assert(count == 0);
    Actor *first = spawn(10), *middle = spawn(20), *last = spawn(30);
    P_RemoveThinker(&first->link);
    P_RemoveThinker(&middle->link);
    P_RunThinkers();
    assert(frees == 2 && count == 1 && order[0] == 30);
    checkLinks();
    last->remove = &last->link;
    count = 0;
    P_RunThinkers();
    assert(count == 1 && frees == 2); /* Self-removal is deferred. */
    P_RunThinkers();
    assert(frees == 3 && thinkercap.next == &thinkercap);
    first = spawn(40); middle = spawn(50); last = spawn(60);
    first->remove = &middle->link;
    count = 0;
    P_RunThinkers();
    assert(frees == 4 && count == 2 && order[0] == 40 && order[1] == 60);
    checkLinks();
    first->remove = NULL;
    P_RemoveThinker(&first->link);
    P_RemoveThinker(&last->link);
    P_RunThinkers();
    assert(frees == 6);
    checkLinks();
    return 0;
}

#include "doomstat.h"
#include "i_net.h"
#include "i_system.h"
#include "m_argv.h"

static doomcom_t local;

void I_InitNetwork(void)
{
    if (M_CheckParm("-net"))
        I_Error("The macOS port supports single player only");

    doomcom = &local;
    doomcom->id = DOOMCOM_ID;
    doomcom->ticdup = 1;
    doomcom->numnodes = 1;
    doomcom->numplayers = 1;
    doomcom->consoleplayer = 0;
    netgame = false;
}

void I_NetCmd(void)
{
    I_Error("Unexpected network command in a local game");
}

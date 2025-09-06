#include "global.h"
#include "main.h"
#include "m4a.h"
#include <stdlib.h>

#if PLATFORM_PC
int main(void)
{
    atexit(m4aSoundShutdown);
    gPCVram = malloc(VRAM_SIZE);
    gPCPltt = malloc(PLTT_SIZE);
    gPCOam = malloc(OAM_SIZE);

    AgbMain();
    return 0;
}
#endif


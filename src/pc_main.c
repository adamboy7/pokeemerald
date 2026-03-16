#include "global.h"
#include "main.h"
#include "m4a.h"
#include <stdio.h>
#include <stdlib.h>

#if PLATFORM_PC
int main(void)
{
    atexit(m4aSoundShutdown);
    gPCVram = malloc(VRAM_SIZE);
    gPCPltt = malloc(PLTT_SIZE);
    gPCOam = malloc(OAM_SIZE);
    if (!gPCVram || !gPCPltt || !gPCOam)
    {
        fprintf(stderr, "Failed to allocate video memory\n");
        return 1;
    }

    AgbMain();
    return 0;
}
#endif


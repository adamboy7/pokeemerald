#include "global.h"
#include "main.h"
#include <stdlib.h>

#ifdef PC
int main(void)
{
    gPCVram = malloc(VRAM_SIZE);
    gPCPltt = malloc(PLTT_SIZE);
    gPCOam = malloc(OAM_SIZE);

    AgbMain();
    return 0;
}
#endif


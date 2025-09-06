#include "gba/gba.h"
#include <assert.h>
#include <stdio.h>

static int sSoundInitCalls;
void m4aSoundInit(void)
{
    sSoundInitCalls++;
}

int main(void)
{
    RegisterRamReset(RESET_SOUND_REGS);
    assert(sSoundInitCalls == 1);
    printf("PC audio reset calls m4aSoundInit\n");
    return 0;
}

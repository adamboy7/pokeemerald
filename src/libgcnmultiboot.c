#include "libgcnmultiboot.h"
#include "gba/gba.h"

#if PLATFORM_PC

// Desktop stubs for GameCube multiboot routines.
// TODO: Provide high-level implementation of GameCube link transfers.

u32 GameCubeMultiBoot_Hash(u32 value, u32 key)
{
    (void)value;
    (void)key;
    // TODO: Implement hashing algorithm used during multiboot.
    return 0;
}

void GameCubeMultiBoot_Main(struct GcmbStruct *pStruct)
{
    (void)pStruct;
    // TODO: Drive multiboot transfer state machine.
}

void GameCubeMultiBoot_ExecuteProgram(struct GcmbStruct *pStruct)
{
    (void)pStruct;
    // TODO: Jump to received multiboot image.
}

void GameCubeMultiBoot_Init(struct GcmbStruct *pStruct)
{
    (void)pStruct;
    // TODO: Initialise multiboot state before transfer.
}

void GameCubeMultiBoot_HandleSerialInterrupt(struct GcmbStruct *pStruct)
{
    (void)pStruct;
    // TODO: Handle JOY Bus serial interrupts.
}

void GameCubeMultiBoot_Quit(void)
{
    // TODO: Clean up multiboot session and restore state.
}

#endif // PLATFORM_PC

#include "global.h"
#include "main.h"

#if PLATFORM_PC
u32 IntrMain[0x200] = {0};
const u8 RomHeaderGameCode[GAME_CODE_LENGTH] = {0};
const u8 RomHeaderSoftwareVersion = 0;
#endif

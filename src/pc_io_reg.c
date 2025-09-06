#ifdef PLATFORM_PC
#include "gba/types.h"

// Simple emulated I/O register space for desktop builds.
// The size covers the range of GBA I/O registers used by the engine.
u8 gIoRegisters[0x400];
#endif

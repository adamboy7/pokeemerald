#ifndef GUARD_PLATFORM_IO_H
#define GUARD_PLATFORM_IO_H

#include "gba/types.h"
#include "gba/io_reg.h"

#ifdef GBA
static inline u16 PlatformReadReg(u16 regOffset)
{
    return *(vu16 *)(REG_BASE + regOffset);
}

static inline void PlatformWriteReg(u16 regOffset, u16 value)
{
    *(vu16 *)(REG_BASE + regOffset) = value;
}
#else
u16 PlatformReadReg(u16 regOffset);
void PlatformWriteReg(u16 regOffset, u16 value);
#endif // GBA

#endif // GUARD_PLATFORM_IO_H

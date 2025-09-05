#include "platform/io.h"

#ifndef GBA
#define IO_REG_COUNT 0x400
static u16 sIoRegs[IO_REG_COUNT / 2];

u16 PlatformReadReg(u16 regOffset)
{
    return sIoRegs[regOffset / 2];
}

void PlatformWriteReg(u16 regOffset, u16 value)
{
    sIoRegs[regOffset / 2] = value;
}
#endif // GBA

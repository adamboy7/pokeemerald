#include "platform/io.h"

#if PLATFORM_PC
extern u8 gIoRegisters[0x400];

u16 PlatformReadReg(u16 regOffset)
{
    return *(u16 *)(gIoRegisters + regOffset);
}

void PlatformWriteReg(u16 regOffset, u16 value)
{
    *(u16 *)(gIoRegisters + regOffset) = value;
}
#endif // PLATFORM_PC

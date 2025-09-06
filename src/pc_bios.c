#include "gba/gba.h"

#ifdef PLATFORM_PC
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "m4a.h"

// Simple stubs for GBA BIOS calls when running on a desktop PC.
// These provide minimal behaviour sufficient for bringing up the engine
// without relying on actual GBA hardware or the BIOS.

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    // On a PC build, a soft reset can be approximated by terminating
    // the process. The launcher or invoking script is expected to
    // restart the program if desired.
    exit(0);
}

void RegisterRamReset(u32 resetFlags)
{
    // Only sound registers are handled explicitly. Other flags are
    // ignored as they have no direct analogue on PC builds.
    if (resetFlags & RESET_SOUND_REGS)
        m4aSoundInit();
}

void IntrWait(u32 flags, u32 unused)
{
    (void)flags;
    (void)unused;
    // Simply wait for roughly one display frame. This mimics the
    // blocking behaviour of the BIOS call without relying on actual
    // interrupt hardware.
    usleep(1000000 / 60);
}

void VBlankIntrWait(void)
{
    IntrWait(0, 0);
}

u16 Sqrt(u32 num)
{
    return (u16)sqrt((double)num);
}

u16 ArcTan(s16 x)
{
    // GBA ArcTan takes a fixed-point value where 256 represents 1.0.
    double angle = atan((double)x / 256.0);
    return (u16)(angle * 0x8000 / M_PI);
}

u16 ArcTan2(s16 x, s16 y)
{
    double angle = atan2((double)y, (double)x);
    if (angle < 0)
        angle += 2 * M_PI;
    return (u16)(angle * 0x8000 / M_PI);
}

void CpuSet(const void *src, void *dest, u32 control)
{
    // The real BIOS call supports various modes via the control
    // parameter. For desktop builds we only need basic copying, so we
    // ignore the flags and simply perform a memmove.
    size_t size = control & 0x1FFFFF; // lower 21 bits encode length
    memmove(dest, src, size);
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    // Like CpuSet, this simplified implementation just performs a
    // regular memory move. The "fast" behaviour is irrelevant on
    // modern CPUs.
    size_t size = control & 0x1FFFFF;
    memmove(dest, src, size);
}

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    memcpy(dest, src, count * sizeof(*src));
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    u8 *d = dest;
    for (s32 i = 0; i < count; i++)
    {
        memcpy(d + i * offset, &src[i], sizeof(*src));
    }
}

static void LZ77UnComp(const u8 *src, u8 *dest)
{
    u32 header = *(const u32 *)src;
    src += 4;
    u32 remaining = header >> 8;

    while (remaining)
    {
        u8 flags = *src++;
        for (int i = 0; i < 8 && remaining; i++, flags <<= 1)
        {
            if (flags & 0x80)
            {
                u8 byte1 = *src++;
                u8 byte2 = *src++;
                int disp = ((byte1 & 0xF) << 8) | byte2;
                int count = (byte1 >> 4) + 3;
                const u8 *copySrc = dest - disp - 1;
                for (int j = 0; j < count && remaining; j++)
                {
                    *dest++ = *copySrc++;
                    remaining--;
                }
            }
            else
            {
                *dest++ = *src++;
                remaining--;
            }
        }
    }
}

static void RLUnComp(const u8 *src, u8 *dest)
{
    u32 header = *(const u32 *)src;
    src += 4;
    u32 remaining = header >> 8;

    while (remaining)
    {
        u8 info = *src++;
        if (info & 0x80)
        {
            int count = (info & 0x7F) + 3;
            u8 value = *src++;
            for (int i = 0; i < count && remaining; i++)
            {
                *dest++ = value;
                remaining--;
            }
        }
        else
        {
            int count = (info & 0x7F) + 1;
            for (int i = 0; i < count && remaining; i++)
            {
                *dest++ = *src++;
                remaining--;
            }
        }
    }
}

void LZ77UnCompWram(const u32 *src, void *dest)
{
    LZ77UnComp((const u8 *)src, dest);
}

void LZ77UnCompVram(const u32 *src, void *dest)
{
    LZ77UnComp((const u8 *)src, dest);
}

void RLUnCompWram(const u32 *src, void *dest)
{
    RLUnComp((const u8 *)src, dest);
}

void RLUnCompVram(const u32 *src, void *dest)
{
    RLUnComp((const u8 *)src, dest);
}

int MultiBoot(struct MultiBootParam *mp)
{
    (void)mp;
    return 0;
}

s32 Div(s32 num, s32 denom)
{
    if (denom == 0)
        return 0;
    return num / denom;
}

s32 Mod(s32 num, s32 denom)
{
    if (denom == 0)
        return 0;
    return num % denom;
}

#else
#error "PLATFORM_PC must be defined"
#endif // PLATFORM_PC


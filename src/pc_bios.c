#include "global.h"

#ifdef PLATFORM_PC
#include <math.h>
#include <string.h>
#include <unistd.h>

// Simple stubs for GBA BIOS calls when running on a desktop PC.
// These provide minimal behaviour sufficient for bringing up the engine
// without relying on actual GBA hardware or the BIOS.

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
}

void RegisterRamReset(u32 resetFlags)
{
    (void)resetFlags;
}

void VBlankIntrWait(void)
{
    // Sleep roughly for one frame at 60Hz to simulate VBlank.
    usleep(1000000 / 60);
}

u16 Sqrt(u32 num)
{
    return (u16)sqrt((double)num);
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
    size_t size = control & 0x1FFFFF; // lower 21 bits encode length
    if (control & CPU_SET_SRC_FIXED)
    {
        if (control & CPU_SET_32BIT)
        {
            u32 value = *(const u32 *)src;
            u32 *d = dest;
            for (size_t i = 0; i < size / 4; i++)
                d[i] = value;
        }
        else
        {
            u16 value = *(const u16 *)src;
            u16 *d = dest;
            for (size_t i = 0; i < size / 2; i++)
                d[i] = value;
        }
    }
    else
    {
        memcpy(dest, src, size);
    }
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    size_t size = control & 0x1FFFFF;
    if (control & CPU_FAST_SET_SRC_FIXED)
    {
        u32 value = *(const u32 *)src;
        u32 *d = dest;
        for (size_t i = 0; i < size / 4; i++)
            d[i] = value;
    }
    else
    {
        memcpy(dest, src, size);
    }
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

static void LZ77Stub(const u32 *src, void *dest)
{
    // Very small placeholder: the size is stored in the top 24 bits after the first byte.
    // This does not actually decompress but allows graphics to be treated as raw.
    u32 header = *src++;
    u32 size = header >> 8;
    memcpy(dest, src, size);
}

void LZ77UnCompWram(const u32 *src, void *dest)
{
    LZ77Stub(src, dest);
}

void LZ77UnCompVram(const u32 *src, void *dest)
{
    LZ77Stub(src, dest);
}

void RLUnCompWram(const u32 *src, void *dest)
{
    LZ77Stub(src, dest);
}

void RLUnCompVram(const u32 *src, void *dest)
{
    LZ77Stub(src, dest);
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

#endif // PLATFORM_PC


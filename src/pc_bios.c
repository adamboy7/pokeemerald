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

#endif // PLATFORM_PC


#include "gba/gba.h"

#ifdef PLATFORM_PC
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "m4a.h"

// Desktop implementations of a subset of the GBA BIOS calls. These aim to
// emulate the behaviour of the real BIOS closely enough for engine bring-up
// and unit testing on a PC.

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    // On a PC build, a soft reset can be approximated by terminating
    // the process. The launcher or invoking script is expected to
    // restart the program if desired.
    exit(0);
}

void SoftResetRom(void)
{
    SoftReset(RESET_ALL);
}

void SoftResetExram(void)
{
    SoftReset(RESET_ALL);
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
    u32 count = control & 0x001FFFFF;
    bool32 use32 = control & CPU_SET_32BIT;
    bool32 fixed = control & CPU_SET_SRC_FIXED;

    if (use32)
    {
        const u32 *s = src;
        u32 *d = dest;
        u32 value = *s;
        for (u32 i = 0; i < count; i++)
            d[i] = fixed ? value : s[i];
    }
    else
    {
        const u16 *s = src;
        u16 *d = dest;
        u16 value = *s;
        for (u32 i = 0; i < count; i++)
            d[i] = fixed ? value : s[i];
    }
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    u32 count = control & 0x001FFFFF;
    bool32 fixed = control & CPU_FAST_SET_SRC_FIXED;
    const u32 *s = src;
    u32 *d = dest;
    u32 value = *s;

    // CpuFastSet operates in units of 8 words.
    for (u32 i = 0; i < count * 8; i++)
        d[i] = fixed ? value : s[i];
}

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    for (s32 i = 0; i < count; i++)
    {
        double sx = (double)src[i].sx / 256.0;
        double sy = (double)src[i].sy / 256.0;
        double angle = src[i].alpha * M_PI / 32768.0;
        double cosA = cos(angle);
        double sinA = sin(angle);

        dest[i].pa = (s16)(cosA * sx * 256);
        dest[i].pb = (s16)(-sinA * sx * 256);
        dest[i].pc = (s16)(sinA * sy * 256);
        dest[i].pd = (s16)(cosA * sy * 256);

        dest[i].dx = src[i].texX - ((dest[i].pa * src[i].scrX + dest[i].pb * src[i].scrY) >> 8);
        dest[i].dy = src[i].texY - ((dest[i].pc * src[i].scrX + dest[i].pd * src[i].scrY) >> 8);
    }
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    s16 (*d)[4] = dest;
    for (s32 i = 0; i < count; i++)
    {
        double x = (double)src[i].xScale / 256.0;
        double y = (double)src[i].yScale / 256.0;
        double angle = src[i].rotation * M_PI / 32768.0;
        double cosA = cos(angle);
        double sinA = sin(angle);

        d[i * offset / sizeof(*d)][0] = (s16)(cosA * x * 256);
        d[i * offset / sizeof(*d)][1] = (s16)(-sinA * x * 256);
        d[i * offset / sizeof(*d)][2] = (s16)(sinA * y * 256);
        d[i * offset / sizeof(*d)][3] = (s16)(cosA * y * 256);
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

s32 DivArm(s32 num, s32 denom)
{
    return Div(num, denom);
}

s32 ModArm(s32 num, s32 denom)
{
    return Mod(num, denom);
}

u32 MidiKey2Freq(u8 key, u8 fractional, u8 octave)
{
    double semitone = key + fractional / 256.0 + octave * 12;
    double freq = 440.0 * pow(2.0, (semitone - 69) / 12.0);
    return (u32)freq;
}

#else
#error "PLATFORM_PC must be defined"
#endif // PLATFORM_PC


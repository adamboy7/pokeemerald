#include "gba/gba.h"

#ifndef GBA

#ifdef PLATFORM_PC
#include "m4a.h"
#endif

// NOTE: These are minimal desktop stubs for GBA BIOS syscalls.
// The actual desktop implementations live in src/pc_bios.c.

struct BitUnPackParams; // Forward declaration for BitUnPack parameters.

__attribute__((weak)) void IntrWait(u32 flags, u32 unused)
{
    (void)flags;
    (void)unused;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void RegisterRamReset(u32 resetFlags)
{
    (void)resetFlags;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void SoftResetRom(void)
{
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void SoftResetExram(void)
{
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void VBlankIntrWait(void)
{
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) u16 Sqrt(u32 num)
{
    (void)num;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) u16 ArcTan(s16 x)
{
    (void)x;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) u16 ArcTan2(s16 x, s16 y)
{
    (void)x;
    (void)y;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) s32 Div(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) s32 DivArm(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) s32 Mod(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) s32 ModArm(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) void CpuSet(const void *src, void *dest, u32 control)
{
    (void)src;
    (void)dest;
    (void)control;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void CpuFastSet(const void *src, void *dest, u32 control)
{
    (void)src;
    (void)dest;
    (void)control;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    (void)src;
    (void)dest;
    (void)count;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    (void)src;
    (void)dest;
    (void)count;
    (void)offset;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void LZ77UnCompWram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void LZ77UnCompVram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void RLUnCompWram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void RLUnCompVram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // Stub; handled by src/pc_bios.c on desktop builds.
}

__attribute__((weak)) void HuffUnComp(const u8 *src, void *dest)
{
    (void)src;
    (void)dest;
    // Not implemented on desktop builds.
}

__attribute__((weak)) void BitUnPack(const void *src, void *dest, const struct BitUnPackParams *params)
{
    (void)src;
    (void)dest;
    (void)params;
    // Not implemented on desktop builds.
}

__attribute__((weak)) void Diff8bitUnFilterWram(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // Not implemented on desktop builds.
}

__attribute__((weak)) void Diff8bitUnFilterVram(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // Not implemented on desktop builds.
}

__attribute__((weak)) void Diff16bitUnFilter(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // Not implemented on desktop builds.
}

__attribute__((weak)) int MultiBoot(struct MultiBootParam *mp)
{
    (void)mp;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

__attribute__((weak)) u32 MidiKey2Freq(u8 key, u8 fractional, u8 octave)
{
    (void)key;
    (void)fractional;
    (void)octave;
    // Stub; handled by src/pc_bios.c on desktop builds.
    return 0;
}

void SoundDriverInit(void)
{
    m4aSoundInit();
}

void SoundDriverMain(void)
{
    m4aSoundMain();
}

void SoundDriverVSync(void)
{
    m4aSoundVSync();
}

void SoundDriverVSyncOff(void)
{
    m4aSoundVSyncOff();
}

void SoundDriverVSyncOn(void)
{
    m4aSoundVSyncOn();
}

void SoundDriverMode(u32 mode)
{
    m4aSoundMode(mode);
}

void SoundBiasSet(void)
{
    // No-op on PC.
}

void SoundBiasReset(void)
{
    // No-op on PC.
}

void SoundBiasChange(void)
{
    // No-op on PC.
}

__attribute__((weak)) void MusicPlayerOpen(void)
{
    // Not implemented on desktop builds.
}

__attribute__((weak)) void MusicPlayerStart(void)
{
    // Not implemented on desktop builds.
}

__attribute__((weak)) void MusicPlayerStop(void)
{
    // Not implemented on desktop builds.
}

__attribute__((weak)) void MusicPlayerContinue(void)
{
    // Not implemented on desktop builds.
}

__attribute__((weak)) void MusicPlayerFadeOut(void)
{
    // Not implemented on desktop builds.
}

__attribute__((weak)) void SoundChannelClear(void)
{
    // Not implemented on desktop builds.
}

#endif // GBA


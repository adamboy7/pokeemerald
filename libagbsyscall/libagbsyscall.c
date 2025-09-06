#include "gba/gba.h"

#ifndef GBA

#ifdef PLATFORM_PC
#include "m4a.h"
#endif

// NOTE: These are minimal desktop stubs for GBA BIOS syscalls.
// TODO: Provide high-level implementations where appropriate.

struct BitUnPackParams; // Forward declaration for BitUnPack parameters.

__attribute__((weak)) void IntrWait(u32 flags, u32 unused)
{
    (void)flags;
    (void)unused;
    // TODO: Wait for interrupts or VBlank.
}

__attribute__((weak)) void RegisterRamReset(u32 resetFlags)
{
    (void)resetFlags;
    // TODO: Reset selected RAM regions.
}

__attribute__((weak)) void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    // TODO: Perform a soft reset.
}

__attribute__((weak)) void SoftResetRom(void)
{
    // TODO: Reset using ROM entry point.
}

__attribute__((weak)) void SoftResetExram(void)
{
    // TODO: Reset external RAM.
}

__attribute__((weak)) void VBlankIntrWait(void)
{
    // TODO: Block until VBlank on desktop build.
}

__attribute__((weak)) u16 Sqrt(u32 num)
{
    (void)num;
    // TODO: Implement square root.
    return 0;
}

__attribute__((weak)) u16 ArcTan(s16 x)
{
    (void)x;
    // TODO: Implement arctangent lookup.
    return 0;
}

__attribute__((weak)) u16 ArcTan2(s16 x, s16 y)
{
    (void)x;
    (void)y;
    // TODO: Implement two-argument arctangent.
    return 0;
}

__attribute__((weak)) s32 Div(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // TODO: Provide signed division.
    return 0;
}

__attribute__((weak)) s32 DivArm(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // TODO: Provide signed division.
    return 0;
}

__attribute__((weak)) s32 Mod(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // TODO: Provide modulo operation.
    return 0;
}

__attribute__((weak)) s32 ModArm(s32 num, s32 denom)
{
    (void)num;
    (void)denom;
    // TODO: Provide modulo operation.
    return 0;
}

__attribute__((weak)) void CpuSet(const void *src, void *dest, u32 control)
{
    (void)src;
    (void)dest;
    (void)control;
    // TODO: Implement CpuSet memory copy.
}

__attribute__((weak)) void CpuFastSet(const void *src, void *dest, u32 control)
{
    (void)src;
    (void)dest;
    (void)control;
    // TODO: Implement fast memory copy.
}

__attribute__((weak)) void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    (void)src;
    (void)dest;
    (void)count;
    // TODO: Implement BG affine transformation setup.
}

__attribute__((weak)) void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    (void)src;
    (void)dest;
    (void)count;
    (void)offset;
    // TODO: Implement OBJ affine transformation setup.
}

__attribute__((weak)) void LZ77UnCompWram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement LZ77 decompression to WRAM.
}

__attribute__((weak)) void LZ77UnCompVram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement LZ77 decompression to VRAM.
}

__attribute__((weak)) void RLUnCompWram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement RLE decompression to WRAM.
}

__attribute__((weak)) void RLUnCompVram(const u32 *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement RLE decompression to VRAM.
}

__attribute__((weak)) void HuffUnComp(const u8 *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement Huffman decompression.
}

__attribute__((weak)) void BitUnPack(const void *src, void *dest, const struct BitUnPackParams *params)
{
    (void)src;
    (void)dest;
    (void)params;
    // TODO: Implement bit unpacking.
}

__attribute__((weak)) void Diff8bitUnFilterWram(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement 8-bit differential unfilter for WRAM.
}

__attribute__((weak)) void Diff8bitUnFilterVram(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement 8-bit differential unfilter for VRAM.
}

__attribute__((weak)) void Diff16bitUnFilter(const void *src, void *dest)
{
    (void)src;
    (void)dest;
    // TODO: Implement 16-bit differential unfilter.
}

__attribute__((weak)) int MultiBoot(struct MultiBootParam *mp)
{
    (void)mp;
    // TODO: Implement multi-boot communication.
    return 0;
}

__attribute__((weak)) u32 MidiKey2Freq(u8 key, u8 fractional, u8 octave)
{
    (void)key;
    (void)fractional;
    (void)octave;
    // TODO: Convert MIDI key to frequency.
    return 0;
}

void SoundDriverInit(void)
{
#ifdef PLATFORM_PC
    m4aSoundInit();
#endif
}

void SoundDriverMain(void)
{
#ifdef PLATFORM_PC
    m4aSoundMain();
#endif
}

void SoundDriverVSync(void)
{
#ifdef PLATFORM_PC
    m4aSoundVSync();
#endif
}

void SoundDriverVSyncOff(void)
{
#ifdef PLATFORM_PC
    m4aSoundVSyncOff();
#endif
}

void SoundDriverVSyncOn(void)
{
#ifdef PLATFORM_PC
    m4aSoundVSyncOn();
#endif
}

void SoundDriverMode(u32 mode)
{
#ifdef PLATFORM_PC
    m4aSoundMode(mode);
#else
    (void)mode;
#endif
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
    // TODO: Open music player.
}

__attribute__((weak)) void MusicPlayerStart(void)
{
    // TODO: Start playback on music player.
}

__attribute__((weak)) void MusicPlayerStop(void)
{
    // TODO: Stop playback on music player.
}

__attribute__((weak)) void MusicPlayerContinue(void)
{
    // TODO: Resume playback on music player.
}

__attribute__((weak)) void MusicPlayerFadeOut(void)
{
    // TODO: Fade out music player.
}

__attribute__((weak)) void SoundChannelClear(void)
{
    // TODO: Clear sound channels.
}

#endif // GBA


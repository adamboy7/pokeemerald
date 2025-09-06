#include "gba/gba.h"

#ifndef GBA

#ifdef PLATFORM_PC
#include "m4a.h"
#endif

// Desktop wrappers for BIOS sound driver calls. Other BIOS functions
// are provided by src/pc_bios.c on non-GBA builds.

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


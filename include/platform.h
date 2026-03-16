#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H

// Define PLATFORM_PC for desktop builds. When undefined the build targets the GBA.
// This allows code to provide alternate implementations without breaking
// the original hardware build.
#if PLATFORM_PC
#define PLATFORM_GBA 0
// Record the full-width host pointer for each DMA channel so that HandleDmas
// can avoid 64-bit→32-bit truncation on 64-bit targets.
// The dmaNum argument must be an integer literal (0-3) to select the array slot.
#include <stdint.h>
extern uintptr_t gPCDmaSrc[4];
extern uintptr_t gPCDmaDst[4];
#define PC_DMA_RECORD(dmaNum, src, dest) \
    do { gPCDmaSrc[dmaNum] = (uintptr_t)(src); gPCDmaDst[dmaNum] = (uintptr_t)(dest); } while (0)
#else
#define PLATFORM_GBA 1
#define PC_DMA_RECORD(dmaNum, src, dest) do {} while (0)
#endif

#endif // GUARD_PLATFORM_H

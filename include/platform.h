#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H

// Define PLATFORM_PC for desktop builds. When undefined the build targets the GBA.
// This allows code to provide alternate implementations without breaking
// the original hardware build.
#ifdef PLATFORM_PC
#define PLATFORM_GBA 0
#else
#define PLATFORM_GBA 1
#endif

#endif // GUARD_PLATFORM_H

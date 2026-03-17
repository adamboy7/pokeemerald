# PC Port

This branch adds a native desktop build of pokeemerald using SDL2. It runs the original game logic unchanged, with GBA hardware replaced by software emulation.

**Build:** `make pc` → produces `build/pc/pokeemerald` (or `.exe` on Windows)
**Requires:** SDL2, pkg-config (or set `SDL_CFLAGS`/`SDL_LIBS` manually), clang or gcc

---

## New Files

### Platform Abstraction

| File | What it does | Why it exists |
|------|-------------|---------------|
| [include/platform.h](include/platform.h) | Defines `PLATFORM_PC`/`PLATFORM_GBA` compile-time flags and `PC_DMA_RECORD` macro | Central switch for all conditional compilation; `PC_DMA_RECORD` saves full 64-bit DMA pointers before they're stored in 32-bit task data slots |
| [include/platform/io.h](include/platform/io.h) | `PlatformReadReg`/`PlatformWriteReg` — routes register access to hardware on GBA or the emulated array on PC | Single abstraction point so the rest of the codebase needs no per-site `#ifdef` for register reads/writes |
| [src/platform/io_pc.c](src/platform/io_pc.c) | Full SDL2 platform layer: software renderer (all GBA BG/sprite/window/blend modes), keyboard+gamepad input, DMA processing, timer emulation, interrupt dispatch | Replaces the entire GBA PPU, joypad, DMA controller, and timer hardware |

### Desktop Entry Point

| File | What it does | Why it exists |
|------|-------------|---------------|
| [src/pc_main.c](src/pc_main.c) | `main()` — heap-allocates VRAM, palette, and OAM buffers then calls `AgbMain()` | GBA provides these at fixed hardware addresses; on PC they must be allocated before the engine starts |

### PC Hardware Implementations

| File | What it does | Why it exists |
|------|-------------|---------------|
| [src/pc_audio.c](src/pc_audio.c) | SDL2 audio callback, DirectSound channel mixer, PCM and tone playback | Replaces the GBA's ARM-driven audio DMA and mixing hardware so the m4a engine can produce sound on a PC |
| [src/pc_bios.c](src/pc_bios.c) | Pure-C reimplementations of GBA BIOS syscalls: math (Sqrt, ArcTan), memory ops (CpuSet, CpuFastSet), decompression (LZ77, RLE, Huffman, BitUnPack, Diff filter), affine matrix setup | The GBA BIOS is mask ROM; PC has no BIOS, so every syscall the game uses must be reimplemented |
| [src/pc_flash.c](src/pc_flash.c) | File-backed save flash emulation: read/write/erase backed by an in-memory buffer that syncs to disk | Replaces the physical Panasonic/Sanyo/SST flash chip; saves persist to a `.sav` file |
| [src/pc_io_reg.c](src/pc_io_reg.c) | Defines `gIoRegisters[0x400]` shadow array for non-SDL builds | Backing store for `PlatformReadReg`/`PlatformWriteReg` when the SDL renderer is not active |
| [src/pc_rtc.c](src/pc_rtc.c) | Real-time clock using standard C `time.h`, with BCD encoding/decoding | GBA uses a Seiko S-3511A RTC over SPI; PC reads the host system clock via `localtime()` instead |
| [src/pc_m4a_stub.c](src/pc_m4a_stub.c) | Full C reimplementation of the m4a/MP2000 music engine: sequencer, note/envelope/LFO handlers, PCM mixer, Pokemon cry system | The original `m4a.c`/`m4a_1.s` is ARM Thumb assembly that cannot be assembled for x86; this C version provides equivalent behavior |
| [src/pc_multiboot.c](src/pc_multiboot.c) | No-op multiboot stubs that always return success | Multiboot sends a ROM to a GBA over a link cable — there is no equivalent on PC |
| [src/pc_link_rfu_stub.c](src/pc_link_rfu_stub.c) | Stubs for all ~100 RFU wireless adapter functions | The RF Unit is a physical GBA accessory; its entire serial/interrupt-driven protocol doesn't exist on PC |
| [src/pc_stub.c](src/pc_stub.c) | Provides `IntrMain_Buffer` and ROM header fields | On GBA these come from the linker script and ROM header binary; the PC linker has no such section |
| [src/libgcnmultiboot.c](src/libgcnmultiboot.c) | No-op GameCube multiboot stubs (TODO) | GameCube↔GBA linking is hardware-only |

### Tests

| File | What it does |
|------|-------------|
| [tests/pc_bios_tests.c](tests/pc_bios_tests.c) | Unit tests verifying BIOS reimplementations match GBA BIOS output |
| [tests/pc_flash_tests.c](tests/pc_flash_tests.c) | Unit tests for flash read/write/erase round-trips |
| [tests/pc_audio_tests.c](tests/pc_audio_tests.c) | Scaffolding for future audio accuracy tests |
| [tests/pc_rtc_tests.c](tests/pc_rtc_tests.c) | Scaffolding for future RTC tests |

### Assets

| File | What it does | Why it exists |
|------|-------------|---------------|
| [sound/voicegroups/pc_voicegroup_offsets.inc](sound/voicegroups/pc_voicegroup_offsets.inc) | Precomputed voice group offset table for the PC audio engine | The GBA build derives these offsets from link-time memory layout; the PC build needs them as explicit constants |

---

## Modified Files

### GBA Sources

| File | What changed | Why |
|------|-------------|-----|
| [src/main.c](src/main.c) | Wrap `REG_WAITCNT` write, `InitRFU()`, and soft-reset RFU shutdown in `PLATFORM_PC`/`PLATFORM_GBA` guards; add `gPCVram`/`gPCPltt`/`gPCOam` extern definitions | Prevents writes to non-existent hardware registers and calls to missing RFU code on PC |
| [src/gpu_regs.c](src/gpu_regs.c) | Replace direct `REG_*` writes with `PlatformWriteReg`; mask `DISPSTAT` interrupt bits on PC | Routes GPU register writes through the SDL renderer; `DISPSTAT` interrupt state is managed by the platform layer on PC, not by the GPU register manager |
| [src/dma3_manager.c](src/dma3_manager.c) | Use `PlatformWriteReg` for DMA control register writes | DMA3 VRAM copies must be intercepted by `io_pc.c` to update the software renderer |
| [src/palette.c](src/palette.c) | Use `PlatformWriteReg` for `BLDCNT`/`BLDALPHA`/`BLDY` writes | Blend registers must be visible to the PC renderer's color-effects pass |
| [src/m4a.c](src/m4a.c) | PC initialization hooks | Bridges the original m4a init sequence to the `pc_m4a_stub` engine at startup |
| [src/m4a_tables.c](src/m4a_tables.c) | Minor table adjustments for PC | Frequency and envelope tables tweaked for the C reimplementation |
| [src/agb_flash.c](src/agb_flash.c) | Guard flash chip detection/timing loops with `#if !PLATFORM_PC` | Chip identification uses hardware wait-states that don't exist on PC; `pc_flash.c` handles flash I/O instead |
| [src/agb_flash_1m.c](src/agb_flash_1m.c) | Same as above | Same reason |
| [src/agb_flash_le.c](src/agb_flash_le.c) | Same as above | Same reason |
| [src/agb_flash_mx.c](src/agb_flash_mx.c) | Same as above | Same reason |
| [src/battle_dome.c](src/battle_dome.c) | Wrap RFU calls in `PLATFORM_GBA` guard | Battle Dome wireless battles use the RF Unit, which is GBA-only |
| [src/link.c](src/link.c) | Skip RFU initialization and wireless link management on PC | Link cable and wireless hardware are absent on PC |
| [src/menu.c](src/menu.c) | Use `uintptr_t` in task data for heap pointers; fix `BgDmaFill` VRAM pointer arithmetic | 64-bit hosts have 8-byte pointers; storing them in 32-bit `tData[]` slots truncates the upper half — `uintptr_t` preserves the full address |
| [src/script.c](src/script.c) | Platform register guard around timer register access | Script engine reads timer registers for RNG seeding; on PC these go through `PlatformReadReg` |
| [src/tileset_anims.c](src/tileset_anims.c) | Platform register guards for DMA and GPU register writes | Tileset animation DMA transfers must be routed through the platform layer |
| [src/ereader_helpers.c](src/ereader_helpers.c) | Guard eReader serial hardware access | The eReader is a GBA-only peripheral |
| [src/libisagbprn.c](src/libisagbprn.c) | Redirect debug output to host stdout | `isagbprn` debug printing uses GBA serial hardware; on PC it writes to `stdout` |
| [src/apprentice.c](src/apprentice.c) | `ExecuteFuncAfterButtonPress`/`Task_ExecuteFuncAfterButtonPress`: store and reload function pointer in task `data[0]`/`data[1]` via `uintptr_t` on PC | 64-bit function pointers are 8 bytes; casting to `u32` before splitting discards the upper 32 bits and corrupts the pointer on PC |
| [src/battle_anim_mons.c](src/battle_anim_mons.c) | `LoadPointerFromVars`: add `uintptr_t` intermediate cast when reconstructing a pointer from two `s16` halves | Without `uintptr_t`, the 32-bit integer result is zero-extended to 64 bits by the implicit `void *` cast, which works only when addresses fit in 32 bits; the explicit cast makes the intent clear and correct |
| [src/battle_factory_screen.c](src/battle_factory_screen.c) | All Swap-screen function-pointer store (`tFollowUpTaskPtrHi`/`tFollowUpTaskPtrLo`) and load sites use `uintptr_t` on PC | The Swap screen queues follow-up task functions by splitting a pointer across two `u16` task-data slots; on a 64-bit host the pointer may exceed 32 bits, so `uintptr_t` is required to preserve all bits |
| [src/pokeball.c](src/pokeball.c) | `Task_PlayCryWhenReleasedFromBall`: reconstruct `struct Pokemon *` from `tCryTaskMonPtr1`/`tCryTaskMonPtr2` via `uintptr_t` on PC | Same 32-bit pointer storage pattern as other task-data pointer round-trips; `uintptr_t` prevents truncation on 64-bit hosts |
| [src/pokemon.c](src/pokemon.c) | `STORE_PTR_IN_TASK` macro: use `uintptr_t` instead of `u32` when splitting a pointer into two task-data halfwords on PC | Pointers stored via this macro (e.g. animation callbacks) are 64-bit on PC; `u32` cast truncates the upper half |
| [src/util.c](src/util.c) | `LoadWordFromTwoHalfwords`: cast high halfword as `u16` instead of `s16` before shifting on PC | `(s16)h[1] << 16` sign-extends when bit 15 is set, corrupting bits 16–31 of the result; using `u16` keeps the shift unsigned and correct on both 32- and 64-bit hosts |

### Headers

| File | What changed | Why |
|------|-------------|-----|
| [include/global.h](include/global.h) | Include `platform.h`; `T1_READ_PTR`/`T2_READ_PTR` use `uintptr_t` on PC; `__attribute__((packed))` on `SaveBlock1`/`SaveBlock2` on PC | Pointer task-data round-trips are safe on 64-bit; packed attribute prevents host-compiler padding from corrupting save data layout |
| [include/gba/defines.h](include/gba/defines.h) | `VRAM`/`PLTT`/`OAM` macros resolve to `gPCVram`/`gPCPltt`/`gPCOam` pointers on PC | Unmodified source code transparently accesses heap buffers instead of GBA memory-mapped hardware |
| [include/gba/io_reg.h](include/gba/io_reg.h) | All `REG_*` definitions route through `PlatformReadReg`/`PlatformWriteReg` on PC | One change point; every existing register access in the codebase gains PC support without per-site edits |
| [include/gba/macro.h](include/gba/macro.h) | DMA macros call `PC_DMA_RECORD`; pointer macros use `uintptr_t` | Captures full 64-bit source/destination before the 32-bit register write path discards the upper half |
| [include/gba/syscall.h](include/gba/syscall.h) | Declare `pc_bios.c` functions under `PLATFORM_PC` | The rest of the codebase calls BIOS functions through the same headers on both targets |
| [include/gba/m4a_internal.h](include/gba/m4a_internal.h) | PC audio system struct and function declarations | Shared interface between `m4a.c` and `pc_m4a_stub.c` |
| [include/m4a.h](include/m4a.h) | Declare `m4aSoundShutdown` and other PC audio API | `pc_main.c` registers `m4aSoundShutdown` via `atexit` for clean teardown |
| [include/agb_flash.h](include/agb_flash.h) | Declare PC flash API | `agb_flash.c` delegates to `pc_flash.c` on PC |
| [include/pokemon_storage_system.h](include/pokemon_storage_system.h) | Minor struct layout adjustment | Ensures consistent struct size under the host compiler |

### Build System

| File | What changed | Why |
|------|-------------|-----|
| [Makefile](Makefile) | New `pc` target; host compiler detection (`clang`/`gcc`); SDL2 via `pkg-config`; `PC_OBJS` list excluding GBA-only objects; `SED_PC_CONV` sed pipeline converting ARM assembly directives to x86 GAS syntax | Produces a native executable (`make pc`) alongside the normal GBA ROM build (`make`); ARM `.word`/`@`-comment syntax is invalid in x86 GAS |
| [sound/MPlayDef.s](sound/MPlayDef.s) | Guard ARM-specific assembly directives | The `SED_PC_CONV` pass processes this file; ARM-only directives need to be absent or guarded |
| [MPlayDef.s](MPlayDef.s) | Same | Same |
| [libagbsyscall/libagbsyscall.c](libagbsyscall/libagbsyscall.c) | Wrap all functions in `#if !PLATFORM_PC` | `libagbsyscall` uses ARM `swi` instructions that are meaningless on x86; `pc_bios.c` provides the replacements |
| [README.md](README.md) | PC build prerequisites and instructions | Guides contributors on installing SDL2 and running `make pc` |

---

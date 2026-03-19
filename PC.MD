# PC Port — Change Log

This document describes every source change made in the `PC` branch relative to upstream `master`.
All modifications follow one rule: **the GBA code path is preserved verbatim** inside `#else` / `#endif`
blocks; only `#if PLATFORM_PC` branches are added.

---

## 1. GBA-only translation units excluded on PC

**Files:** `src/agb_flash.c`, `src/agb_flash_1m.c`, `src/agb_flash_le.c`, `src/agb_flash_mx.c`,
`src/m4a.c`, `src/m4a_tables.c`

Each file gains `#include "platform.h"` at the top and its entire body is wrapped in
`#if PLATFORM_GBA … #endif`.

**Why:** Flash-write routines use GBA MMIO flash bus timings and ARM inline assembly.
The M4A sound engine uses IWRAM code sections, GBA sound registers, and ARM-specific calling
conventions. None of this compiles or links on x86-64.

---

## 2. Hardware register access abstracted

**Files:** `src/gpu_regs.c`, `src/main.c`, `src/dma3_manager.c`, `src/link.c`,
`src/battle_dome.c`

Direct register reads/writes (`REG_VCOUNT`, `REG_IME`, `*(vu32*)REG_ADDR_SIOCNT`, etc.) are
replaced in `#if PLATFORM_PC` blocks with calls to `PlatformReadReg(offset)`,
`PlatformWriteReg(offset, val)`, `READ_REG_U8(offset)`, `READ_REG_U32(offset)`, and
`WRITE_REG_U32(offset, val)` (declared in `platform/io.h`).  The original GBA register access
lives unchanged in the matching `#else` block.

**Why:** GBA hardware registers live at fixed MMIO addresses (e.g. `0x04000000`) that are not
mapped in a PC process.  The platform I/O layer redirects reads/writes to the simulated
display-state machine.

`gpu_regs.c` also gains `#if PLATFORM_PC` versions of `CopyBufferedValueToGpuReg`,
`GetGpuReg`, and `SyncRegIE` that call through the platform layer instead of the hardware macro.

---

## 3. 64-bit pointer packing (`u32` → `uintptr_t`)

**Files:** `src/task.c`, `src/apprentice.c`, `src/battle_anim_mons.c`, `src/battle_dome.c`,
`src/battle_factory_screen.c`, `src/braille_puzzles.c`, `src/event_object_movement.c`,
`src/field_door.c`, `src/field_effect.c`, `src/fldeff_cut.c`, `src/fldeff_dig.c`,
`src/fldeff_misc.c`, `src/fldeff_rocksmash.c`, `src/fldeff_strength.c`,
`src/fldeff_sweetscent.c`, `src/fldeff_teleport.c`, `src/list_menu.c`, `src/menu.c`,
`src/pokeball.c`, `src/pokemon.c`, `src/pokemon_animation.c`, `src/record_mixing.c`,
`src/shop.c`, `src/slot_machine.c`, `src/trainer_see.c`

The GBA packs a 32-bit pointer into two consecutive `s16` task-data fields using:

```c
// GBA store:
data[n]   = (u32)ptr;         // low 16 bits
data[n+1] = (u32)ptr >> 16;   // high 16 bits

// GBA load:
ptr = (void *)((u16)data[n] | (data[n+1] << 16));
```

On PC (64-bit), `u32` silently truncates the upper 32 bits of a pointer.  Every store and load
is replaced under `#if PLATFORM_PC` with `uintptr_t`:

```c
// PC store:
data[n]   = (uintptr_t)ptr;
data[n+1] = (uintptr_t)ptr >> 16;

// PC load (correct form — (uintptr_t) applied before shift to prevent sign-extension):
ptr = (void *)((uintptr_t)(u16)data[n] | ((uintptr_t)(u16)data[n+1] << 16));
```

**Known limitation:** two `s16` fields = 32 bits of storage.  Only the lower 32 bits of the
pointer are retained.  This is safe on typical 64-bit Windows/Linux builds where code and heap
reside below the 4 GB mark.

---

## 4. DMA replaced with `memcpy` for palette transfers

**File:** `src/palette.c`

`TransferPlttBuffer`: `DmaCopy16(3, src, dest, PLTT_SIZE)` → `memcpy(dest, src, PLTT_SIZE)`.
`ReadPlttIntoBuffers`: the copy loop → two `memcpy` calls.
`#include <string.h>` is added conditionally under `#if PLATFORM_PC`.

**Why:** GBA DMA channels are hardware resources that do not exist on PC.

---

## 5. Tileset VDest pointer arrays: compile-time → runtime init

**File:** `src/tileset_anims.c`

On GBA the four destination-pointer arrays are `const` and initialised with absolute VRAM
offsets at compile time (e.g. `(u16 *)(BG_VRAM + TILE_OFFSET_4BPP(...))`).  On PC they are
declared as non-`const` arrays of the correct size with no initialiser, and a new static
function `InitTilesetAnimVDestPtrs()` fills them at runtime using the same offset arithmetic
relative to `BG_VRAM` (which on PC resolves to the heap-allocated VRAM base pointer).
`InitTilesetAnimVDestPtrs()` is called from `InitTilesetAnimations()` under `#if PLATFORM_PC`.

Offset values match the GBA arrays exactly:
- Mauville Flower1: `NUM_TILES_IN_PRIMARY + 96 + i*4`
- Mauville Flower2: `NUM_TILES_IN_PRIMARY + 128 + i*4`
- Rustboro WindyWater: `NUM_TILES_IN_PRIMARY + 128 + i*4`  *(same range as Flower2 — intentional; these tilesets never load simultaneously)*
- EverGrande Flowers: `NUM_TILES_IN_PRIMARY + 224 + i*4`

**Why:** On GBA `BG_VRAM` is the fixed hardware address `0x06000000`.  On PC VRAM is a heap
allocation whose address is not known until startup.

---

## 6. RFU / wireless adapter calls skipped on PC

**Files:** `src/main.c`, `src/link.c`

`InitRFU()`, `InitRFUAPI()`, `rfu_LMAN_REQBN_softReset_and_checkID()`, `rfu_REQ_stopMode()`,
`rfu_waitREQComplete()`, and the `IsWirelessAdapterConnected` connection-test body are all
wrapped in `#if !PLATFORM_PC` or `#if PLATFORM_GBA`.

**Why:** The RFU wireless adapter communicates over the GBA serial port using its own
interrupt-driven protocol.  Neither the hardware nor the RFU library exists on PC.

---

## 7. Heap-allocated VRAM / PLTT / OAM pointers

**File:** `src/main.c`

```c
#if PLATFORM_PC
u8 *gPCVram = NULL;
u8 *gPCPltt = NULL;
u8 *gPCOam  = NULL;
#endif
```

**Why:** On GBA `VRAM`, `PLTT`, and `OAM` expand to fixed hardware addresses.  On PC they expand
to these globals, which are populated at startup with heap allocations so game code can use the
same macros without faulting.

---

## 8. VBlank wait loop driven by platform register poll

**File:** `src/main.c`

```c
#if PLATFORM_PC
static void WaitForVBlank(void)
{
    gMain.intrCheck &= ~INTR_FLAG_VBLANK;
    while (!(gMain.intrCheck & INTR_FLAG_VBLANK))
        PlatformReadReg(REG_OFFSET_VCOUNT);   // drives simulated display state
}
#else
static void WaitForVBlank(void)
{
    gMain.intrCheck &= ~INTR_FLAG_VBLANK;
    while (!(gMain.intrCheck & INTR_FLAG_VBLANK))
        ;
}
#endif
```

**Why:** On GBA the busy-spin is woken by a real hardware VBLANK interrupt.  On PC the
interrupt is simulated; each `PlatformReadReg(REG_OFFSET_VCOUNT)` call pumps the
display-state machine and eventually fires the simulated interrupt.

---

## 9. Timer-based functions use platform register layer

**File:** `src/main.c`

`StartTimer1()` and `SeedRngAndSetTrainerId()` replace direct `REG_TM1CNT_H`/`REG_TM1CNT_L`
accesses with `PlatformWriteReg`/`PlatformReadReg` equivalents under `#if PLATFORM_PC`.

---

## 10. Key input read via platform layer

**File:** `src/main.c`

`ReadKeys()`: `REG_KEYINPUT ^ KEYS_MASK` → `PlatformReadReg(REG_OFFSET_KEYINPUT) ^ KEYS_MASK`
under `#if PLATFORM_PC`.

---

## 11. Interrupt-handler bootstrap skipped on PC

**File:** `src/main.c`

The `DmaCopy32` of `IntrMain` into `IntrMain_Buffer` and the assignment of `INTR_VECTOR` are
wrapped in `#if PLATFORM_GBA`.  `REG_IME = 1` is replaced with
`PlatformWriteReg(REG_OFFSET_IME, 1)` under `#if PLATFORM_PC`.

**Why:** On PC the interrupt vector is a redirected C global; the ARM assembly handler and
IWRAM copy step are not needed.

---

## 12. Save-struct size assertions disabled on PC

**File:** `src/save.c`

```c
#if !PLATFORM_PC
STATIC_ASSERT(sizeof(struct SaveBlock2) <= SECTOR_DATA_SIZE, ...);
STATIC_ASSERT(sizeof(struct SaveBlock1) <= SECTOR_DATA_SIZE * ..., ...);
STATIC_ASSERT(sizeof(struct PokemonStorage) <= SECTOR_DATA_SIZE * ..., ...);
#endif
```

**Why:** The PC save backend uses file I/O, not flash sectors.  The sector-size constraints
are irrelevant and would fire incorrectly for 64-bit struct layout.

---

## 13. ARM assembly traps replaced with `abort()`

**File:** `src/libisagbprn.c`

In `AGBAssert`, `NoCashGBAAssert`, and `MgbaAssert` the halt-on-assertion path
`.hword 0xEFFF` (GBA undefined-instruction debug trap) is replaced with `abort()` under
`#if PLATFORM_GBA` / `#else`.  `#include <stdlib.h>` is added under `#if PLATFORM_PC`.

**Why:** ARM undefined-instruction encoding is not valid x86-64.

---

## 14. BIOS HALT syscall replaced with infinite loop

**File:** `src/script.c`

When the null script pointer is executed on GBA the code does `asm("svc 2")` (BIOS `HALT`).
On PC this is replaced with `for(;;)` under `#if PLATFORM_GBA` / `#else`.

---

## Bugs fixed in this branch

### Fix: integer promotion before shift in PC pointer reconstruction

Several initial `#if PLATFORM_PC` pointer-load expressions used `(u16)X << 16` where the
`u16` is promoted to `int` before the shift, which can produce a negative `int` if bit 15 is
set, causing sign-extension when cast to `uintptr_t`.  All such sites were corrected to
`(uintptr_t)(u16)X << 16`.

Corrected in: `src/battle_anim_mons.c` (`LoadPointerFromVars`), `src/apprentice.c`
(`Task_ExecuteFuncAfterButtonPress`), `src/battle_factory_screen.c` (five reconstruction
sites), `src/pokeball.c` (`Task_PlayCryWhenReleasedFromBall`).

### Fix: `pokeball.c` pointer store using `u32` without PC guard

In `SpriteCB_ReleaseMon`, the `mon` pointer was packed into `tCryTaskMonPtr1`/`tCryTaskMonPtr2`
using plain `(u32)` casts (truncating the upper 32 bits) while the corresponding load in
`Task_PlayCryWhenReleasedFromBall` was already guarded with `uintptr_t`.  The store is now
wrapped in `#if PLATFORM_PC` to use `(uintptr_t)` casts, matching the load.

### Fix: `SetWordTaskArg`/`GetWordTaskArg` truncating 64-bit pointer in `menu.c`

`SetWordTaskArg` and `GetWordTaskArg` have `u32` signatures; calling them with a `uintptr_t`
pointer silently truncated the upper 32 bits.  The PC-specific versions of
`DecompressAndLoadBgGfxUsingHeap` and `task_free_buf_after_copying_tile_data_to_vram` in
`src/menu.c` were updated to use direct `gTasks[taskId].data[n]` field access with `uintptr_t`
casts, consistent with every other pointer-packing site in the port.

### Fix: `BgDmaFill` address type in `src/menu.c`

`BgDmaFill` computes a byte offset into VRAM and passes it to `RequestDma3Fill` as
`VRAM + addr`.  On GBA, `VRAM` is the fixed hardware address and `addr` was declared
`void *`; GCC permits `pointer + void *` as an extension (treating `void *` as byte-sized).
On PC, `VRAM` expands to `u8 *gPCVram` (a real pointer), so the offset must be an integer
for valid standard pointer arithmetic.  The `#if PLATFORM_PC` block declares `addr` as `u32`
instead of `void *`; the GBA path is unchanged.

---

## Header / assembly changes

### `include/global.h`

**`platform.h` and `<stdint.h>` included early**

`#include "platform.h"` is added immediately after the existing standard-library includes so that
`PLATFORM_PC` / `PLATFORM_GBA` guards are available everywhere downstream in the same translation
unit.  Under `#if PLATFORM_PC`, `<stdint.h>` is included to guarantee `uintptr_t` and other
fixed-width types (some toolchains don't pull these in via `<string.h>` alone).

**`T1_READ_PTR` / `T2_READ_PTR` cast through `uintptr_t`**

```c
// GBA:
#define T1_READ_PTR(ptr) (u8 *) T1_READ_32(ptr)

// PC:
#define T1_READ_PTR(ptr) (u8 *)(uintptr_t)(u32)T1_READ_32(ptr)
```

`T1_READ_32` / `T2_READ_32` assemble a 32-bit little-endian value from four bytes and return
`int` (the result of the bitwise-OR expression).  On 64-bit without the intermediate casts,
`(u8 *)(int)value` is implementation-defined; in practice GCC sign-extends negative `int`
values to 64-bit before the pointer cast, which gives a wrong high-address result.  The
intermediate `(u32)` cast zero-extends the signed `int` result to an unsigned 32-bit value
before `(uintptr_t)` widens it to 64 bits, ensuring the conversion follows unsigned widening
rules even when bit 31 of the datum is set.

**`__attribute__((packed))` on `SaveBlock2` and `SaveBlock1`**

```c
#if PLATFORM_PC
} __attribute__((packed)); // sizeof=0xF2C
#else
}; // sizeof=0xF2C
#endif
```

On 64-bit targets the compiler may insert alignment padding between struct members, changing the
binary layout and breaking save-file compatibility with the GBA format.  `packed` forces the
on-disk layout to match the GBA definition exactly.  x86-64 supports unaligned memory accesses
in hardware so `packed` is safe on PC; the `STATIC_ASSERT` size checks (disabled on PC in
`save.c`) still document the expected sizes in comments.

---

### `include/pokemon_storage_system.h`

`struct PokemonStorage` receives the same `__attribute__((packed))` treatment as the save blocks
above, for the same reason: layout must match the GBA binary format for save compatibility.

---

### `include/task.h`

```c
#if PLATFORM_PC
#define NUM_TASK_DATA 24  // struct ListMenu is 44 bytes on 64-bit; need data[] >= 44 bytes
#else
#define NUM_TASK_DATA 16
#endif
```

`struct Task` embeds an `s16 data[NUM_TASK_DATA]` array that game code uses as scratch storage.
Some subsystems cast the entire `data[]` array to a larger struct (notably `struct ListMenu`).
On GBA, `struct ListMenu` fits within `16 x sizeof(s16) = 32` bytes.  On 64-bit, pointer
members in `struct ListMenu` double in size, growing the struct to 44 bytes; `24 x 2 = 48`
bytes provides enough room.

`SetTaskFuncWithFollowupFunc` and `SwitchTaskToFollowupFunc` reserve `data[NUM_TASK_DATA-2]` and
`data[NUM_TASK_DATA-1]` for a packed follow-up function pointer.  Because both the store and the
load use `NUM_TASK_DATA - 2` symbolically, they track the same slot regardless of whether
`NUM_TASK_DATA` is 16 (GBA) or 24 (PC) — no index mismatch.

---

### `include/agb_flash.h`

```c
#if PLATFORM_PC
void SetFlashFilePath(const char *path);
#endif
```

Declares a PC-only helper that overrides the save-file path used by the PC flash emulation
layer (default: `"pokeemerald.sav"` in the working directory).  Not present on GBA because
flash is hardware-mapped and has no concept of a file path.

---

### `include/m4a.h`

```c
#if PLATFORM_PC
void m4aSoundShutdown(void);
#endif
```

Declares a PC-only teardown function that closes the SDL audio device cleanly on exit.
Registered via `atexit()` in `pc_main.c`.  Has no GBA equivalent because the hardware simply
powers off.

---

### `sound/MPlayDef.s`

```asm
#if PLATFORM_PC
    .include "sound/voicegroups/pc_voicegroup_offsets.inc"
#endif
```

On GBA, voicegroup data is linked into ROM at fixed addresses that the M4A engine references
directly.  On PC, ROM is not present; `pc_voicegroup_offsets.inc` provides the runtime-computed
base addresses for each voicegroup so the PC audio engine can locate them without embedded ROM
pointers.  The `#if PLATFORM_PC` guard ensures the GBA build is unaffected.

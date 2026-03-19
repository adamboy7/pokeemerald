# HLE — PC High-Level Emulation Layer

This document describes every source file that provides a PC-side replacement for GBA
hardware or firmware.  All files are compiled only under `#if PLATFORM_PC`; the GBA
build is never affected.

---

## Architecture overview

```
game code
    │
    ▼
platform.h          ← compile-time gating: PLATFORM_PC / PLATFORM_GBA
    │
    ├─ pc_main.c         entry point, heap VRAM/PLTT/OAM
    │
    ├─ pc_bios.c         GBA BIOS syscalls (SWI reimplementations)
    ├─ libagbsyscall.c   SoundDriver* thin wrappers → m4aSound*
    │
    ├─ pc_audio.c        M4A engine globals + SDL2 audio backend
    ├─ pc_m4a_stub.c     M4A engine internals (sequencer, mixer, jump-table cmds)
    ├─ sound/voicegroups/pc_voicegroup_offsets.inc   voicegroup symbols for assembler
    │
    ├─ pc_flash.c        flash save emulated as a flat file
    ├─ pc_io_reg.c       emulated I/O register array
    ├─ pc_rtc.c          RTC via time.h
    │
    ├─ pc_stub.c         ROM header + interrupt-buffer stubs
    ├─ pc_link_rfu_stub.c  wireless/RFU subsystem stubs
    ├─ pc_multiboot.c    GBA Multiboot (cable) stubs
    └─ libgcnmultiboot.c GameCube JOY-Bus multiboot stubs
```

---

## `include/platform.h`

Central platform-detection header included by `global.h` early in every translation
unit.

| Symbol | Purpose |
|--------|---------|
| `PLATFORM_PC` | user-defined (`-DPLATFORM_PC=1`); enables the whole HLE layer |
| `PLATFORM_GBA` | set to `0` when PC, `1` otherwise |
| `gPCDmaSrc[4]` / `gPCDmaDst[4]` | full-width (`uintptr_t`) copies of each DMA channel's source/destination, preventing 64-bit→32-bit truncation in `HandleDmas` |
| `PC_DMA_RECORD(dmaNum, src, dest)` | records a DMA transfer into the arrays above; compiles to nothing on GBA |

---

## `src/pc_main.c`

PC `main()` function.

1. Registers `m4aSoundShutdown` with `atexit`.
2. Allocates the three video-memory regions (`gPCVram`, `gPCPltt`, `gPCOam`) with
   `malloc`; these back the `VRAM`, `PLTT`, `OAM` macros on PC.
3. Calls `AgbMain()` to enter the game loop.

---

## `src/pc_bios.c`

C reimplementations of every GBA BIOS syscall (SWI) used by the engine.

### Reset

| Function | GBA SWI | PC behaviour |
|----------|---------|-------------|
| `SoftReset(flags)` | 0x00 | `exit(0)` |
| `SoftResetRom()` | — | `SoftReset(RESET_ALL)` |
| `SoftResetExram()` | — | `SoftReset(RESET_ALL)` |
| `RegisterRamReset(flags)` | 0x01 | if `RESET_SOUND_REGS`, calls `m4aSoundInit()`; other flags ignored |

### Wait / sync

| Function | GBA SWI | PC behaviour |
|----------|---------|-------------|
| `IntrWait(clearFlags, intrFlags)` | 0x04 | spin-pumps `PlatformReadReg(REG_OFFSET_VCOUNT)` until `INTR_CHECK` matches; SDL path adds a 1-frame timeout |
| `VBlankIntrWait()` | 0x05 | `IntrWait(1, INTR_FLAG_VBLANK)` |

### Math

| Function | GBA SWI | PC implementation |
|----------|---------|------------------|
| `Sqrt(num)` | 0x08 | `(u16)sqrt((double)num)` |
| `ArcTan(x)` | 0x09 | `atan(x/256.0)` → GBA binary-angle units |
| `ArcTan2(x,y)` | 0x0A | `atan2(y,x)` → GBA binary-angle units (unwrapped to `[0, 0x10000)`) |
| `Div/Mod/DivArm/ModArm` | 0x06 | standard C `/` and `%`; returns 0 on divide-by-zero |
| `MidiKey2Freq(key,frac,oct)` | 0x1F | `440 * 2^((semitone-69)/12)` |

### Copy / fill

| Function | GBA SWI | PC implementation |
|----------|---------|------------------|
| `CpuSet(src,dest,ctrl)` | 0x0B | C loop; respects 16/32-bit and fixed-src flags |
| `CpuFastSet(src,dest,ctrl)` | 0x0C | C loop over `u32`; respects fixed-src flag |

### Affine

| Function | GBA SWI | PC implementation |
|----------|---------|------------------|
| `BgAffineSet(src,dest,n)` | 0x0E | double-precision `cos`/`sin`; computes pa/pb/pc/pd and dx/dy |
| `ObjAffineSet(src,dest,n,offset)` | 0x0F | double-precision `cos`/`sin`; writes into strided destination |

### Decompression

All decompressors are pure C; Wram and Vram variants are identical on PC.

| Function pair | GBA SWI | Algorithm |
|---------------|---------|-----------|
| `LZ77UnCompWram/Vram` | 0x11/0x12 | LZ77 — sliding-window back-references, 8-flag bytes |
| `RLUnCompWram/Vram` | 0x14/0x15 | RLE — run/literal blocks |
| `HuffUnComp` | 0x13 | Huffman — 4-bit or 8-bit symbols, MSB-first bitstream |
| `BitUnPack` | 0x10 | bit-width expansion with optional offset-add |
| `Diff8bitUnFilter Wram/Vram` | 0x16/0x17 | 8-bit delta filter |
| `Diff16bitUnFilter` | 0x18 | 16-bit delta filter |

### Multiboot

`MultiBoot(mp)` — always returns 0 (no hardware link cable).

---

## `libagbsyscall/libagbsyscall.c`

Thin C wrappers that map the GBA BIOS sound-driver entry points
(`SoundDriverInit`, `SoundDriverMain`, `SoundDriverVSync`, `SoundDriverVSyncOff`,
`SoundDriverVSyncOn`, `SoundDriverMode`, `SoundBiasSet/Reset/Change`) to the
corresponding `m4aSound*` functions in `pc_audio.c`.

`MusicPlayerOpen/Start/Stop/Continue/FadeOut` and `SoundChannelClear` are declared
`__attribute__((weak))` no-ops; game code may override them if needed.

---

## `src/pc_audio.c`

Provides all M4A global state and the SDL2 audio backend.

### Globals (normally in GBA IWRAM)

`gSoundInfo`, `gSoundInfoPtr`, `gIntrCheck`, `gIntrVector`,
`gPokemonCrySongs`, `gPokemonCryMusicPlayers`, `gPokemonCryTracks`,
`gPokemonCrySong`, `gMPlayInfo_BGM/SE1/SE2/SE3`,
`gMPlayMemAccArea`, `gMPlayJumpTable`, `gCgbChans`,
`SoundMainRAM_Buffer` (0x800-byte buffer replacing the IWRAM code copy).

### Sound lifecycle

| Function | Role |
|----------|------|
| `m4aSoundInit()` | initialises `SoundInfo`, calls `MPlayExtender`, opens SDL audio device |
| `m4aSoundShutdown()` | closes SDL device; registered with `atexit` from `pc_main.c` |
| `m4aSoundMain()` | called each game tick; pumps `SoundMain` manually when SDL device unavailable |
| `m4aSoundVSync/VSync On/Off()` | mirrors M4A DMA counter bookkeeping |
| `m4aSoundMode(mode)` | sets sample rate, bit depth (8/16), master volume, channel count |

### SDL audio callback (`SdlAudioCallback`)

Called by SDL from its audio thread to fill the output buffer.  Each call:

1. Calls `SoundMain()` to advance the music sequencer and mix one PCM frame.
2. Copies the left/right PCM buffers from `SoundInfo` into the SDL stream, converting
   `s8` → `Sint16` when 16-bit output is requested.

### M4A player API

Full implementations of `m4aSongNum{Start,StartOrChange,StartOrContinue,Stop,Continue}`,
`m4aMPlay{AllStop,Continue,AllContinue,FadeOut,FadeOutTemporarily,FadeIn,ImmInit,Stop,
TempoControl,VolumeControl,PitchControl,PanpotControl,ModDepthSet,LFOSpeedSet}`, and
all `SetPokemonCry*` helpers.

### `MidiKeyToFreq`

Port of the GBA lookup-table frequency calculation (`gScaleTable` / `gFreqTable` /
`umul3232H32`) used to convert MIDI key + fine-adjust to a sample step frequency.

---

## `src/pc_m4a_stub.c`

Provides the M4A engine internals that on GBA live in `m4a.c` (compiled to IWRAM).
This file is always compiled on PC; `src/m4a.c` is excluded by `#if PLATFORM_GBA`.

### Utilities

| Function | Description |
|----------|-------------|
| `umul3232H32(a,b)` | `(u32)(((u64)a * b) >> 32)` — upper half of 32×32 multiply |
| `Clear64byte(addr)` | `memset(addr, 0, 64)` |

### CGB stubs

`CgbSound`, `CgbOscOff`, `MidiKeyToCgbFreq` — all no-ops; CGB channels are not
rendered on PC.

### Channel management

`RealClearChain(ch)` — unlinks a `SoundChannel` from the doubly-linked list of its
`MusicPlayerTrack`, clearing `ch->track` and patching `prev`/`next` pointers.

### `SoundMain` — PCM mixer

Called each audio frame (from the SDL callback via `pc_audio.c`).

1. Runs `MPlayMainHead` to advance all music players.
2. Iterates `soundInfo->chans[]`; for each active channel:
   - Computes a 16.16 fixed-point playback step from `ch->frequency / pcmFreq`.
   - Advances `ch->currentPointer`; handles loop wrap (`wav->loopStart` + `wav->size`)
     or stops the channel at end-of-sample.
   - Accumulates left/right into `s16 mixL/R[]` buffers applying per-channel and
     master volume.
3. Clamps and writes the mix to `soundInfo->pcmBuffer`.

### `MPlayMain` — music sequencer tick

One call = one audio frame.  For each active track:

- Decrements gate times; marks channels `SF_STOP` at zero.
- Decodes M4A command bytes until `track->wait > 0`; dispatches to
  `soundInfo->MPlayJumpTable` entries or calls `soundInfo->plynote`.
- Runs the LFO triangle oscillator; sets `MPT_FLG_PITCHG/VOLCHG` when modulation
  changes.

After all tracks: runs a volume/pitch update pass (`TrkVolPitSet`, `ChnVolSet`,
`MidiKeyToFreq` on every affected active channel).

### Jump-table command handlers (`ply_*`)

Implements all 36 jump-table slots required by M4A, including the extended-command
(`ply_xcmd`) sub-table entries:

`ply_fine`, `ply_goto`, `ply_patt`, `ply_pend`, `ply_rept`, `ply_prio`,
`ply_tempo`, `ply_keysh`, `ply_voice`, `ply_vol`, `ply_pan`, `ply_bend`,
`ply_bendr`, `ply_lfodl`, `ply_modt`, `ply_tune`, `ply_port` (no-op),
`ply_lfos`, `ply_mod`, `ply_memacc`, `ply_xcmd`, `ply_endtie`,
`ply_xwave`, `ply_xtype`, `ply_xatta`, `ply_xdeca`, `ply_xsust`, `ply_xrele`,
`ply_xiecv`, `ply_xiecl`, `ply_xleng`, `ply_xswee`, `ply_xwait`,
`ply_xcmd_0C/0D`, `ply_xxx`.

`ply_note` — allocates a `SoundChannel`, resolves SPL/RHY sub-tables, computes
priority, links the channel into the track's doubly-linked list, and populates all
channel fields before setting `SF_START`.

### Player lifecycle

| Function | Description |
|----------|-------------|
| `MPlayJumpTableCopy(t)` | fills the 36-entry table with all `ply_*` pointers |
| `MPlayOpen(info,tracks,n)` | zeroes player, links it into `soundInfo->MPlayMainHead` chain |
| `MPlayStart(info,header)` | starts a song: initialises tempo, sets `MPT_FLG_START` on each track |
| `TrackStop(info,track)` | stops all active channels on a track and clears its channel list |
| `FadeOutBody(info)` | handles fade-out / fade-in / temporary-fade logic each sequencer tick |
| `TrkVolPitSet(info,track)` | recomputes `volML/MR`, `keyM`, `pitM` from track parameters |
| `IsPokemonCryPlaying(info)` | scans channel list for any channel whose `track` belongs to `info` |

### Linker symbols

`gNumMusicPlayers` (= 4), `gMaxLines` (= 0), `SoundMainRAM[0x800]` — normally
placed by the GBA linker script; provided here as plain globals.

---

## `sound/voicegroups/pc_voicegroup_offsets.inc`

Assembler `.equ` directives setting every voicegroup symbol to `0`.

On GBA, voicegroups are linked into ROM at fixed addresses; M4A references them by
absolute pointer.  On PC there is no ROM, so `sound/MPlayDef.s` includes this file
under `#if PLATFORM_PC` to satisfy all voicegroup symbol references at assembly time.
The actual voicegroup data is accessed at runtime through the normal song-header
`tone` pointer, which is already a real heap pointer.

---

## `src/pc_flash.c`

Emulates the GBA flash chip as a flat binary file (`pokeemerald.sav` by default).

### Storage

`sFlashMemory[SECTORS_COUNT * SECTOR_SIZE]` — in-process byte array mirroring the
full flash address space.

### Lifecycle

| Function | Description |
|----------|-------------|
| `IdentifyFlash()` | loads save file into `sFlashMemory`; installs function pointers; registers `FlushFlashMemoryAtExit` with `atexit` |
| `SetFlashFilePath(path)` | overrides the default save-file path |
| `FlushFlashMemory()` | writes the entire array to disk; clears `sDirty` |
| `FlushFlashMemoryAtExit()` | called on exit; flushes only if `sDirty` is set |
| `LoadFlashMemory()` | reads save file; fills with `0xFF` for bytes beyond end-of-file or if file absent |

### R/W operations

| Function | GBA equivalent | PC behaviour |
|----------|---------------|-------------|
| `ReadFlash(sector,offset,dest,size)` | flash read | `memcpy` from `sFlashMemory` |
| `ProgramFlashByte_PC(sector,offset,data)` | byte write | sets byte; marks dirty |
| `ProgramFlashSector_PC(sector,src)` | sector write | copies `SECTOR_SIZE` bytes; flushes |
| `EraseFlashSector_PC(sector)` | sector erase | `memset` to `0xFF`; flushes |
| `EraseFlashChip_PC()` | chip erase | `memset` entire array to `0xFF`; flushes |
| `WaitForFlashWrite_PC(…)` | bus-timing poll | no-op; returns 0 immediately |
| `SetFlashTimerIntr(…)` | timer interrupt setup | no-op; returns 0 |
| `ProgramFlashSectorAndVerify(…)` | write + verify | delegates to `ProgramFlashSector_PC` |

---

## `src/pc_io_reg.c`

```c
u8 gIoRegisters[0x400];
```

A 1 KB byte array that backs the GBA I/O register window (`0x04000000`–`0x040003FF`).
`PlatformReadReg(offset)` and `PlatformWriteReg(offset, val)` (declared in
`platform/io.h`) index into this array.  The display-state machine and interrupt
simulation also read/write this array to drive `VCOUNT`, `DISPSTAT`, and
`INTR_CHECK`.

---

## `src/pc_rtc.c`

Implements the S-3511 RTC (`SiiRtc*` API) using the host system clock.

| Function | Behaviour |
|----------|-----------|
| `SiiRtcProbe()` | always returns 1 (RTC present) |
| `SiiRtcReset()` | returns `TRUE` |
| `SiiRtcGetStatus(rtc)` | sets `SIIRTCINFO_24HOUR`; returns `TRUE` |
| `SiiRtcGetDateTime(rtc)` / `SiiRtcGetTime(rtc)` | reads `time()` via `localtime_r` (POSIX) or `localtime_s` (Windows); converts to GBA BCD format |
| `SiiRtcSetDateTime/Time/Alarm/Status` | no-ops; return `TRUE` (PC clock is not game-writable) |
| `SiiRtcUnprotect/Protect()` | no-ops |
| `BinaryToBcd(v)` | `((v/10) << 4) \| (v%10)` |

---

## `src/pc_stub.c`

Provides symbols normally supplied by the GBA linker script or ROM header.

| Symbol | GBA source | PC definition |
|--------|-----------|--------------|
| `IntrMain[0x200]` | ARM interrupt handler in IWRAM | zero-filled `u32` array |
| `RomHeaderGameCode[GAME_CODE_LENGTH]` | ROM header bytes 0xAC–0xAF | zero-filled |
| `RomHeaderSoftwareVersion` | ROM header byte 0xBC | `0` |

---

## `src/pc_link_rfu_stub.c`

Stubs out the entire RFU wireless-adapter subsystem (~80 functions).

All functions are generated by four macros:

```c
#define STUB_VOID(name, args)   void name args {}
#define STUB_BOOL(name, args)   bool8  name args { return FALSE; }
#define STUB_BOOL32(name, args) bool32 name args { return FALSE; }
#define STUB_U8(name, args)     u8     name args { return 0; }
#define STUB_U32(name, args)    u32    name args { return 0; }
#define STUB_S32(name, args)    s32    name args { return 0; }
```

Global structs (`gRfu`, `gHostRfuGameData`, `gRfuLinkStatus`, `gRfuSlotStatusNI[]`,
`gWirelessStatusIndicatorSpriteId`, `lman`) are defined as zero-initialised globals.
`GetHostRfuGameData()` returns `&gHostRfuGameData`.

Two functions with non-trivial signatures are implemented directly:
`rfu_NI_setSendData` and `rfu_clearSlot` both return 0.

---

## `src/pc_multiboot.c`

Stubs for the GBA serial-port Multiboot protocol (single-pak cable multiplayer).

| Function | Behaviour |
|----------|-----------|
| `MultiBootInit(mp)` | `memset(mp, 0, sizeof(*mp))` |
| `MultiBootMain(mp)` | returns 0 (no transfer in progress) |
| `MultiBootStartProbe(mp)` | sets `mp->probe_count = 0` |
| `MultiBootStartMaster(mp,srcp,len,pal,speed)` | fills `boot_srcp`, `boot_endp`, `palette_data` |
| `MultiBootCheckComplete(mp)` | always returns 1 (transfer "complete") |

---

## `src/libgcnmultiboot.c`

Stubs for the GameCube JOY-Bus multiboot protocol.  All functions are no-ops; each
carries a `// TODO` comment noting what the real implementation would do.

| Function | Intended behaviour |
|----------|--------------------|
| `GameCubeMultiBoot_Hash(value,key)` | returns 0 |
| `GameCubeMultiBoot_Init(pStruct)` | no-op |
| `GameCubeMultiBoot_Main(pStruct)` | no-op |
| `GameCubeMultiBoot_ExecuteProgram(pStruct)` | no-op |
| `GameCubeMultiBoot_HandleSerialInterrupt(pStruct)` | no-op |
| `GameCubeMultiBoot_Quit()` | no-op |

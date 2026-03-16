# Pokémon Emerald

This is based off the decompilation of Pokémon Emerald, kindly provided by [Pret and all contributors](https://github.com/pret/pokeemerald). Changed have been made to allow the GBA version to compile, but also enable stubs and emulation-lite functionality for register handling and piping sound and video to enable building for a standard x86 PC. The addition of any branching or checks in the code effectively mean the compiler won't build a checksum perfect rom, but should still be functional complete and accurate.

To set up the repository, see [INSTALL.md](INSTALL.md). To compile the GBA rom, you can just run the `make` command. You can also compile the PC build with `make pc`, which uses the host compiler and includes the necessary stubs and emulations for things like the BIOS (hosts math helpers for in game functions and frame tied screen events), real time clock, and emulated save storage. This build outputs audio via SDL2; install SDL2 development libraries before building. The build system looks for SDL2 using `pkg-config`. If `pkg-config` is unavailable, provide SDL paths manually with the `SDL_CFLAGS` and `SDL_LIBS` variables:

```
make pc SDL_CFLAGS="-I/opt/SDL2/include" SDL_LIBS="-L/opt/SDL2/lib -lSDL2"
```

# That's really cool, but why not just emulate the GBA version?
Running pokemon Emerald in an emulator is perfectly serviceable and accessible, and you can even relatively easily access memory addresses for higher level computer science and general poking. But it comes at an overhead cost. While hardware requirements are laughable by today's standards, an emulator works with insrtuction by instruction translation- it takes everything the game and system have to say in ARM and translates to the new architecture. Nothing runs natively, but it runs.

# But you said you were emulating real time clock/save storage/bios
Emulation is a bit of a strong word, that's why I say emulation-lite. You could argue it's a form of high level emulation, but 99% of the overhead is removed thanks to the fact I only need to emulate the basic functionality and quirks of those three bits of hardware the game relies on. I don't need to worry about translating every possible arm CPU call, and can let the rest of the code run natively on x86 platforms. This allows you to paralellize many more copies than you could otherwise, and opens the door to GPU accelerated simulation.

# What about Arbitrary Code Execution (ACE)?

The original GBA hardware had no concept of memory protection. All RAM was readable, writable, and executable — the CPU would happily treat any byte in memory as an instruction if the program counter landed there. This is what makes ACE glitches possible on original hardware: tricks like carefully corrupted save data or specific in-game item arrangements can redirect execution into attacker-controlled memory regions, turning the game into a general-purpose code runner.

On this PC build, that class of glitch is silently broken by several modern OS and hardware features:

- **W^X (Write XOR Execute)**: Modern operating systems enforce that a memory page is either writable *or* executable, never both at the same time. Data buffers, the stack, and heap allocations are marked non-executable. If a glitch redirects the program counter into one of these regions, the CPU raises a protection fault and the process crashes instead of running attacker-controlled bytes.

- **NX/XD bits**: The CPU itself participates via the No-Execute (Intel: XD, AMD: NX) bit in page table entries. Even if the OS wanted to allow it, the hardware enforces the executable/non-executable distinction at the silicon level.

- **ASLR (Address Space Layout Randomization)**: On the GBA, every memory address is fixed and known at compile time. On PC, the OS randomizes where the stack, heap, and libraries are loaded each run. ACE glitches that rely on jumping to a specific hardcoded address will land somewhere unpredictable instead.

- **Stack canaries**: Compilers insert secret values before return addresses on the stack. A buffer overflow that tries to overwrite a return address will corrupt the canary first, and the runtime detects this and aborts before the hijacked address is ever used.

- **Safe save/flash emulation**: The PC build's save storage is implemented as normal file I/O through the standard library. There is no direct memory region that the save file maps into at a fixed executable address, so the classic "corrupt the save to write a payload at 0x02000000" style of ACE has no equivalent attack surface here.

In short: ACE on the original GBA is a consequence of extremely flat, unprotected memory. The PC build inherits none of those properties — the same glitch that launches homebrew on a cartridge will either crash the process or do nothing here.

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).

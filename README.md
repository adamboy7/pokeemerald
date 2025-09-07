# Pokémon Emerald

This is based off the decompilation of Pokémon Emerald, kindly provided by [Pret and all contributors](https://github.com/pret/pokeemerald). Changed have been made to allow the GBA version to compile, but also enable stubs and emulation-lite functionality for register handling and piping sound and video to enable building for a standard x86 PC. The addition of any branching or checks in the code effectively mean the compiler won't build a checksum perfect rom, but should still be functional complete and accurate.

To set up the repository, see [INSTALL.md](INSTALL.md). To compile the GBA rom, you can just run the `make` command. You can also compile the PC build with `make pc`, which uses the host compiler and includes the necessary stubs and emulations for things like the BIOS (hosts math helpers for in game functions and frame tied screen events), real time clock, and emulated save storage. This build outputs audio via SDL2; install SDL2 development libraries before building. The build system looks for SDL2 using `pkg-config`. If `pkg-config` is unavailable, provide SDL paths manually with the `SDL_CFLAGS` and `SDL_LIBS` variables:

```
make pc SDL_CFLAGS="-I/opt/SDL2/include" SDL_LIBS="-L/opt/SDL2/lib -lSDL2"
```

# That's really cool, but why not just emulate the GBA version?
Running pokemon Emerald in an emulator is a perfectly serviceable and accessible, and you can even relatively easily access memory addresses for higher level computer science and general poking. But it comes at an overhead cost. While hardware requirements are laughable by today's standards, an emulator works with insrtuction by instruction translation- it translates everything the game and system have to say in ARM and converts to the new architecture. Nothing runs natively, but it runs.

# But you said you were emulating real time clock/save storage/bios
Emulation is a bit of a strong word, that's why I say emulation-lite. You could argue it's a form of high level emulation, but 99% of the overhead is removed thanks to the fact I only need to emulate basic functionality and quirks those three bits of hardware rely on. I don't need to worry about translating every possible arm CPU call, and can let the rest of the code run natively on x86 platforms. This allows you to paralellize many more copies than you could otherwise, and opens the door to GPU accelerated simulation.

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).

# Pokémon Emerald

This is based off the decompilation of Pokémon Emerald, kindly provided by [Pret and all contributors](https://github.com/pret/pokeemerald). Changed have been made to allow the GBA version to compile, but also enable stubs and emulation-lite functionality for register handling and piping sound and video to enable building for a standard x86 PC. The addition of any branching or checks in the code effectively mean the compiler won't build a checksum perfect rom, but should still be functional complete and accurate.

To set up the repository, see [INSTALL.md](INSTALL.md). To compile the GBA rom, you can just run the `make` command. You can also compile the PC build with `make pc`, which uses the host compiler and includes `src/pc_bios.c`. This build outputs audio via SDL2; install SDL2 development libraries before building. The build system looks for SDL2 using `pkg-config`. If `pkg-config` is unavailable, provide SDL paths manually with the `SDL_CFLAGS` and `SDL_LIBS` variables:

```
make pc SDL_CFLAGS="-I/opt/SDL2/include" SDL_LIBS="-L/opt/SDL2/lib -lSDL2"
```

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).

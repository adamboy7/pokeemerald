# Pokémon Emerald

This is a decompilation of Pokémon Emerald.

It builds the following ROM:

* [**pokeemerald.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=1961) `sha1: f3ae088181bf583e55daf962a92bb46f4f1d07b7`

You can also compile a minimal PC build with `make pc`, which uses the host compiler and includes `src/pc_bios.c`. This build outputs audio via SDL2; install SDL2 development libraries before building. The build system looks for SDL2 using `pkg-config`. If `pkg-config` is unavailable, provide SDL paths manually with the `SDL_CFLAGS` and `SDL_LIBS` variables:

```
make pc SDL_CFLAGS="-I/opt/SDL2/include" SDL_LIBS="-L/opt/SDL2/lib -lSDL2"
```

To set up the repository, see [INSTALL.md](INSTALL.md).

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).

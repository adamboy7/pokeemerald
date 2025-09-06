#include "platform/io.h"

#ifdef PLATFORM_PC
#include <SDL2/SDL.h>
#include <stdlib.h>
#include "gba/io_reg.h"

// Emulated I/O register space. Shared with the macros in io_reg.h.
u8 gIoRegisters[0x400];

static SDL_Window *sWindow;
static SDL_Renderer *sRenderer;
static SDL_Texture *sTexture;
static u32 sFramebuffer[240 * 160];

static void InitVideo(void)
{
    if (sWindow != NULL)
        return;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        exit(1);

    sWindow = SDL_CreateWindow("pokeemerald", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               240, 160, 0);
    sRenderer = SDL_CreateRenderer(sWindow, -1, SDL_RENDERER_ACCELERATED);
    sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, 240, 160);
}

static void PresentFramebuffer(void)
{
    if (sWindow == NULL)
        return;

    SDL_UpdateTexture(sTexture, NULL, sFramebuffer, 240 * sizeof(u32));
    SDL_RenderClear(sRenderer);
    SDL_RenderCopy(sRenderer, sTexture, NULL, NULL);
    SDL_RenderPresent(sRenderer);
}

static void PollInput(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            exit(0);
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    u16 state = KEYS_MASK;

    if (keys[SDL_SCANCODE_Z])      state &= ~A_BUTTON;
    if (keys[SDL_SCANCODE_X])      state &= ~B_BUTTON;
    if (keys[SDL_SCANCODE_BACKSPACE]) state &= ~SELECT_BUTTON;
    if (keys[SDL_SCANCODE_RETURN]) state &= ~START_BUTTON;
    if (keys[SDL_SCANCODE_RIGHT])  state &= ~DPAD_RIGHT;
    if (keys[SDL_SCANCODE_LEFT])   state &= ~DPAD_LEFT;
    if (keys[SDL_SCANCODE_UP])     state &= ~DPAD_UP;
    if (keys[SDL_SCANCODE_DOWN])   state &= ~DPAD_DOWN;
    if (keys[SDL_SCANCODE_S])      state &= ~R_BUTTON;
    if (keys[SDL_SCANCODE_A])      state &= ~L_BUTTON;

    *(u16 *)(gIoRegisters + REG_OFFSET_KEYINPUT) = state;
}

u16 PlatformReadReg(u16 regOffset)
{
    if (regOffset == REG_OFFSET_KEYINPUT)
        PollInput();

    return *(u16 *)(gIoRegisters + regOffset);
}

void PlatformWriteReg(u16 regOffset, u16 value)
{
    *(u16 *)(gIoRegisters + regOffset) = value;

    switch (regOffset)
    {
    case REG_OFFSET_DISPCNT:
    case REG_OFFSET_BG0CNT:
    case REG_OFFSET_BG1CNT:
    case REG_OFFSET_BG2CNT:
    case REG_OFFSET_BG3CNT:
        InitVideo();
        PresentFramebuffer();
        break;
    }
}
#endif // PLATFORM_PC


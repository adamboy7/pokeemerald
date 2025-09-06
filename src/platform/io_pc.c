#include "platform/io.h"

#ifdef PLATFORM_PC
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gba/io_reg.h"

// Emulated I/O register space. Shared with the macros in io_reg.h.
u8 gIoRegisters[0x400];

#define DMA_CHANNELS 4
#define TIMER_COUNT 4

struct TimerState
{
    u16 reload;
    u16 control;
    u16 counter;
    Uint64 lastTick;
};

static struct TimerState sTimers[TIMER_COUNT];
static Uint64 sFrameStart;
static u16 sPrevDispstat;

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
    sFrameStart = SDL_GetPerformanceCounter();
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

static void TriggerFramebufferUpdate(void)
{
    InitVideo();
    PresentFramebuffer();
}

static void UpdateDisplayState(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    if (sFrameStart == 0)
        sFrameStart = now;

    double seconds = (double)(now - sFrameStart) / (double)freq;
    double frameDur = 1.0 / 60.0;
    double lineDur = frameDur / 228.0;
    u16 vcount = (u16)((int)(seconds / lineDur) % 228);

    *(u16 *)(gIoRegisters + REG_OFFSET_VCOUNT) = vcount;

    u16 dispstat = *(u16 *)(gIoRegisters + REG_OFFSET_DISPSTAT);
    u16 prev = sPrevDispstat;
    dispstat &= ~(DISPSTAT_VBLANK | DISPSTAT_HBLANK | DISPSTAT_VCOUNT);
    if (vcount >= 160)
        dispstat |= DISPSTAT_VBLANK;
    if (vcount == (dispstat >> 8))
        dispstat |= DISPSTAT_VCOUNT;
    *(u16 *)(gIoRegisters + REG_OFFSET_DISPSTAT) = dispstat;

    if ((dispstat & DISPSTAT_VBLANK) && !(prev & DISPSTAT_VBLANK) && (dispstat & DISPSTAT_VBLANK_INTR))
        *(u16 *)(gIoRegisters + REG_OFFSET_IF) |= INTR_FLAG_VBLANK;
    if ((dispstat & DISPSTAT_VCOUNT) && !(prev & DISPSTAT_VCOUNT) && (dispstat & DISPSTAT_VCOUNT_INTR))
        *(u16 *)(gIoRegisters + REG_OFFSET_IF) |= INTR_FLAG_VCOUNT;

    sPrevDispstat = dispstat;
}

static void UpdateTimers(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    static const u32 sPrescaler[4] = {1, 64, 256, 1024};

    for (int i = 0; i < TIMER_COUNT; i++)
    {
        struct TimerState *t = &sTimers[i];
        if (t->control & TIMER_ENABLE)
        {
            Uint64 diff = now - t->lastTick;
            u64 cycles = diff * 16777216ULL / freq;
            cycles /= sPrescaler[t->control & 3];
            if (cycles)
            {
                u32 value = t->counter + cycles;
                if (value >= 0x10000)
                {
                    t->counter = t->reload + (value & 0xFFFF);
                    if (t->control & TIMER_INTR_ENABLE)
                        *(u16 *)(gIoRegisters + REG_OFFSET_IF) |= (INTR_FLAG_TIMER0 << i);
                }
                else
                {
                    t->counter = (u16)value;
                }
                t->lastTick = now;
            }
        }
        *(u16 *)(gIoRegisters + REG_OFFSET_TM0CNT_L + i * 4) = t->counter;
        *(u16 *)(gIoRegisters + REG_OFFSET_TM0CNT_H + i * 4) = t->control;
    }
}

static void HandleDmas(void)
{
    for (int i = 0; i < DMA_CHANNELS; i++)
    {
        u32 base = REG_OFFSET_DMA0 + i * 12;
        u16 control = *(u16 *)(gIoRegisters + base + 10);
        if (control & DMA_ENABLE)
        {
            u32 src = *(u32 *)(gIoRegisters + base);
            u32 dst = *(u32 *)(gIoRegisters + base + 4);
            u8 *srcPtr = (u8 *)(uintptr_t)src;
            u8 *dstPtr = (u8 *)(uintptr_t)dst;
            u16 count = *(u16 *)(gIoRegisters + base + 8);
            u32 units = count;
            if (units == 0)
                units = (i == 3) ? 0x10000 : 0x4000;

            u32 unit = (control & DMA_32BIT) ? 4 : 2;
            s32 srcStep = unit;
            s32 dstStep = unit;

            switch (control & (DMA_SRC_DEC | DMA_SRC_FIXED))
            {
            case DMA_SRC_DEC:
                srcStep = -((s32)unit);
                break;
            case DMA_SRC_FIXED:
                srcStep = 0;
                break;
            }

            switch (control & (DMA_DEST_DEC | DMA_DEST_FIXED | DMA_DEST_RELOAD))
            {
            case DMA_DEST_DEC:
                dstStep = -((s32)unit);
                break;
            case DMA_DEST_FIXED:
                dstStep = 0;
                break;
            default:
                break;
            }

            for (u32 j = 0; j < units; j++)
            {
                if (unit == 4)
                    *(u32 *)dstPtr = *(u32 *)srcPtr;
                else
                    *(u16 *)dstPtr = *(u16 *)srcPtr;
                srcPtr += srcStep;
                dstPtr += dstStep;
            }

            *(u32 *)(gIoRegisters + base) = (u32)(uintptr_t)srcPtr;
            *(u32 *)(gIoRegisters + base + 4) = (u32)(uintptr_t)dstPtr;

            if (!(control & DMA_REPEAT))
            {
                control &= ~DMA_ENABLE;
                *(u16 *)(gIoRegisters + base + 10) = control;
            }

            if (control & DMA_INTR_ENABLE)
                *(u16 *)(gIoRegisters + REG_OFFSET_IF) |= (INTR_FLAG_DMA0 << i);

            TriggerFramebufferUpdate();
        }
    }
}

u16 PlatformReadReg(u16 regOffset)
{
    HandleDmas();
    UpdateTimers();
    UpdateDisplayState();

    if (regOffset == REG_OFFSET_KEYINPUT)
        PollInput();

    switch (regOffset)
    {
    case REG_OFFSET_TM0CNT_L:
    case REG_OFFSET_TM1CNT_L:
    case REG_OFFSET_TM2CNT_L:
    case REG_OFFSET_TM3CNT_L:
        return sTimers[(regOffset - REG_OFFSET_TM0CNT_L) / 4].counter;
    case REG_OFFSET_TM0CNT_H:
    case REG_OFFSET_TM1CNT_H:
    case REG_OFFSET_TM2CNT_H:
    case REG_OFFSET_TM3CNT_H:
        return sTimers[(regOffset - REG_OFFSET_TM0CNT_H) / 4].control;
    default:
        return *(u16 *)(gIoRegisters + regOffset);
    }
}

void PlatformWriteReg(u16 regOffset, u16 value)
{
    HandleDmas();
    UpdateTimers();
    UpdateDisplayState();

    switch (regOffset)
    {
    case REG_OFFSET_TM0CNT_L:
    case REG_OFFSET_TM1CNT_L:
    case REG_OFFSET_TM2CNT_L:
    case REG_OFFSET_TM3CNT_L:
    {
        int idx = (regOffset - REG_OFFSET_TM0CNT_L) / 4;
        sTimers[idx].reload = value;
        sTimers[idx].counter = value;
        *(u16 *)(gIoRegisters + regOffset) = value;
        break;
    }
    case REG_OFFSET_TM0CNT_H:
    case REG_OFFSET_TM1CNT_H:
    case REG_OFFSET_TM2CNT_H:
    case REG_OFFSET_TM3CNT_H:
    {
        int idx = (regOffset - REG_OFFSET_TM0CNT_H) / 4;
        sTimers[idx].control = value;
        *(u16 *)(gIoRegisters + regOffset) = value;
        if (value & TIMER_ENABLE)
        {
            sTimers[idx].counter = sTimers[idx].reload;
            sTimers[idx].lastTick = SDL_GetPerformanceCounter();
        }
        break;
    }
    case REG_OFFSET_IF:
        *(u16 *)(gIoRegisters + REG_OFFSET_IF) &= ~value;
        break;
    default:
        *(u16 *)(gIoRegisters + regOffset) = value;
        break;
    }

    HandleDmas();

    if (regOffset <= REG_OFFSET_BLDY)
        TriggerFramebufferUpdate();
}
#endif // PLATFORM_PC


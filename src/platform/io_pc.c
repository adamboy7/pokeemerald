#include "platform/io.h"

#ifdef PLATFORM_PC
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gba/io_reg.h"
#include "gba/defines.h"


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
static bool sShowSpriteBoxes;
static SDL_GameController *sController;

static void Render(void);

static void InitVideo(void)
{
    if (sWindow != NULL)
        return;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        exit(1);

    sWindow = SDL_CreateWindow("pokeemerald", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               240, 160, SDL_WINDOW_RESIZABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    sRenderer = SDL_CreateRenderer(sWindow, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(sRenderer, 240, 160);
    SDL_RenderSetIntegerScale(sRenderer, SDL_TRUE);
    sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, 240, 160);

    // Open the first available controller so gamepad input can be mapped to
    // REG_KEYINPUT alongside the keyboard state.
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        if (SDL_IsGameController(i))
        {
            sController = SDL_GameControllerOpen(i);
            break;
        }
    }
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
    InitVideo();
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            exit(0);
        if (e.type == SDL_KEYDOWN)
        {
            if (e.key.keysym.sym == SDLK_F1)
                sShowSpriteBoxes = !sShowSpriteBoxes;
        }
        if (e.type == SDL_CONTROLLERDEVICEADDED && sController == NULL)
        {
            if (SDL_IsGameController(e.cdevice.which))
                sController = SDL_GameControllerOpen(e.cdevice.which);
        }
        if (e.type == SDL_CONTROLLERDEVICEREMOVED && sController != NULL)
        {
            SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sController));
            if (id == e.cdevice.which)
            {
                SDL_GameControllerClose(sController);
                sController = NULL;
            }
        }
    }

    SDL_GameControllerUpdate();

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

    if (sController != NULL)
    {
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_A)) state &= ~A_BUTTON;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_B)) state &= ~B_BUTTON;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_BACK)) state &= ~SELECT_BUTTON;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_START)) state &= ~START_BUTTON;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) state &= ~DPAD_RIGHT;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) state &= ~DPAD_LEFT;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_UP)) state &= ~DPAD_UP;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) state &= ~DPAD_DOWN;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) state &= ~R_BUTTON;
        if (SDL_GameControllerGetButton(sController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) state &= ~L_BUTTON;
    }

    // Update the emulated key input register
    WRITE_REG_U16(REG_OFFSET_KEYINPUT, state);
}

static inline u32 PlttColorToArgb(u16 color)
{
    u32 r = (color & 0x1F) << 3;
    u32 g = ((color >> 5) & 0x1F) << 3;
    u32 b = ((color >> 10) & 0x1F) << 3;
    // Replicate high bits for smoother colors
    r |= r >> 5;
    g |= g >> 5;
    b |= b >> 5;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static void GetSpriteSize(int shape, int size, int *width, int *height)
{
    static const int dimensions[3][4][2] = {
        {{8,8}, {16,16}, {32,32}, {64,64}},       // square
        {{16,8}, {32,8}, {32,16}, {64,32}},       // horizontal
        {{8,16}, {8,32}, {16,32}, {32,64}},       // vertical
    };

    if (shape > 2 || size > 3)
    {
        *width = *height = 8;
        return;
    }
    *width = dimensions[shape][size][0];
    *height = dimensions[shape][size][1];
}

static void DrawRect(int x, int y, int w, int h, u32 color)
{
    int x2 = x + w - 1;
    int y2 = y + h - 1;
    if (x2 < 0 || y2 < 0 || x >= 240 || y >= 160)
        return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 >= 240) x2 = 239;
    if (y2 >= 160) y2 = 159;

    for (int i = x; i <= x2; i++)
    {
        sFramebuffer[y * 240 + i] = color;
        sFramebuffer[y2 * 240 + i] = color;
    }
    for (int j = y; j <= y2; j++)
    {
        sFramebuffer[j * 240 + x] = color;
        sFramebuffer[j * 240 + x2] = color;
    }
}

static void Render(void)
{
    u16 dispcnt = READ_REG_U16(REG_OFFSET_DISPCNT);
    u16 *bgPltt = (u16 *)gPCPltt;
    u16 *objPltt = (u16 *)(gPCPltt + BG_PLTT_SIZE);
    static u8 sPriorityBuf[240 * 160];

    u32 backdrop = PlttColorToArgb(bgPltt[0]);
    for (int i = 0; i < 240 * 160; i++)
    {
        sFramebuffer[i] = backdrop;
        sPriorityBuf[i] = 4;
    }

    if (dispcnt & DISPCNT_FORCED_BLANK)
        return;

    // Render backgrounds by priority (3 -> 0)
    for (int prio = 3; prio >= 0; prio--)
    {
        for (int bg = 0; bg < 4; bg++)
        {
            if (!(dispcnt & (DISPCNT_BG0_ON << bg)))
                continue;

            u16 bgcnt = READ_REG_U16(REG_OFFSET_BG0CNT + bg * 2);
            if ((bgcnt & 3) != prio)
                continue;

            bool is8bpp = bgcnt & BGCNT_256COLOR;
            u8 *charBase = BG_CHAR_ADDR((bgcnt >> 2) & 3);
            u8 *screenBase = BG_SCREEN_ADDR((bgcnt >> 8) & 31);
            int screenSize = (bgcnt >> 14) & 3;
            static const int sBgDimensions[4][2] = {{256,256},{512,256},{256,512},{512,512}};
            int width = sBgDimensions[screenSize][0];
            int height = sBgDimensions[screenSize][1];
            u16 hofs = READ_REG_U16(REG_OFFSET_BG0HOFS + bg * 8);
            u16 vofs = READ_REG_U16(REG_OFFSET_BG0VOFS + bg * 8);
            int tileSize = is8bpp ? 64 : 32;

            for (int y = 0; y < 160; y++)
            {
                int yCoord = (y + vofs) % height;
                int tileRow = yCoord / 8;
                int inTileY = yCoord % 8;
                for (int x = 0; x < 240; x++)
                {
                    int xCoord = (x + hofs) % width;
                    int tileCol = xCoord / 8;
                    int inTileX = xCoord % 8;
                    u32 block = 0;
                    switch (screenSize)
                    {
                    default:
                    case 0: block = 0; break;
                    case 1: block = (tileCol / 32); break; // 512x256
                    case 2: block = (tileRow / 32); break; // 256x512
                    case 3: block = (tileRow / 32) * 2 + (tileCol / 32); break; // 512x512
                    }
                    u16 *map = (u16 *)(screenBase + block * 0x800);
                    u16 entry = map[(tileRow % 32) * 32 + (tileCol % 32)];
                    u16 tileNum = entry & 0x3FF;
                    bool hflip = entry & 0x400;
                    bool vflip = entry & 0x800;
                    u8 palBank = entry >> 12;
                    u8 *tile = charBase + tileNum * tileSize;
                    int tx = hflip ? (7 - inTileX) : inTileX;
                    int ty = vflip ? (7 - inTileY) : inTileY;
                    u16 colorIndex;
                    if (is8bpp)
                    {
                        colorIndex = tile[ty * 8 + tx];
                    }
                    else
                    {
                        u8 byte = tile[ty * 4 + tx / 2];
                        colorIndex = (tx & 1) ? (byte >> 4) : (byte & 0xF);
                        colorIndex += palBank * 16;
                    }
                    u16 color = bgPltt[colorIndex];
                    int idx = y * 240 + x;
                    sFramebuffer[idx] = PlttColorToArgb(color);
                    sPriorityBuf[idx] = prio;
                }
            }
        }
    }

    // Render sprites
    if (dispcnt & DISPCNT_OBJ_ON)
    {
        bool obj1D = dispcnt & DISPCNT_OBJ_1D_MAP;
        u16 *oam = (u16 *)gPCOam;
        for (int i = 0; i < 128; i++)
        {
            u16 attr0 = oam[i * 4 + 0];
            u16 attr1 = oam[i * 4 + 1];
            u16 attr2 = oam[i * 4 + 2];
            if (((attr0 >> 8) & 3) == 2) // hidden
                continue;

            int y = attr0 & 0xFF;
            int x = attr1 & 0x1FF;
            if (y >= 160) y -= 256;
            if (x >= 240) x -= 512;

            int shape = (attr0 >> 14) & 3;
            int size = (attr1 >> 14) & 3;
            int width, height;
            GetSpriteSize(shape, size, &width, &height);

            bool hflip = attr1 & (1 << 12);
            bool vflip = attr1 & (1 << 13);
            bool is8bpp = attr0 & (1 << 13);
            int tileNum = attr2 & 0x3FF;
            int priority = (attr2 >> 10) & 3;
            int palNum = (attr2 >> 12) & 0xF;
            int tileSizeSprite = is8bpp ? 64 : 32;
            int tilesPerRow = is8bpp ? width / 8 : width / 8;

            for (int py = 0; py < height; py++)
            {
                int screenY = y + py;
                if (screenY < 0 || screenY >= 160)
                    continue;
                int ty = vflip ? (height - 1 - py) : py;
                int tileRow = ty / 8;
                int inTileY = ty % 8;
                for (int px = 0; px < width; px++)
                {
                    int screenX = x + px;
                    if (screenX < 0 || screenX >= 240)
                        continue;
                    int tx = hflip ? (width - 1 - px) : px;
                    int tileCol = tx / 8;
                    int inTileX = tx % 8;
                    int tileIndex;
                    if (obj1D)
                        tileIndex = tileNum + tileRow * tilesPerRow + tileCol;
                    else
                        tileIndex = tileNum + tileCol + tileRow * 32;
                    u8 *tile = OBJ_VRAM0 + tileIndex * tileSizeSprite;
                    u16 colorIndex;
                    if (is8bpp)
                    {
                        colorIndex = tile[inTileY * 8 + inTileX];
                        if (colorIndex == 0)
                            continue;
                    }
                    else
                    {
                        u8 byte = tile[inTileY * 4 + inTileX / 2];
                        colorIndex = (inTileX & 1) ? (byte >> 4) : (byte & 0xF);
                        if (colorIndex == 0)
                            continue;
                        colorIndex += palNum * 16;
                    }
                    int idx = screenY * 240 + screenX;
                    if (sPriorityBuf[idx] <= priority)
                        continue;
                    u16 color = objPltt[colorIndex];
                    sFramebuffer[idx] = PlttColorToArgb(color);
                    sPriorityBuf[idx] = priority;
                }
            }

            if (sShowSpriteBoxes)
                DrawRect(x, y, width, height, 0xFFFF00FF);
        }
    }
}

static void RenderAndPresent(void)
{
    InitVideo();
    Render();
    PresentFramebuffer();
}

static void UpdateDisplayState(void)
{
    PollInput();
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    if (sFrameStart == 0)
        sFrameStart = now;

    double seconds = (double)(now - sFrameStart) / (double)freq;
    double frameDur = 1.0 / 60.0;
    double lineDur = frameDur / 228.0;
    double lines = seconds / lineDur;
    int lineIndex = (int)lines;
    double lineTime = (lines - lineIndex) * lineDur;
    double activeDur = lineDur * (240.0 / 308.0);
    u16 vcount = (u16)(lineIndex % 228);

    WRITE_REG_U16(REG_OFFSET_VCOUNT, vcount);

    u16 dispstat = READ_REG_U16(REG_OFFSET_DISPSTAT);
    u16 prev = sPrevDispstat;
    dispstat &= ~(DISPSTAT_VBLANK | DISPSTAT_HBLANK | DISPSTAT_VCOUNT);
    if (vcount >= 160)
        dispstat |= DISPSTAT_VBLANK;
    if (lineTime >= activeDur)
        dispstat |= DISPSTAT_HBLANK;
    if (vcount == (dispstat >> 8))
        dispstat |= DISPSTAT_VCOUNT;
    WRITE_REG_U16(REG_OFFSET_DISPSTAT, dispstat);

    if ((dispstat & DISPSTAT_VBLANK) && !(prev & DISPSTAT_VBLANK))
    {
        if (dispstat & DISPSTAT_VBLANK_INTR)
            WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | INTR_FLAG_VBLANK);
        RenderAndPresent();
    }
    if ((dispstat & DISPSTAT_HBLANK) && !(prev & DISPSTAT_HBLANK) && (dispstat & DISPSTAT_HBLANK_INTR))
        WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | INTR_FLAG_HBLANK);
    if ((dispstat & DISPSTAT_VCOUNT) && !(prev & DISPSTAT_VCOUNT) && (dispstat & DISPSTAT_VCOUNT_INTR))
        WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | INTR_FLAG_VCOUNT);

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
                        WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | (INTR_FLAG_TIMER0 << i));
                }
                else
                {
                    t->counter = (u16)value;
                }
                t->lastTick = now;
            }
        }
        WRITE_REG_U16(REG_OFFSET_TM0CNT_L + i * 4, t->counter);
        WRITE_REG_U16(REG_OFFSET_TM0CNT_H + i * 4, t->control);
    }
}

static void HandleDmas(void)
{
    for (int i = 0; i < DMA_CHANNELS; i++)
    {
        u32 base = REG_OFFSET_DMA0 + i * 12;
        u16 control = READ_REG_U16(base + 10);
        if (control & DMA_ENABLE)
        {
            u32 src = READ_REG_U32(base);
            u32 dst = READ_REG_U32(base + 4);
            u8 *srcPtr = (u8 *)(uintptr_t)src;
            u8 *dstPtr = (u8 *)(uintptr_t)dst;
            u16 count = READ_REG_U16(base + 8);
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

            WRITE_REG_U32(base, (u32)(uintptr_t)srcPtr);
            WRITE_REG_U32(base + 4, (u32)(uintptr_t)dstPtr);

            if (!(control & DMA_REPEAT))
            {
                control &= ~DMA_ENABLE;
                WRITE_REG_U16(base + 10, control);
            }

            if (control & DMA_INTR_ENABLE)
                WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | (INTR_FLAG_DMA0 << i));

            RenderAndPresent();
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
        return READ_REG_U16(regOffset);
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
        WRITE_REG_U16(regOffset, value);
        break;
    }
    case REG_OFFSET_TM0CNT_H:
    case REG_OFFSET_TM1CNT_H:
    case REG_OFFSET_TM2CNT_H:
    case REG_OFFSET_TM3CNT_H:
    {
        int idx = (regOffset - REG_OFFSET_TM0CNT_H) / 4;
        sTimers[idx].control = value;
        WRITE_REG_U16(regOffset, value);
        if (value & TIMER_ENABLE)
        {
            sTimers[idx].counter = sTimers[idx].reload;
            sTimers[idx].lastTick = SDL_GetPerformanceCounter();
        }
        break;
    }
    case REG_OFFSET_IF:
        WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) & ~value);
        break;
    default:
        WRITE_REG_U16(regOffset, value);
        break;
    }

    HandleDmas();

    if (regOffset <= REG_OFFSET_BLDY)
        RenderAndPresent();
}
#endif // PLATFORM_PC


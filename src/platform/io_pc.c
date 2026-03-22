#include "platform/io.h"

#if PLATFORM_PC
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gba/io_reg.h"
#include "gba/defines.h"


// Forward declaration of the interrupt handler table populated by InitIntrHandlers().
// Use the raw function pointer type to avoid a conflicting typedef with main.h.
extern void (*gIntrTable[])(void);

#define DMA_CHANNELS 4
#define TIMER_COUNT 4

// Full-width (host-pointer-sized) DMA address shadow.
// DmaSetUnchecked (include/gba/macro.h) truncates src/dst to u32 when writing
// to gIoRegisters, which loses the upper 32 bits on 64-bit builds.  These
// arrays preserve the full pointer so HandleDmas can use correct addresses.
uintptr_t gPCDmaSrc[DMA_CHANNELS];
uintptr_t gPCDmaDst[DMA_CHANNELS];

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

// Per-pixel layer and priority tracking used by the graphics effects post-pass.
// sLayerBuf / sLayerBuf2 hold the layer index (0-5) of the topmost and second-topmost
// pixel respectively. sFramebuffer2 holds the second-topmost colour for alpha blending.
// sWindowMask holds the per-pixel layer-enable bitmask derived from WIN0/WIN1 registers.
static u8  sPriorityBuf[240 * 160];
static u8  sLayerBuf[240 * 160];
static u32 sFramebuffer2[240 * 160];
static u8  sLayerBuf2[240 * 160];
static u8  sWindowMask[240 * 160];

// Layer index constants — these match the bit positions of the BLDCNT target fields.
#define LAYER_BG0 0
#define LAYER_BG1 1
#define LAYER_BG2 2
#define LAYER_BG3 3
#define LAYER_OBJ 4
#define LAYER_BD  5

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

// Commit a pixel to the framebuffer, pushing the previous top pixel to the
// second slot so that the alpha-blend post-pass has both layers available.
static inline void CommitPixel(int idx, u32 color, u8 layer)
{
    sLayerBuf2[idx]    = sLayerBuf[idx];
    sFramebuffer2[idx] = sFramebuffer[idx];
    sLayerBuf[idx]     = layer;
    sFramebuffer[idx]  = color;
}

// Build sWindowMask[240*160].  Each byte is a bitmask of which layers may
// draw at that screen position: bits 0-3 = BG0-BG3, bit 4 = OBJ.
// When no windows are active every pixel gets 0x1F (all layers enabled).
static void ComputeWindowMask(u16 dispcnt)
{
    bool win0   = (dispcnt & DISPCNT_WIN0_ON)   != 0;
    bool win1   = (dispcnt & DISPCNT_WIN1_ON)   != 0;
    bool objwin = (dispcnt & DISPCNT_OBJWIN_ON) != 0;

    if (!win0 && !win1 && !objwin)
    {
        memset(sWindowMask, 0x1F, sizeof(sWindowMask));
        return;
    }

    u16 winin  = READ_REG_U16(REG_OFFSET_WININ);
    u16 winout = READ_REG_U16(REG_OFFSET_WINOUT);

    u8 out_mask  = winout & 0x1F;          // outside all windows
    u8 win0_mask = winin & 0x1F;           // inside WIN0
    u8 win1_mask = (winin >> 8) & 0x1F;   // inside WIN1

    u16 win0h = win0 ? READ_REG_U16(REG_OFFSET_WIN0H) : 0;
    u16 win0v = win0 ? READ_REG_U16(REG_OFFSET_WIN0V) : 0;
    u16 win1h = win1 ? READ_REG_U16(REG_OFFSET_WIN1H) : 0;
    u16 win1v = win1 ? READ_REG_U16(REG_OFFSET_WIN1V) : 0;

    int win0x1 = win0h >> 8,  win0x2 = win0h & 0xFF;
    int win0y1 = win0v >> 8,  win0y2 = win0v & 0xFF;
    int win1x1 = win1h >> 8,  win1x2 = win1h & 0xFF;
    int win1y1 = win1v >> 8,  win1y2 = win1v & 0xFF;

    for (int y = 0; y < 160; y++)
    {
        bool inWin0y = win0 && y >= win0y1 && y < win0y2;
        bool inWin1y = win1 && y >= win1y1 && y < win1y2;
        for (int x = 0; x < 240; x++)
        {
            u8 mask;
            if (inWin0y && x >= win0x1 && x < win0x2)
                mask = win0_mask;
            else if (inWin1y && x >= win1x1 && x < win1x2)
                mask = win1_mask;
            else
                mask = out_mask;
            sWindowMask[y * 240 + x] = mask;
        }
    }
}

// Apply colour special effects (BLDCNT / BLDALPHA / BLDY) to sFramebuffer.
// Must be called after all layers have been rendered and sLayerBuf is final.
static void ApplyColorEffects(void)
{
    u16 bldcnt = READ_REG_U16(REG_OFFSET_BLDCNT);
    u8  effect  = (bldcnt >> 6) & 3;
    if (effect == 0)
        return;

    u8 target1 = bldcnt & 0x3F;       // bits 0-5: BG0,BG1,BG2,BG3,OBJ,BD

    if (effect >= 2)
    {
        // Effect 2: brightness increase   Effect 3: brightness decrease
        u32 bldy = READ_REG_U16(REG_OFFSET_BLDY) & 0x1F;
        if (bldy > 16) bldy = 16;
        if (bldy == 0) return;

        for (int i = 0; i < 240 * 160; i++)
        {
            if (!((1u << sLayerBuf[i]) & target1))
                continue;
            u32 argb = sFramebuffer[i];
            u32 r = (argb >> 16) & 0xFF;
            u32 g = (argb >>  8) & 0xFF;
            u32 b =  argb        & 0xFF;
            if (effect == 2)
            {
                r += (255 - r) * bldy / 16;
                g += (255 - g) * bldy / 16;
                b += (255 - b) * bldy / 16;
            }
            else
            {
                r = r - r * bldy / 16;
                g = g - g * bldy / 16;
                b = b - b * bldy / 16;
            }
            sFramebuffer[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    else
    {
        // Effect 1: alpha blend between first and second targets.
        u16 bldalpha = READ_REG_U16(REG_OFFSET_BLDALPHA);
        u32 eva = bldalpha & 0x1F;
        u32 evb = (bldalpha >> 8) & 0x1F;
        if (eva > 16) eva = 16;
        if (evb > 16) evb = 16;
        u8 target2 = (bldcnt >> 8) & 0x3F;  // bits 8-13: second target

        for (int i = 0; i < 240 * 160; i++)
        {
            if (!((1u << sLayerBuf[i])  & target1)) continue;
            if (!((1u << sLayerBuf2[i]) & target2)) continue;

            u32 c1 = sFramebuffer[i];
            u32 c2 = sFramebuffer2[i];
            u32 r = ((c1 >> 16 & 0xFF) * eva + (c2 >> 16 & 0xFF) * evb) / 16;
            u32 g = ((c1 >>  8 & 0xFF) * eva + (c2 >>  8 & 0xFF) * evb) / 16;
            u32 b = ((c1       & 0xFF) * eva + (c2        & 0xFF) * evb) / 16;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            sFramebuffer[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

static void Render(void)
{
    u16 dispcnt  = READ_REG_U16(REG_OFFSET_DISPCNT);
    u16 *bgPltt  = (u16 *)gPCPltt;
    u16 *objPltt = (u16 *)(gPCPltt + BG_PLTT_SIZE);
    int bgMode   = dispcnt & 7;

    u32 backdrop = PlttColorToArgb(bgPltt[0]);
    for (int i = 0; i < 240 * 160; i++)
    {
        sFramebuffer[i]  = backdrop;
        sFramebuffer2[i] = backdrop;
        sPriorityBuf[i]  = 4;
        sLayerBuf[i]     = LAYER_BD;
        sLayerBuf2[i]    = LAYER_BD;
    }

    if (dispcnt & DISPCNT_FORCED_BLANK)
        return;

    bool anyWin = (dispcnt & (DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJWIN_ON)) != 0;
    ComputeWindowMask(dispcnt);

    // Mosaic sizes (1 = no effect, 2+ = cell size in pixels).
    u16 mosaic  = READ_REG_U16(REG_OFFSET_MOSAIC);
    int bgMosH  = (mosaic & 0xF) + 1;
    int bgMosV  = ((mosaic >> 4) & 0xF) + 1;
    int objMosH = ((mosaic >> 8) & 0xF) + 1;
    int objMosV = ((mosaic >> 12) & 0xF) + 1;

    // --- Bitmap modes (3, 4, 5) ---
    // These modes use VRAM directly as a framebuffer instead of tile maps.
    if (bgMode == 3)
    {
        // Mode 3: 240×160, 15-bpp (RGB555), single page at VRAM offset 0.
        const u16 *src = (const u16 *)gPCVram;
        for (int y = 0; y < 160; y++)
            for (int x = 0; x < 240; x++)
                CommitPixel(y * 240 + x, PlttColorToArgb(src[y * 240 + x]), LAYER_BG2);
        ApplyColorEffects();
        return;
    }
    if (bgMode == 4)
    {
        // Mode 4: 240×160, 8-bpp palette-indexed, double-buffered.
        // DISPCNT bit 4 (DISPCNT_FRAME_SEL) selects page 0 (0x0000) or page 1 (0xA000).
        u32 page = (dispcnt & 0x0010) ? 0xA000 : 0x0000;
        const u8 *src = (const u8 *)gPCVram + page;
        for (int y = 0; y < 160; y++)
            for (int x = 0; x < 240; x++)
                CommitPixel(y * 240 + x, PlttColorToArgb(bgPltt[src[y * 240 + x]]), LAYER_BG2);
        ApplyColorEffects();
        return;
    }
    if (bgMode == 5)
    {
        // Mode 5: 160×128, 15-bpp, double-buffered. Centred in the 240×160 display.
        u32 page = (dispcnt & 0x0010) ? 0xA000 : 0x0000;
        const u16 *src = (const u16 *)(gPCVram + page);
        int xOff = (240 - 160) / 2;
        int yOff = (160 - 128) / 2;
        for (int y = 0; y < 128; y++)
            for (int x = 0; x < 160; x++)
                CommitPixel((y + yOff) * 240 + (x + xOff), PlttColorToArgb(src[y * 160 + x]), LAYER_BG2);
        ApplyColorEffects();
        return;
    }

    // Affine map dimensions indexed by BGCNT bits 14-15.
    static const int sAffineMapSizes[4] = { 128, 256, 512, 1024 };

    // Render backgrounds, lowest priority first (drawn first = furthest back).
    for (int prio = 3; prio >= 0; prio--)
    {
        for (int bg = 0; bg < 4; bg++)
        {
            if (!(dispcnt & (DISPCNT_BG0_ON << bg)))
                continue;

            u16 bgcnt = READ_REG_U16(REG_OFFSET_BG0CNT + bg * 2);
            if ((bgcnt & 3) != prio)
                continue;

            // Affine BG: mode 1 → BG2 affine; mode 2 → BG2+BG3 affine.
            bool isAffine = (bgMode == 1 && bg == 2) ||
                            (bgMode == 2 && (bg == 2 || bg == 3));
            // Bit in the window mask corresponding to this BG.
            u8 layerBit = (u8)(1u << bg);
            bool mosEn = (bgcnt & 0x40) != 0;

            if (isAffine)
            {
                // Affine BG: scanline-based affine transformation using
                // the PA/PB/PC/PD matrix and reference point X/Y registers.
                u32 paOff = (bg == 2) ? REG_OFFSET_BG2PA : REG_OFFSET_BG3PA;
                u32 xOff  = (bg == 2) ? REG_OFFSET_BG2X  : REG_OFFSET_BG3X;
                u32 yOff  = (bg == 2) ? REG_OFFSET_BG2Y  : REG_OFFSET_BG3Y;

                s16 pa = (s16)READ_REG_U16(paOff);
                s16 pb = (s16)READ_REG_U16(paOff + 2);
                s16 pc = (s16)READ_REG_U16(paOff + 4);
                s16 pd = (s16)READ_REG_U16(paOff + 6);

                // 28.8 fixed-point reference point; sign-extend from bit 27.
                s32 refX = (s32)READ_REG_U32(xOff);
                s32 refY = (s32)READ_REG_U32(yOff);
                if (refX & 0x08000000) refX |= (s32)0xF8000000;
                if (refY & 0x08000000) refY |= (s32)0xF8000000;

                int mapSize  = sAffineMapSizes[(bgcnt >> 14) & 3];
                int mapTiles = mapSize / 8;
                bool wrap    = (bgcnt & BGCNT_WRAP) != 0;

                u8 *charBase = BG_CHAR_ADDR((bgcnt >> 2) & 3);
                u8 *mapBase  = BG_SCREEN_ADDR((bgcnt >> 8) & 31);

                for (int y = 0; y < 160; y++)
                {
                    int ym = mosEn ? (y / bgMosV) * bgMosV : y;
                    for (int x = 0; x < 240; x++)
                    {
                        int idx = y * 240 + x;
                        if (anyWin && !(sWindowMask[idx] & layerBit))
                            continue;

                        int xm = mosEn ? (x / bgMosH) * bgMosH : x;

                        s32 sx = refX + pa * xm + pb * ym;
                        s32 sy = refY + pc * xm + pd * ym;
                        int srcX = sx >> 8;
                        int srcY = sy >> 8;

                        if (wrap)
                        {
                            srcX = ((srcX % mapSize) + mapSize) % mapSize;
                            srcY = ((srcY % mapSize) + mapSize) % mapSize;
                        }
                        else if (srcX < 0 || srcX >= mapSize || srcY < 0 || srcY >= mapSize)
                            continue;

                        int tileX = srcX / 8, tileY = srcY / 8;
                        int inX   = srcX & 7, inY   = srcY & 7;
                        u8 tileIdx   = mapBase[tileY * mapTiles + tileX];
                        u8 *tile     = charBase + (u32)tileIdx * 64;
                        u8 colorIdx  = tile[inY * 8 + inX];
                        if (colorIdx == 0)
                            continue;

                        CommitPixel(idx, PlttColorToArgb(bgPltt[colorIdx]), (u8)bg);
                        sPriorityBuf[idx] = (u8)prio;
                    }
                }
            }
            else
            {
                // Text BG: tile-map based rendering with HOFS/VOFS scrolling.
                bool is8bpp    = (bgcnt & BGCNT_256COLOR) != 0;
                u8 *charBase   = BG_CHAR_ADDR((bgcnt >> 2) & 3);
                u8 *screenBase = BG_SCREEN_ADDR((bgcnt >> 8) & 31);
                int screenSize = (bgcnt >> 14) & 3;
                static const int sBgDimensions[4][2] =
                    { {256,256}, {512,256}, {256,512}, {512,512} };
                int mapW  = sBgDimensions[screenSize][0];
                int mapH  = sBgDimensions[screenSize][1];
                u16 hofs  = READ_REG_U16(REG_OFFSET_BG0HOFS + bg * 8);
                u16 vofs  = READ_REG_U16(REG_OFFSET_BG0VOFS + bg * 8);
                int tileSize = is8bpp ? 64 : 32;

                for (int y = 0; y < 160; y++)
                {
                    int ym       = mosEn ? (y / bgMosV) * bgMosV : y;
                    int yCoord   = (ym + vofs) % mapH;
                    int tileRow  = yCoord / 8;
                    int inTileY  = yCoord % 8;

                    for (int x = 0; x < 240; x++)
                    {
                        int idx = y * 240 + x;
                        if (anyWin && !(sWindowMask[idx] & layerBit))
                            continue;

                        int xm      = mosEn ? (x / bgMosH) * bgMosH : x;
                        int xCoord  = (xm + hofs) % mapW;
                        int tileCol = xCoord / 8;
                        int inTileX = xCoord % 8;

                        u32 block;
                        switch (screenSize)
                        {
                        default:
                        case 0: block = 0; break;
                        case 1: block = tileCol / 32; break;
                        case 2: block = tileRow / 32; break;
                        case 3: block = (tileRow / 32) * 2 + (tileCol / 32); break;
                        }
                        u16 *map   = (u16 *)(screenBase + block * 0x800);
                        u16  entry = map[(tileRow % 32) * 32 + (tileCol % 32)];
                        u16  tileNum = entry & 0x3FF;
                        bool hflip   = (entry & 0x400) != 0;
                        bool vflip   = (entry & 0x800) != 0;
                        u8   palBank = entry >> 12;
                        u8 *tile = charBase + tileNum * tileSize;
                        if (tile + tileSize > gPCVram + VRAM_SIZE)
                            continue;
                        int tx = hflip ? (7 - inTileX) : inTileX;
                        int ty = vflip ? (7 - inTileY)  : inTileY;

                        u16 colorIdx;
                        if (is8bpp)
                        {
                            colorIdx = tile[ty * 8 + tx];
                            if (colorIdx == 0) continue;
                        }
                        else
                        {
                            u8 byte = tile[ty * 4 + tx / 2];
                            colorIdx = (tx & 1) ? (byte >> 4) : (byte & 0xF);
                            if (colorIdx == 0) continue;  // transparent within palette bank
                            colorIdx += palBank * 16;
                        }

                        CommitPixel(idx, PlttColorToArgb(bgPltt[colorIdx]), (u8)bg);
                        sPriorityBuf[idx] = (u8)prio;
                    }
                }
            }
        }
    }

    // Render sprites
    if (dispcnt & DISPCNT_OBJ_ON)
    {
        bool obj1D = (dispcnt & DISPCNT_OBJ_1D_MAP) != 0;
        u16 *oam   = (u16 *)gPCOam;
        for (int i = 0; i < 128; i++)
        {
            u16 attr0 = oam[i * 4 + 0];
            u16 attr1 = oam[i * 4 + 1];
            u16 attr2 = oam[i * 4 + 2];
            if (((attr0 >> 8) & 3) == 2) // OBJ_MODE_HIDDEN
                continue;

            int y = attr0 & 0xFF;
            int x = attr1 & 0x1FF;
            if (y >= 160) y -= 256;
            if (x >= 240) x -= 512;

            int shape = (attr0 >> 14) & 3;
            int size  = (attr1 >> 14) & 3;
            int width, height;
            GetSpriteSize(shape, size, &width, &height);

            bool is8bpp  = (attr0 & (1 << 13)) != 0;
            int  tileNum = attr2 & 0x3FF;
            int  priority = (attr2 >> 10) & 3;
            int  palNum  = (attr2 >> 12) & 0xF;
            int  tileSz  = is8bpp ? 64 : 32;
            int  tilesW  = width / 8;
            bool mosEn   = (attr0 & 0x1000) != 0;

            // Flip bits are only meaningful in non-affine mode (attr0 bits 8-9 == 0).
            bool nonAffine = ((attr0 >> 8) & 1) == 0;
            bool hflip     = nonAffine && (attr1 & (1 << 12)) != 0;
            bool vflip     = nonAffine && (attr1 & (1 << 13)) != 0;

            for (int py = 0; py < height; py++)
            {
                int screenY = y + py;
                if (screenY < 0 || screenY >= 160)
                    continue;

                // Mosaic: sample from the top-left corner of the mosaic cell.
                int pym    = mosEn ? (py / objMosV) * objMosV : py;
                int tym    = vflip ? (height - 1 - pym) : pym;
                int tileRowM = tym / 8;
                int inTileYM = tym % 8;

                for (int px = 0; px < width; px++)
                {
                    int screenX = x + px;
                    if (screenX < 0 || screenX >= 240)
                        continue;
                    int idx = screenY * 240 + screenX;
                    if (anyWin && !(sWindowMask[idx] & (1 << LAYER_OBJ)))
                        continue;
                    if (sPriorityBuf[idx] <= priority)
                        continue;

                    int pxm    = mosEn ? (px / objMosH) * objMosH : px;
                    int tx     = hflip ? (width - 1 - pxm) : pxm;
                    int tileCol = tx / 8;
                    int inTileX = tx % 8;

                    int tileIndex;
                    if (obj1D)
                        tileIndex = tileNum + tileRowM * tilesW + tileCol;
                    else
                        tileIndex = tileNum + tileCol + tileRowM * 32;

                    u8 *tile = OBJ_VRAM0 + tileIndex * tileSz;
                    if (tile + tileSz > gPCVram + VRAM_SIZE)
                        continue;
                    u16 colorIdx;
                    if (is8bpp)
                    {
                        colorIdx = tile[inTileYM * 8 + inTileX];
                        if (colorIdx == 0) continue;
                    }
                    else
                    {
                        u8 byte = tile[inTileYM * 4 + inTileX / 2];
                        colorIdx = (inTileX & 1) ? (byte >> 4) : (byte & 0xF);
                        if (colorIdx == 0) continue;
                        colorIdx += palNum * 16;
                    }
                    CommitPixel(idx, PlttColorToArgb(objPltt[colorIdx]), LAYER_OBJ);
                    sPriorityBuf[idx] = (u8)priority;
                }
            }

            if (sShowSpriteBoxes)
                DrawRect(x, y, width, height, 0xFFFF00FF);
        }
    }

    // Post-pass: colour special effects (brightness / alpha blend).
    ApplyColorEffects();
}

static void RenderAndPresent(void)
{
    InitVideo();
    Render();
    PresentFramebuffer();
}

// Dispatch one pending interrupt to the appropriate gIntrTable handler.
// This replaces the ARM IntrMain handler from crt0.s (excluded on PC).
// The bit-to-table mapping mirrors the priority scan in crt0.s IntrMain:
//   IF bit 0=VBlank→[4], 1=HBlank→[3], 2=VCount→[0], 6=Timer3→[2],
//   7=Serial→[1], 3-5=Timer0-2→[5-7], 8-11=DMA0-3→[8-11], 12-13→[12-13]
static void DispatchInterrupts(void)
{
    static const int bitToTable[] = {4, 3, 0, 5, 6, 7, 2, 1, 8, 9, 10, 11, 12, 13};
    if (!READ_REG_U16(REG_OFFSET_IME))
        return;
    u16 pending = READ_REG_U16(REG_OFFSET_IE) & READ_REG_U16(REG_OFFSET_IF);
    for (int i = 0; i < 14; i++)
    {
        if (pending & (1 << i))
        {
            // Clear the flag before calling the handler (GBA interrupt protocol).
            WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) & ~(1 << i));
            gIntrTable[bitToTable[i]]();
            break; // one interrupt per call; re-enter next tick for additional pending
        }
    }
}

static void UpdateDisplayState(void)
{
    // Re-entrancy guard: the new SetGpuReg PC path calls PlatformReadReg /
    // PlatformWriteReg from within CopyBufferedValueToGpuReg, which would
    // re-enter here from an interrupt handler mid-update.  Mirrors GBA
    // hardware where interrupts are masked during handler execution.
    static bool sInUpdate = false;
    if (sInUpdate)
        return;
    sInUpdate = true;

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
    DispatchInterrupts();
    sInUpdate = false;
}

static void UpdateTimers(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    static const u32 sPrescaler[4] = {1, 64, 256, 1024};
    // Track per-timer overflow counts so cascade-mode timers can be driven.
    u32 overflowCount[TIMER_COUNT] = {0, 0, 0, 0};

    for (int i = 0; i < TIMER_COUNT; i++)
    {
        struct TimerState *t = &sTimers[i];
        if (!(t->control & TIMER_ENABLE))
            continue;

        u64 ticks = 0;
        if (t->control & TIMER_CASCADE)
        {
            // Cascade: this timer is incremented by the previous timer's overflows.
            if (i > 0)
                ticks = overflowCount[i - 1];
        }
        else
        {
            Uint64 diff = now - t->lastTick;
            u64 cycles = diff * 16777216ULL / freq;
            ticks = cycles / sPrescaler[t->control & 3];
            if (ticks)
                t->lastTick = now;
        }

        if (ticks)
        {
            u32 value = (u32)t->counter + (u32)ticks;
            if (value >= 0x10000)
            {
                overflowCount[i] = (value - t->reload) / (0x10000 - t->reload);
                t->counter = (u16)(t->reload + (value - 0x10000) % (0x10000 - t->reload));
                if (t->control & TIMER_INTR_ENABLE)
                    WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | (INTR_FLAG_TIMER0 << i));
            }
            else
            {
                t->counter = (u16)value;
            }
        }

        WRITE_REG_U16(REG_OFFSET_TM0CNT_L + i * 4, t->counter);
        WRITE_REG_U16(REG_OFFSET_TM0CNT_H + i * 4, t->control);
    }
}

// Execute a single DMA channel immediately using the full-width shadow pointers.
// Shared by both PCFireDmaNow and HandleDmas to avoid duplication.
static void FireDmaChannel(int i, bool renderAfter)
{
    u32 base = REG_OFFSET_DMA0 + i * 12;
    u16 control = READ_REG_U16(base + 10);

    u8 *srcPtr = (u8 *)gPCDmaSrc[i];
    u8 *dstPtr = (u8 *)gPCDmaDst[i];
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

    // Write back updated addresses to both shadow and I/O register array.
    gPCDmaSrc[i] = (uintptr_t)srcPtr;
    gPCDmaDst[i] = (uintptr_t)dstPtr;
    WRITE_REG_U32(base, (u32)(uintptr_t)srcPtr);
    WRITE_REG_U32(base + 4, (u32)(uintptr_t)dstPtr);

    if (!(control & DMA_REPEAT))
    {
        control &= ~DMA_ENABLE;
        WRITE_REG_U16(base + 10, control);
    }

    if (control & DMA_INTR_ENABLE)
        WRITE_REG_U16(REG_OFFSET_IF, READ_REG_U16(REG_OFFSET_IF) | (INTR_FLAG_DMA0 << i));

    if (renderAfter)
        RenderAndPresent();
}

// Called from PC_DMA_RECORD immediately after DmaSetUnchecked arms a channel.
// Fires DMA_START_NOW transfers right away so that:
//   (a) fill-value temporaries (from DMA_FILL_UNCHECKED) are still on the stack, and
//   (b) Dma3FillLarge_/Dma3CopyLarge_ loops correctly fire every chunk in order
//       rather than only the last one.
// VBLANK/HBLANK start DMAs are left pending for HandleDmas to fire at the
// correct phase.
void PCFireDmaNow(int dmaNum)
{
    u32 base = REG_OFFSET_DMA0 + dmaNum * 12;
    u16 control = READ_REG_U16(base + 10);
    if (!(control & DMA_ENABLE))
        return;
    if ((control & DMA_START_MASK) != DMA_START_NOW)
        return;
    FireDmaChannel(dmaNum, false);
}

static void HandleDmas(void)
{
    u16 dispstat = READ_REG_U16(REG_OFFSET_DISPSTAT);
    bool inVBlank = (dispstat & DISPSTAT_VBLANK) != 0;
    bool inHBlank = (dispstat & DISPSTAT_HBLANK) != 0;

    for (int i = 0; i < DMA_CHANNELS; i++)
    {
        u32 base = REG_OFFSET_DMA0 + i * 12;
        u16 control = READ_REG_U16(base + 10);
        if (!(control & DMA_ENABLE))
            continue;

        // Check start condition: only fire the DMA during the requested phase.
        u16 startMode = control & DMA_START_MASK;
        if (startMode == DMA_START_VBLANK && !inVBlank)
            continue;
        if (startMode == DMA_START_HBLANK && !inHBlank)
            continue;
        // DMA_START_SPECIAL is used for video-capture / sound FIFO — treated as
        // immediate for compatibility (game code enabling these expects them to fire).
        // DMA_START_NOW transfers are fired immediately by PCFireDmaNow, so
        // HandleDmas only reaches here for non-START_NOW channels.

        // Use the full-width shadow pointers to avoid 64-bit truncation.
        // gPCDmaSrc/gPCDmaDst are written by DmaSetUnchecked (via PC_DMA_RECORD)
        // before the DMA_ENABLE bit is set; they hold the complete host address.
        FireDmaChannel(i, true);
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

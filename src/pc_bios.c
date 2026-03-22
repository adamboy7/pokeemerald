#include "gba/gba.h"

#if PLATFORM_PC
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif
#include "m4a.h"
#include "platform/io.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Desktop implementations of a subset of the GBA BIOS calls. These aim to
// emulate the behaviour of the real BIOS closely enough for engine bring-up
// and unit testing on a PC.

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    // On a PC build, a soft reset can be approximated by terminating
    // the process. The launcher or invoking script is expected to
    // restart the program if desired.
    exit(0);
}

void SoftResetRom(void)
{
    SoftReset(RESET_ALL);
}

void SoftResetExram(void)
{
    SoftReset(RESET_ALL);
}

void RegisterRamReset(u32 resetFlags)
{
    if (resetFlags & RESET_VRAM)
        memset(gPCVram, 0, VRAM_SIZE);
    if (resetFlags & RESET_PALETTE)
        memset(gPCPltt, 0, PLTT_SIZE);
    if (resetFlags & RESET_OAM)
        memset(gPCOam, 0, OAM_SIZE);
    if (resetFlags & RESET_SOUND_REGS)
        m4aSoundInit();
}

void IntrWait(u32 clearFlags, u32 intrFlags)
{
    // On GBA the BIOS clears the matching bits in INTR_CHECK before waiting
    // (when clearFlags != 0), then spins until at least one requested flag is set.
    // On PC we drive UpdateDisplayState() via PlatformReadReg so that the
    // display simulation advances and DispatchInterrupts() can fire.
    if (clearFlags)
        INTR_CHECK &= ~(u16)intrFlags;

#ifdef USE_SDL
    // Spin-pump the register read loop until the interrupt fires or a
    // maximum of one frame elapses to prevent hanging on unknown flags.
    Uint64 start = SDL_GetPerformanceCounter();
    Uint64 freq  = SDL_GetPerformanceFrequency();
    Uint64 limit = freq / 60; // 1 frame timeout

    while (!(INTR_CHECK & (u16)intrFlags))
    {
        PlatformReadReg(REG_OFFSET_VCOUNT);
        if (SDL_GetPerformanceCounter() - start >= limit)
            break;
    }
#else
    clock_t start    = clock();
    clock_t duration = CLOCKS_PER_SEC / 60;
    while (!(INTR_CHECK & (u16)intrFlags) && clock() - start < duration)
        PlatformReadReg(REG_OFFSET_VCOUNT);
#endif
}

void VBlankIntrWait(void)
{
    IntrWait(1, INTR_FLAG_VBLANK);
}

u16 Sqrt(u32 num)
{
    return (u16)sqrt((double)num);
}

u16 ArcTan(s16 x)
{
    // GBA ArcTan takes a fixed-point value where 256 represents 1.0.
    double angle = atan((double)x / 256.0);
    return (u16)(angle * 0x8000 / M_PI);
}

u16 ArcTan2(s16 x, s16 y)
{
    double angle = atan2((double)y, (double)x);
    if (angle < 0)
        angle += 2 * M_PI;
    return (u16)(angle * 0x8000 / M_PI);
}

void CpuSet(const void *src, void *dest, u32 control)
{
    u32 count = control & 0x001FFFFF;
    bool32 use32 = control & CPU_SET_32BIT;
    bool32 fixed = control & CPU_SET_SRC_FIXED;

    if (use32)
    {
        const u32 *s = src;
        u32 *d = dest;
        u32 value = *s;
        for (u32 i = 0; i < count; i++)
            d[i] = fixed ? value : s[i];
    }
    else
    {
        const u16 *s = src;
        u16 *d = dest;
        u16 value = *s;
        for (u32 i = 0; i < count; i++)
            d[i] = fixed ? value : s[i];
    }
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    u32 count = control & 0x001FFFFF;
    bool32 fixed = control & CPU_FAST_SET_SRC_FIXED;
    const u32 *s = src;
    u32 *d = dest;
    u32 value = *s;

    for (u32 i = 0; i < count; i++)
        d[i] = fixed ? value : s[i];
}

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    for (s32 i = 0; i < count; i++)
    {
        double sx = (double)src[i].sx / 256.0;
        double sy = (double)src[i].sy / 256.0;
        double angle = src[i].alpha * M_PI / 32768.0;
        double cosA = cos(angle);
        double sinA = sin(angle);

        dest[i].pa = (s16)(cosA * sx * 256);
        dest[i].pb = (s16)(-sinA * sx * 256);
        dest[i].pc = (s16)(sinA * sy * 256);
        dest[i].pd = (s16)(cosA * sy * 256);

        dest[i].dx = src[i].texX - ((dest[i].pa * src[i].scrX + dest[i].pb * src[i].scrY) >> 8);
        dest[i].dy = src[i].texY - ((dest[i].pc * src[i].scrX + dest[i].pd * src[i].scrY) >> 8);
    }
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    s16 (*d)[4] = dest;
    for (s32 i = 0; i < count; i++)
    {
        double x = (double)src[i].xScale / 256.0;
        double y = (double)src[i].yScale / 256.0;
        double angle = src[i].rotation * M_PI / 32768.0;
        double cosA = cos(angle);
        double sinA = sin(angle);

        d[i * offset / sizeof(*d)][0] = (s16)(cosA * x * 256);
        d[i * offset / sizeof(*d)][1] = (s16)(-sinA * x * 256);
        d[i * offset / sizeof(*d)][2] = (s16)(sinA * y * 256);
        d[i * offset / sizeof(*d)][3] = (s16)(cosA * y * 256);
    }
}

static void LZ77UnComp(const u8 *src, u8 *dest)
{
    u32 header = *(const u32 *)src;
    src += 4;
    u32 remaining = header >> 8;

    while (remaining)
    {
        u8 flags = *src++;
        for (int i = 0; i < 8 && remaining; i++, flags <<= 1)
        {
            if (flags & 0x80)
            {
                u8 byte1 = *src++;
                u8 byte2 = *src++;
                int disp = ((byte1 & 0xF) << 8) | byte2;
                int count = (byte1 >> 4) + 3;
                const u8 *copySrc = dest - disp - 1;
                for (int j = 0; j < count && remaining; j++)
                {
                    *dest++ = *copySrc++;
                    remaining--;
                }
            }
            else
            {
                *dest++ = *src++;
                remaining--;
            }
        }
    }
}

static void RLUnComp(const u8 *src, u8 *dest)
{
    u32 header = *(const u32 *)src;
    src += 4;
    u32 remaining = header >> 8;

    while (remaining)
    {
        u8 info = *src++;
        if (info & 0x80)
        {
            int count = (info & 0x7F) + 3;
            u8 value = *src++;
            for (int i = 0; i < count && remaining; i++)
            {
                *dest++ = value;
                remaining--;
            }
        }
        else
        {
            int count = (info & 0x7F) + 1;
            for (int i = 0; i < count && remaining; i++)
            {
                *dest++ = *src++;
                remaining--;
            }
        }
    }
}

void LZ77UnCompWram(const u32 *src, void *dest)
{
    LZ77UnComp((const u8 *)src, dest);
}

void LZ77UnCompVram(const u32 *src, void *dest)
{
    LZ77UnComp((const u8 *)src, dest);
}

void RLUnCompWram(const u32 *src, void *dest)
{
    RLUnComp((const u8 *)src, dest);
}

void RLUnCompVram(const u32 *src, void *dest)
{
    RLUnComp((const u8 *)src, dest);
}

// GBA BIOS Huffman decompression (SWI 0x13).
// Header layout (little-endian):
//   Byte  0:      compression type — 0x28 (8-bit symbols) or 0x24 (4-bit symbols)
//   Bytes 1-3:    uncompressed output size in bytes
//   Byte  4:      tree_size — tree data is (tree_size + 1) * 2 bytes
//   Bytes 5..:    Huffman tree nodes (root at index 0)
// Bitstream follows the tree, 4-byte aligned from the start of src.
//
// Each tree node byte at index n:
//   Bit 7:  left  child (bit=0 path) is a leaf — value is tree[childPair]
//   Bit 6:  right child (bit=1 path) is a leaf — value is tree[childPair+1]
//   Bits 5-1: offset to child pair from current pair base:
//             childPair = (n & ~1) + (nodeVal & 0x3E) + 2
// Bitstream is MSB-first; 4-bit mode packs two nibbles per byte, low nibble first.
void HuffUnComp(const u8 *src, void *dest)
{
    u32 header = *(const u32 *)src;
    u8 bitDepth = header & 0xF;     // 4 or 8
    u32 outBytes = header >> 8;

    if (bitDepth != 4 && bitDepth != 8)
        return;

    u8 treeSize = src[4];
    const u8 *tree = src + 5;

    // Bitstream starts at next 4-byte boundary after header + tree_size byte + tree data.
    u32 treeDataBytes = (u32)(treeSize + 1) * 2;
    u32 streamOff = (5u + treeDataBytes + 3u) & ~3u;
    const u32 *stream = (const u32 *)(src + streamOff);

    u8 *dst = (u8 *)dest;
    u32 wordBuf = 0;
    int bitsLeft = 0;

    u32 pendingNibble = 0;  // first nibble waiting for second (4-bit mode)
    int hasPending = 0;
    u32 bytesOut = 0;

    while (bytesOut < outBytes)
    {
        // Walk the tree from the root to decode one symbol.
        u32 nodeIdx = 0;

        for (;;)
        {
            if (bitsLeft == 0)
            {
                wordBuf = *stream++;
                bitsLeft = 32;
            }
            u32 bit = (wordBuf >> 31) & 1;
            wordBuf <<= 1;
            bitsLeft--;

            u8 nodeVal = tree[nodeIdx];
            u32 pairBase = nodeIdx & ~1u;
            u32 childPair = pairBase + (nodeVal & 0x3E) + 2;
            u32 childIdx = childPair + bit;
            // bit 7 = left child is leaf, bit 6 = right child is leaf
            bool isLeaf = (nodeVal >> (7u - bit)) & 1u;

            if (isLeaf)
            {
                u8 symbol = tree[childIdx];
                if (bitDepth == 8)
                {
                    *dst++ = symbol;
                    bytesOut++;
                }
                else
                {
                    // 4-bit: accumulate two nibbles into one byte, low nibble first.
                    if (!hasPending)
                    {
                        pendingNibble = symbol & 0xF;
                        hasPending = 1;
                    }
                    else
                    {
                        *dst++ = (u8)(pendingNibble | ((symbol & 0xF) << 4));
                        bytesOut++;
                        hasPending = 0;
                    }
                }
                break;
            }
            nodeIdx = childIdx;
        }
    }
}

void BitUnPack(const void *src, void *dest, const struct BitUnPackParams *params)
{
    const u8 *s = src;
    u8 *d = dest;
    u32 srcBits = params->srcBitNum;
    u32 destBits = params->destBitNum;
    u32 mask = (1u << srcBits) - 1;
    u32 destMask = (1u << destBits) - 1;
    u32 add = params->destOffset;
    bool32 addZero = params->offset0On;

    u32 totalOut = (params->srcLength * 8) / srcBits;
    u32 buffer = 0;
    u32 bits = 0;

    for (u32 i = 0; i < totalOut; i++)
    {
        while (bits < srcBits)
        {
            buffer |= (u32)(*s++) << bits;
            bits += 8;
        }

        u32 value = buffer & mask;
        buffer >>= srcBits;
        bits -= srcBits;

        if (value || addZero)
            value = (value + add) & destMask;

        if (destBits <= 8)
        {
            *d++ = value;
        }
        else
        {
            *(u16 *)d = value;
            d += 2;
        }
    }
}

static void Diff8bitUnFilter(const u8 *src, u8 *dest)
{
    u32 header = *(const u32 *)src;
    src += 4;
    u32 remaining = header >> 8;
    s16 prev = 0;

    while (remaining--)
    {
        prev += (s8)(*src++);
        *dest++ = (u8)prev;
    }
}

void Diff8bitUnFilterWram(const void *src, void *dest)
{
    Diff8bitUnFilter(src, dest);
}

void Diff8bitUnFilterVram(const void *src, void *dest)
{
    Diff8bitUnFilter(src, dest);
}

void Diff16bitUnFilter(const void *src, void *dest)
{
    const u8 *s = src;
    u16 *d = dest;
    u32 header = *(const u32 *)s;
    s += 4;
    u32 remaining = header >> 8;
    s32 prev = 0;

    while (remaining)
    {
        s16 diff = (s16)(s[0] | (s[1] << 8));
        s += 2;
        prev += diff;
        *d++ = (u16)prev;
        remaining -= 2;
    }
}

int MultiBoot(struct MultiBootParam *mp)
{
    (void)mp;
    return 0;
}

s32 Div(s32 num, s32 denom)
{
    if (denom == 0)
        return 0;
    return num / denom;
}

s32 Mod(s32 num, s32 denom)
{
    if (denom == 0)
        return 0;
    return num % denom;
}

s32 DivArm(s32 num, s32 denom)
{
    return Div(num, denom);
}

s32 ModArm(s32 num, s32 denom)
{
    return Mod(num, denom);
}

u32 MidiKey2Freq(u8 key, u8 fractional, u8 octave)
{
    double semitone = key + fractional / 256.0 + octave * 12;
    double freq = 440.0 * pow(2.0, (semitone - 69) / 12.0);
    return (u32)freq;
}

#else
#error "PLATFORM_PC must be defined"
#endif // PLATFORM_PC


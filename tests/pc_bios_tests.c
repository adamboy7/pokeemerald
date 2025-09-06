#include "gba/types.h"
#include "gba/syscall.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void m4aSoundInit(void) {}
void m4aSoundMain(void) {}
void m4aSoundVSync(void) {}
void m4aSoundVSyncOff(void) {}
void m4aSoundVSyncOn(void) {}
void m4aSoundMode(u32 mode) { (void)mode; }

int main(void)
{
    // CpuSet 16-bit copy
    u16 src16[4] = {1, 2, 3, 4};
    u16 dest16[4] = {0};
    CpuSet(src16, dest16, 4);
    assert(memcmp(src16, dest16, sizeof(src16)) == 0);

    // CpuSet 32-bit fill
    u32 value = 0xDEADBEEF;
    u32 dest32[4] = {0};
    CpuSet(&value, dest32, CPU_SET_SRC_FIXED | CPU_SET_32BIT | 4);
    for (int i = 0; i < 4; i++)
        assert(dest32[i] == value);

    // BgAffineSet identity
    struct BgAffineSrcData src = {0, 0, 0, 0, 256, 256, 0};
    struct BgAffineDstData dst;
    BgAffineSet(&src, &dst, 1);
    assert(dst.pa == 256 && dst.pb == 0 && dst.pc == 0 && dst.pd == 256);

    // LZ77 decompression
    const unsigned char lzData[] = {0x10,0x04,0x00,0x00,0x00,0x41,0x42,0x43,0x44};
    unsigned char lzOut[4] = {0};
    LZ77UnCompWram((const u32*)lzData, lzOut);
    assert(memcmp(lzOut, "ABCD", 4) == 0);

    // RL decompression
    const unsigned char rlData[] = {0x30,0x08,0x00,0x00,0x81,0x41,0x81,0x42};
    unsigned char rlOut[8] = {0};
    RLUnCompWram((const u32*)rlData, rlOut);
    assert(memcmp(rlOut, "AAAABBBB", 8) == 0);

    printf("All tests passed\n");
    return 0;
}

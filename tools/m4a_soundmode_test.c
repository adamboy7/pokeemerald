#include <stdio.h>
#include <stdint.h>

typedef uint32_t u32;

#define SOUND_MODE_DA_BIT_8     0x00900000
#define SOUND_MODE_DA_BIT_7     0x00A00000
#define SOUND_MODE_DA_BIT       0x00B00000
#define SOUND_MODE_DA_BIT_SHIFT 20

static void TestSoundMode(u32 mode)
{
    u32 temp = mode & SOUND_MODE_DA_BIT;
    if (temp)
    {
        u32 daBits = 17 - ((temp >> SOUND_MODE_DA_BIT_SHIFT) & 0xF);
        printf("m4aSoundMode: requested %u-bit audio%s\n", daBits,
               daBits == 8 ? "" : " (unsupported on PC)");
    }
}

int main(void)
{
    TestSoundMode(SOUND_MODE_DA_BIT_8);
    TestSoundMode(SOUND_MODE_DA_BIT_7);
    return 0;
}

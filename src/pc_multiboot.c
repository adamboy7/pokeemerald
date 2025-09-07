#include "global.h"
#include "multiboot.h"
#include "gba/multiboot.h"

#if PLATFORM_PC
#include <string.h>

void MultiBootInit(struct MultiBootParam *mp)
{
    if (mp != NULL)
        memset(mp, 0, sizeof(*mp));
}

int MultiBootMain(struct MultiBootParam *mp)
{
    (void)mp;
    return 0;
}

void MultiBootStartProbe(struct MultiBootParam *mp)
{
    if (mp != NULL)
        mp->probe_count = 0;
}

void MultiBootStartMaster(struct MultiBootParam *mp, const u8 *srcp, int length, u8 palette_color, s8 palette_speed)
{
    (void)palette_speed;
    if (mp != NULL)
    {
        mp->boot_srcp = srcp;
        mp->boot_endp = srcp + length;
        mp->palette_data = palette_color;
    }
}

int MultiBootCheckComplete(struct MultiBootParam *mp)
{
    (void)mp;
    return 1;
}

#endif // PLATFORM_PC

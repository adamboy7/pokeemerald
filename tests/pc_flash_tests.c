#include "gba/gba.h"
#include "save.h"
#include "gba/flash_internal.h"
#include "agb_flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *path = "test_flash.sav";
    SetFlashFilePath(path);
    remove(path);

    IdentifyFlash();

    unsigned char writeBuf[SECTOR_SIZE];
    for (unsigned i = 0; i < SECTOR_SIZE; i++)
        writeBuf[i] = (unsigned char)i;

    assert(ProgramFlashSectorAndVerify(0, writeBuf) == 0);

    unsigned char readBuf[SECTOR_SIZE];
    memset(readBuf, 0, sizeof(readBuf));
    ReadFlash(0, 0, readBuf, SECTOR_SIZE);
    assert(memcmp(writeBuf, readBuf, SECTOR_SIZE) == 0);

    IdentifyFlash();
    memset(readBuf, 0, sizeof(readBuf));
    ReadFlash(0, 0, readBuf, SECTOR_SIZE);
    assert(memcmp(writeBuf, readBuf, SECTOR_SIZE) == 0);

    remove(path);
    printf("Flash read/write tests passed\n");
    return 0;
}

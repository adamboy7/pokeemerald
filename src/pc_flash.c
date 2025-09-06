#include "gba/gba.h"
#include "save.h"
#include "gba/flash_internal.h"
#include "agb_flash.h"

#ifdef PLATFORM_PC
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char sSaveFilePath[PATH_MAX] = "pokeemerald.sav";

static u8 sFlashMemory[SECTORS_COUNT * SECTOR_SIZE];

// Required global variables from flash_internal.h
u16 gFlashNumRemainingBytes;
u16 (*ProgramFlashByte)(u16, u32, u8);
u16 (*ProgramFlashSector)(u16, u8 *);
u16 (*EraseFlashChip)(void);
u16 (*EraseFlashSector)(u16);
u16 (*WaitForFlashWrite)(u8, u8 *, u8);
const u16 *gFlashMaxTime;
const struct FlashType *gFlash;
u8 (*PollFlashStatus)(u8 *);
u8 gFlashTimeoutFlag;

static u16 ProgramFlashByte_PC(u16 sectorNum, u32 offset, u8 data);
static u16 ProgramFlashSector_PC(u16 sectorNum, u8 *src);
static u16 EraseFlashSector_PC(u16 sectorNum);

void SetFlashFilePath(const char *path)
{
    if (path != NULL && path[0] != '\0')
    {
        strncpy(sSaveFilePath, path, sizeof(sSaveFilePath) - 1);
        sSaveFilePath[sizeof(sSaveFilePath) - 1] = '\0';
    }
}

static void FlushFlashMemory(void)
{
    FILE *file = fopen(sSaveFilePath, "wb");
    if (file == NULL)
    {
        perror("fopen");
        return;
    }

    if (fwrite(sFlashMemory, 1, sizeof(sFlashMemory), file) != sizeof(sFlashMemory))
        perror("fwrite");

    if (fclose(file) != 0)
        perror("fclose");
}

static void LoadFlashMemory(void)
{
    FILE *file = fopen(sSaveFilePath, "rb");
    if (file != NULL)
    {
        size_t read = fread(sFlashMemory, 1, sizeof(sFlashMemory), file);
        if (read != sizeof(sFlashMemory))
        {
            if (ferror(file))
                perror("fread");
            if (read < sizeof(sFlashMemory))
                memset(sFlashMemory + read, 0xFF, sizeof(sFlashMemory) - read);
        }
        if (fclose(file) != 0)
            perror("fclose");
    }
    else
    {
        if (errno != ENOENT)
            perror("fopen");
        memset(sFlashMemory, 0xFF, sizeof(sFlashMemory));
    }
}

u16 SetFlashTimerIntr(u8 timerNum, void (**intrFunc)(void))
{
    (void)timerNum;
    (void)intrFunc;
    return 0;
}

u16 IdentifyFlash(void)
{
    LoadFlashMemory();
    ProgramFlashByte = ProgramFlashByte_PC;
    ProgramFlashSector = ProgramFlashSector_PC;
    EraseFlashSector = EraseFlashSector_PC;
    return 0;
}

void ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size)
{
    if (sectorNum >= SECTORS_COUNT || offset + size > SECTOR_SIZE)
    {
        memset(dest, 0xFF, size);
        return;
    }

    memcpy(dest, sFlashMemory + sectorNum * SECTOR_SIZE + offset, size);
}

u16 ProgramFlashByte_PC(u16 sectorNum, u32 offset, u8 data)
{
    if (sectorNum >= SECTORS_COUNT || offset >= SECTOR_SIZE)
        return 0x8000;

    sFlashMemory[sectorNum * SECTOR_SIZE + offset] = data;
    FlushFlashMemory();
    return 0;
}

u16 EraseFlashSector_PC(u16 sectorNum)
{
    if (sectorNum >= SECTORS_COUNT)
        return 0x80FF;

    memset(sFlashMemory + sectorNum * SECTOR_SIZE, 0xFF, SECTOR_SIZE);
    FlushFlashMemory();
    return 0;
}

u16 ProgramFlashSector_PC(u16 sectorNum, u8 *src)
{
    if (sectorNum >= SECTORS_COUNT)
        return 0x80FF;

    gFlashNumRemainingBytes = SECTOR_SIZE;

    for (u32 i = 0; i < SECTOR_SIZE; i++)
    {
        sFlashMemory[sectorNum * SECTOR_SIZE + i] = src[i];
        gFlashNumRemainingBytes--;
    }

    FlushFlashMemory();
    return 0;
}

u32 ProgramFlashSectorAndVerify(u16 sectorNum, u8 *src)
{
    return ProgramFlashSector_PC(sectorNum, src);
}

#endif // PLATFORM_PC

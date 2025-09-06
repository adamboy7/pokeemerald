#include "global.h"
#include "save.h"
#include "gba/flash_internal.h"
#include "agb_flash.h"

#ifdef PLATFORM_PC
#include <stdio.h>
#include <string.h>

#define SAVE_FILE "pokeemerald.sav"

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

static void FlushFlashMemory(void)
{
    FILE *file = fopen(SAVE_FILE, "wb");
    if (file != NULL)
    {
        fwrite(sFlashMemory, 1, sizeof(sFlashMemory), file);
        fclose(file);
    }
}

static void LoadFlashMemory(void)
{
    FILE *file = fopen(SAVE_FILE, "rb");
    if (file != NULL)
    {
        fread(sFlashMemory, 1, sizeof(sFlashMemory), file);
        fclose(file);
    }
    else
    {
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
    memcpy(dest, sFlashMemory + sectorNum * SECTOR_SIZE + offset, size);
}

u16 ProgramFlashByte_PC(u16 sectorNum, u32 offset, u8 data)
{
    sFlashMemory[sectorNum * SECTOR_SIZE + offset] = data;
    FlushFlashMemory();
    return 0;
}

u16 EraseFlashSector_PC(u16 sectorNum)
{
    memset(sFlashMemory + sectorNum * SECTOR_SIZE, 0xFF, SECTOR_SIZE);
    FlushFlashMemory();
    return 0;
}

u16 ProgramFlashSector_PC(u16 sectorNum, u8 *src)
{
    memcpy(sFlashMemory + sectorNum * SECTOR_SIZE, src, SECTOR_SIZE);
    FlushFlashMemory();
    return 0;
}

u32 ProgramFlashSectorAndVerify(u16 sectorNum, u8 *src)
{
    return ProgramFlashSector_PC(sectorNum, src);
}

#endif // PLATFORM_PC

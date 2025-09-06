#include "m4a.h"
#include <string.h>
#ifdef PLATFORM_PC
struct MusicPlayerInfo gMPlayInfo_BGM;
struct MusicPlayerInfo gMPlayInfo_SE1;
struct MusicPlayerInfo gMPlayInfo_SE2;
struct MusicPlayerInfo gMPlayInfo_SE3;
struct SoundInfo gSoundInfo;
struct PokemonCrySong gPokemonCrySongs[MAX_POKEMON_CRIES];
struct PokemonCrySong gPokemonCrySong;
struct MusicPlayerInfo gPokemonCryMusicPlayers[MAX_POKEMON_CRIES];
struct MusicPlayerTrack gPokemonCryTracks[MAX_POKEMON_CRIES * 2];

static void ClearPlayer(struct MusicPlayerInfo *mplayInfo)
{
    memset(mplayInfo, 0, sizeof(*mplayInfo));
    mplayInfo->ident = ID_NUMBER;
}

void m4aSoundVSync(void) {}
void m4aSoundVSyncOn(void) {}
void m4aSoundVSyncOff(void) {}

void m4aSoundInit(void)
{
    memset(&gSoundInfo, 0, sizeof(gSoundInfo));
    ClearPlayer(&gMPlayInfo_BGM);
    ClearPlayer(&gMPlayInfo_SE1);
    ClearPlayer(&gMPlayInfo_SE2);
    ClearPlayer(&gMPlayInfo_SE3);
    memset(gPokemonCrySongs, 0, sizeof(gPokemonCrySongs));
    memset(gPokemonCryMusicPlayers, 0, sizeof(gPokemonCryMusicPlayers));
}

void m4aSoundMain(void) {}

static void StartSong(struct MusicPlayerInfo *mplayInfo, u16 n)
{
    (void)n;
    mplayInfo->status |= MUSICPLAYER_STATUS_TRACK;
}

void m4aSongNumStart(u16 n)
{
    StartSong(&gMPlayInfo_BGM, n);
}

void m4aSongNumStartOrChange(u16 n)
{
    StartSong(&gMPlayInfo_BGM, n);
}

void m4aSongNumStop(u16 n)
{
    (void)n;
    gMPlayInfo_BGM.status &= ~MUSICPLAYER_STATUS_TRACK;
}

void m4aMPlayAllStop(void)
{
    m4aSongNumStop(0);
    gMPlayInfo_SE1.status &= ~MUSICPLAYER_STATUS_TRACK;
    gMPlayInfo_SE2.status &= ~MUSICPLAYER_STATUS_TRACK;
    gMPlayInfo_SE3.status &= ~MUSICPLAYER_STATUS_TRACK;
}

void m4aMPlayStop(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo)
        mplayInfo->status &= ~MUSICPLAYER_STATUS_TRACK;
}

void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo)
        mplayInfo->status |= MUSICPLAYER_STATUS_TRACK;
}

void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)speed;
    m4aMPlayStop(mplayInfo);
}

void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)speed;
    m4aMPlayStop(mplayInfo);
}

void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)speed;
    m4aMPlayContinue(mplayInfo);
}

void m4aMPlayImmInit(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo)
        ClearPlayer(mplayInfo);
}

void m4aMPlayTempoControl(struct MusicPlayerInfo *mplayInfo, u16 tempo)
{
    (void)mplayInfo;
    (void)tempo;
}

void m4aMPlayVolumeControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u16 volume)
{
    (void)mplayInfo;
    (void)trackBits;
    (void)volume;
}

void m4aMPlayPitchControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, s16 pitch)
{
    (void)mplayInfo;
    (void)trackBits;
    (void)pitch;
}

void m4aMPlayPanpotControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, s8 pan)
{
    (void)mplayInfo;
    (void)trackBits;
    (void)pan;
}

void m4aMPlayModDepthSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 modDepth)
{
    (void)mplayInfo;
    (void)trackBits;
    (void)modDepth;
}

void m4aMPlayLFOSpeedSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 lfoSpeed)
{
    (void)mplayInfo;
    (void)trackBits;
    (void)lfoSpeed;
}

void m4aSoundMode(u32 mode)
{
    (void)mode;
}

struct MusicPlayerInfo *SetPokemonCryTone(struct ToneData *tone)
{
    (void)tone;
    ClearPlayer(&gPokemonCryMusicPlayers[0]);
    gPokemonCryMusicPlayers[0].status |= MUSICPLAYER_STATUS_TRACK;
    return &gPokemonCryMusicPlayers[0];
}

bool32 IsPokemonCryPlaying(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo && (mplayInfo->status & MUSICPLAYER_STATUS_TRACK))
        return TRUE;
    return FALSE;
}
#endif

#include "global.h"
#include "gba/m4a_internal.h"

#if PLATFORM_PC
#include <stdio.h>
#ifdef USE_SDL
#include <SDL2/SDL.h>
#include <stdlib.h>
static SDL_AudioDeviceID sAudioDevice;
// SDL audio format requested by m4aSoundMode.
static SDL_AudioFormat sAudioFormat = AUDIO_S8;
// Guard shared audio state accessed from the audio callback.
#define AUDIO_LOCK()   do { if (sAudioDevice != 0) SDL_LockAudioDevice(sAudioDevice); } while (0)
#define AUDIO_UNLOCK() do { if (sAudioDevice != 0) SDL_UnlockAudioDevice(sAudioDevice); } while (0)
#else
#define AUDIO_LOCK()
#define AUDIO_UNLOCK()
#endif

struct SoundInfo gSoundInfo;
struct PokemonCrySong gPokemonCrySongs[MAX_POKEMON_CRIES];
struct MusicPlayerInfo gPokemonCryMusicPlayers[MAX_POKEMON_CRIES];
struct MusicPlayerInfo gMPlayInfo_BGM;
struct MusicPlayerInfo gMPlayInfo_SE1;
struct MusicPlayerInfo gMPlayInfo_SE2;
struct MusicPlayerInfo gMPlayInfo_SE3;
struct MusicPlayerTrack gPokemonCryTracks[MAX_POKEMON_CRIES * 2];
struct PokemonCrySong gPokemonCrySong;
u8 gMPlayMemAccArea[0x10];
MPlayFunc gMPlayJumpTable[36];
struct CgbChannel gCgbChans[4];
ALIGNED(4) static char SoundMainRAM_Buffer[0x800] = {0};

#ifdef USE_SDL
static void SdlAudioCallback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    struct SoundInfo *soundInfo = &gSoundInfo;

    while (len > 0)
    {
        if (soundInfo->pcmDmaCounter == 0)
            soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;

        SoundMain();

        int offset = (soundInfo->pcmDmaPeriod - soundInfo->pcmDmaCounter) * soundInfo->pcmSamplesPerVBlank * 2;
        int count = soundInfo->pcmSamplesPerVBlank;

        if (sAudioFormat == AUDIO_S16SYS)
        {
            if (count * 4 > len)
                count = len / 4;

            s8 *srcL = soundInfo->pcmBuffer + offset / 2;
            s8 *srcR = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + offset / 2;
            Sint16 *dst = (Sint16 *)stream;

            for (int i = 0; i < count; i++)
            {
                dst[2 * i] = srcL[i] << 8;
                dst[2 * i + 1] = srcR[i] << 8;
            }

            stream += count * 4;
            len -= count * 4;
        }
        else
        {
            if (count * 2 > len)
                count = len / 2;

            s8 *srcL = soundInfo->pcmBuffer + offset / 2;
            s8 *srcR = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + offset / 2;

            for (int i = 0; i < count; i++)
            {
                stream[2 * i] = srcL[i];
                stream[2 * i + 1] = srcR[i];
            }

            stream += count * 2;
            len -= count * 2;
        }

        soundInfo->pcmDmaCounter--;
    }
}

void m4aSoundShutdown(void)
{
    if (sAudioDevice != 0)
    {
        SDL_CloseAudioDevice(sAudioDevice);
        sAudioDevice = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}
#endif // USE_SDL

u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 fineAdjust)
{
    u32 val1;
    u32 val2;
    u32 fineAdjustShifted = fineAdjust << 24;

    if (key > 178)
    {
        key = 178;
        fineAdjustShifted = 255 << 24;
    }

    val1 = gScaleTable[key];
    val1 = gFreqTable[val1 & 0xF] >> (val1 >> 4);

    val2 = gScaleTable[key + 1];
    val2 = gFreqTable[val2 & 0xF] >> (val2 >> 4);

    return umul3232H32(wav->freq, val1 + umul3232H32(val2 - val1, fineAdjustShifted));
}

void UnusedDummyFunc(void)
{
}

void MPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->ident++;
        mplayInfo->status &= ~MUSICPLAYER_STATUS_PAUSE;
        mplayInfo->ident = ID_NUMBER;
    }
}

void MPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->ident++;
        mplayInfo->fadeOC = speed;
        mplayInfo->fadeOI = speed;
        mplayInfo->fadeOV = (64 << FADE_VOL_SHIFT);
        mplayInfo->ident = ID_NUMBER;
    }
}

void SoundInit(struct SoundInfo *soundInfo)
{
    SOUND_INFO_PTR = soundInfo;
    CpuFill32(0, soundInfo, sizeof(struct SoundInfo));
    soundInfo->maxChans = 8;
    soundInfo->masterVolume = 15;
    soundInfo->plynote = ply_note;
    soundInfo->CgbSound = DummyFunc;
    soundInfo->CgbOscOff = (CgbOscOffFunc)DummyFunc;
    soundInfo->MidiKeyToCgbFreq = (MidiKeyToCgbFreqFunc)DummyFunc;
    soundInfo->ExtVolPit = (ExtVolPitFunc)DummyFunc;

    MPlayJumpTableCopy(gMPlayJumpTable);
    soundInfo->MPlayJumpTable = gMPlayJumpTable;
    SampleFreqSet(SOUND_MODE_FREQ_13379);
    soundInfo->ident = ID_NUMBER;
}

void SampleFreqSet(u32 freq)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    freq = (freq & 0xF0000) >> 16;
    soundInfo->freq = freq;
    soundInfo->pcmSamplesPerVBlank = gPcmSamplesPerVBlankTable[freq - 1];
    soundInfo->pcmDmaPeriod = PCM_DMA_BUF_SIZE / soundInfo->pcmSamplesPerVBlank;
    soundInfo->pcmFreq = (597275 * soundInfo->pcmSamplesPerVBlank + 5000) / 10000;
    soundInfo->divFreq = (16777216 / soundInfo->pcmFreq + 1) >> 1;
    soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;
}

void MPlayExtender(struct CgbChannel *cgbChans)
{
    struct SoundInfo *soundInfo;
    u32 ident;

    soundInfo = SOUND_INFO_PTR;

    ident = soundInfo->ident;

    if (ident != ID_NUMBER)
        return;

    soundInfo->ident++;

#if __STDC_VERSION__ < 202311L
    gMPlayJumpTable[8] = ply_memacc;
    gMPlayJumpTable[17] = ply_lfos;
    gMPlayJumpTable[19] = ply_mod;
    gMPlayJumpTable[28] = ply_xcmd;
    gMPlayJumpTable[29] = ply_endtie;
    gMPlayJumpTable[30] = SampleFreqSet;
    gMPlayJumpTable[31] = TrackStop;
    gMPlayJumpTable[32] = FadeOutBody;
    gMPlayJumpTable[33] = TrkVolPitSet;
#else
    gMPlayJumpTable[8] = (void (*)(...))ply_memacc;
    gMPlayJumpTable[17] = (void (*)(...))ply_lfos;
    gMPlayJumpTable[19] = (void (*)(...))ply_mod;
    gMPlayJumpTable[28] = (void (*)(...))ply_xcmd;
    gMPlayJumpTable[29] = (void (*)(...))ply_endtie;
    gMPlayJumpTable[30] = (void (*)(...))SampleFreqSet;
    gMPlayJumpTable[31] = (void (*)(...))TrackStop;
    gMPlayJumpTable[32] = (void (*)(...))FadeOutBody;
    gMPlayJumpTable[33] = (void (*)(...))TrkVolPitSet;
#endif

    soundInfo->cgbChans = cgbChans;
    soundInfo->CgbSound = CgbSound;
    soundInfo->CgbOscOff = CgbOscOff;
    soundInfo->MidiKeyToCgbFreq = MidiKeyToCgbFreq;
    soundInfo->maxLines = MAX_LINES;

    CpuFill32(0, cgbChans, sizeof(struct CgbChannel) * 4);

    cgbChans[0].type = 1;
    cgbChans[0].panMask = 0x11;
    cgbChans[1].type = 2;
    cgbChans[1].panMask = 0x22;
    cgbChans[2].type = 3;
    cgbChans[2].panMask = 0x44;
    cgbChans[3].panMask = 0x88;

    soundInfo->ident = ID_NUMBER;
}

void SoundClear(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    s32 i;
    void *chan;

    if (soundInfo->ident != ID_NUMBER)
        return;

    soundInfo->ident++;

    i = MAX_DIRECTSOUND_CHANNELS;
    chan = &soundInfo->chans[0];

    while (i > 0)
    {
        ((struct SoundChannel *)chan)->statusFlags = 0;
        i--;
        chan = (void *)((s32)chan + sizeof(struct SoundChannel));
    }

    chan = soundInfo->cgbChans;

    if (chan)
    {
        i = 1;

        while (i <= 4)
        {
            soundInfo->CgbOscOff(i);
            ((struct CgbChannel *)chan)->statusFlags = 0;
            i++;
            chan = (void *)((s32)chan + sizeof(struct CgbChannel));
        }
    }

    soundInfo->ident = ID_NUMBER;
}

void m4aSoundVSyncOff(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    AUDIO_LOCK();
    if (soundInfo->ident >= ID_NUMBER && soundInfo->ident <= ID_NUMBER + 1)
    {
        soundInfo->ident += 10;
        CpuFill32(0, soundInfo->pcmBuffer, sizeof(soundInfo->pcmBuffer));
    }
    AUDIO_UNLOCK();
}

void m4aSoundVSyncOn(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    u32 ident = soundInfo->ident;

    AUDIO_LOCK();
    if (ident == ID_NUMBER)
    {
        AUDIO_UNLOCK();
        return;
    }

    soundInfo->pcmDmaCounter = 0;
    soundInfo->ident = ident - 10;
    AUDIO_UNLOCK();
}

void m4aSoundVSync(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    AUDIO_LOCK();
    if (soundInfo->ident < ID_NUMBER || soundInfo->ident > ID_NUMBER + 1)
    {
        AUDIO_UNLOCK();
        return;
    }

    if (soundInfo->pcmDmaCounter)
        soundInfo->pcmDmaCounter--;
    else
        soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;
    AUDIO_UNLOCK();
}

void m4aSoundInit(void)
{
    s32 i;

    CpuCopy32((void *)((s32)SoundMainRAM & ~1), SoundMainRAM_Buffer, sizeof(SoundMainRAM_Buffer));

    SoundInit(&gSoundInfo);
    MPlayExtender(gCgbChans);
    m4aSoundMode(SOUND_MODE_DA_BIT_8
               | SOUND_MODE_FREQ_13379
               | (12 << SOUND_MODE_MASVOL_SHIFT)
               | (5 << SOUND_MODE_MAXCHN_SHIFT));

    for (i = 0; i < NUM_MUSIC_PLAYERS; i++)
    {
        struct MusicPlayerInfo *mplayInfo = gMPlayTable[i].info;
        MPlayOpen(mplayInfo, gMPlayTable[i].track, gMPlayTable[i].numTracks);
        mplayInfo->unk_B = gMPlayTable[i].unk_A;
        mplayInfo->memAccArea = gMPlayMemAccArea;
    }

    memcpy(&gPokemonCrySong, &gPokemonCrySongTemplate, sizeof(struct PokemonCrySong));

    for (i = 0; i < MAX_POKEMON_CRIES; i++)
    {
        struct MusicPlayerInfo *mplayInfo = &gPokemonCryMusicPlayers[i];
        struct MusicPlayerTrack *track = &gPokemonCryTracks[i * 2];
        MPlayOpen(mplayInfo, track, 2);
        track->chan = 0;
    }

#ifdef USE_SDL
    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = gSoundInfo.pcmFreq;
    want.format = sAudioFormat;
    want.channels = 2;
    want.samples = gSoundInfo.pcmSamplesPerVBlank;
    want.callback = SdlAudioCallback;

    sAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (sAudioDevice == 0)
    {
        SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    SDL_PauseAudioDevice(sAudioDevice, 0);
#endif
}

void m4aSoundMain(void)
{
#if defined(USE_SDL)
    // When an SDL audio device is active, SoundMain is driven by the
    // SdlAudioCallback. If the device failed to open, fall back to
    // manually pumping SoundMain here.
    if (sAudioDevice == 0)
#endif
    {
        struct SoundInfo *soundInfo = SOUND_INFO_PTR;

        AUDIO_LOCK();
        SoundMain();

        if (soundInfo->pcmDmaCounter)
            soundInfo->pcmDmaCounter--;
        else
            soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;

        AUDIO_UNLOCK();
    }
}

void m4aSoundMode(u32 mode)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    u32 temp;

    AUDIO_LOCK();
    if (soundInfo->ident != ID_NUMBER)
    {
        AUDIO_UNLOCK();
        return;
    }

    soundInfo->ident++;

    temp = mode & (SOUND_MODE_REVERB_SET | SOUND_MODE_REVERB_VAL);

    if (temp)
        soundInfo->reverb = temp & SOUND_MODE_REVERB_VAL;

    temp = mode & SOUND_MODE_MAXCHN;

    if (temp)
    {
        struct SoundChannel *chan;

        soundInfo->maxChans = temp >> SOUND_MODE_MAXCHN_SHIFT;

        temp = MAX_DIRECTSOUND_CHANNELS;
        chan = &soundInfo->chans[0];

        while (temp != 0)
        {
            chan->statusFlags = 0;
            temp--;
            chan++;
        }
    }

    temp = mode & SOUND_MODE_MASVOL;

    if (temp)
        soundInfo->masterVolume = temp >> SOUND_MODE_MASVOL_SHIFT;

    temp = mode & SOUND_MODE_DA_BIT;

    if (temp)
    {
        u32 daBits = 17 - ((temp >> SOUND_MODE_DA_BIT_SHIFT) & 0xF);
#if defined(USE_SDL)
        sAudioFormat = (daBits == 16) ? AUDIO_S16SYS : AUDIO_S8;
        SDL_Log("m4aSoundMode: requested %u-bit audio", daBits);
#else
        printf("m4aSoundMode: requested %u-bit audio\n", daBits);
#endif
    }

    temp = mode & SOUND_MODE_FREQ;

    if (temp)
        SampleFreqSet(temp);

    soundInfo->ident = ID_NUMBER;
    AUDIO_UNLOCK();
}

void m4aSongNumStart(u16 n)
{
    const struct MusicPlayer *mplayTable = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[n];
    const struct MusicPlayer *mplay = &mplayTable[song->ms];

    MPlayStart(mplay->info, song->header);
}

void m4aSongNumStartOrChange(u16 n)
{
    const struct MusicPlayer *mplayTable = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[n];
    const struct MusicPlayer *mplay = &mplayTable[song->ms];

    if (mplay->info->songHeader != song->header)
    {
        MPlayStart(mplay->info, song->header);
    }
    else
    {
        if ((mplay->info->status & MUSICPLAYER_STATUS_TRACK) == 0
         || (mplay->info->status & MUSICPLAYER_STATUS_PAUSE))
        {
            MPlayStart(mplay->info, song->header);
        }
    }
}

void m4aSongNumStartOrContinue(u16 n)
{
    const struct MusicPlayer *mplayTable = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[n];
    const struct MusicPlayer *mplay = &mplayTable[song->ms];

    if (mplay->info->songHeader != song->header)
        MPlayStart(mplay->info, song->header);
    else if ((mplay->info->status & MUSICPLAYER_STATUS_TRACK) == 0)
        MPlayStart(mplay->info, song->header);
    else if (mplay->info->status & MUSICPLAYER_STATUS_PAUSE)
        MPlayContinue(mplay->info);
}

void m4aSongNumStop(u16 n)
{
    const struct MusicPlayer *mplayTable = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[n];
    const struct MusicPlayer *mplay = &mplayTable[song->ms];

    if (mplay->info->songHeader == song->header)
        m4aMPlayStop(mplay->info);
}

void m4aSongNumContinue(u16 n)
{
    const struct MusicPlayer *mplayTable = gMPlayTable;
    const struct Song *songTable = gSongTable;
    const struct Song *song = &songTable[n];
    const struct MusicPlayer *mplay = &mplayTable[song->ms];

    if (mplay->info->songHeader == song->header)
        MPlayContinue(mplay->info);
}

void m4aMPlayAllStop(void)
{
    s32 i;

    for (i = 0; i < NUM_MUSIC_PLAYERS; i++)
        m4aMPlayStop(gMPlayTable[i].info);

    for (i = 0; i < MAX_POKEMON_CRIES; i++)
        m4aMPlayStop(&gPokemonCryMusicPlayers[i]);
}

void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    MPlayContinue(mplayInfo);
}

void m4aMPlayAllContinue(void)
{
    s32 i;

    for (i = 0; i < NUM_MUSIC_PLAYERS; i++)
        MPlayContinue(gMPlayTable[i].info);

    for (i = 0; i < MAX_POKEMON_CRIES; i++)
        MPlayContinue(&gPokemonCryMusicPlayers[i]);
}

void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    MPlayFadeOut(mplayInfo, speed);
}

void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->ident++;
        mplayInfo->fadeOC = speed;
        mplayInfo->fadeOI = speed;
        mplayInfo->fadeOV = (64 << FADE_VOL_SHIFT) | TEMPORARY_FADE;
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->ident++;
        mplayInfo->fadeOC = speed;
        mplayInfo->fadeOI = speed;
        mplayInfo->fadeOV = (0 << FADE_VOL_SHIFT) | FADE_IN;
        mplayInfo->status &= ~MUSICPLAYER_STATUS_PAUSE;
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayImmInit(struct MusicPlayerInfo *mplayInfo)
{
    s32 trackCount = mplayInfo->trackCount;
    struct MusicPlayerTrack *track = mplayInfo->tracks;

    while (trackCount > 0)
    {
        if (track->flags & MPT_FLG_EXIST)
        {
            if (track->flags & MPT_FLG_START)
            {
                Clear64byte(track);
                track->flags = MPT_FLG_EXIST;
                track->bendRange = 2;
                track->volX = 64;
                track->lfoSpeed = 22;
                track->tone.type = 1;
            }
        }

        trackCount--;
        track++;
    }
}

void m4aMPlayStop(struct MusicPlayerInfo *mplayInfo)
{
    m4aMPlayImmInit(mplayInfo);
    mplayInfo->songHeader = 0;
    mplayInfo->status = 0;
}

void m4aMPlayTempoControl(struct MusicPlayerInfo *mplayInfo, u16 tempo)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        mplayInfo->ident++;
        mplayInfo->tempoD = tempo;
        mplayInfo->tempoU = 0;
        mplayInfo->tempoI = 0;
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayVolumeControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u16 volume)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        int i;

        mplayInfo->ident++;
        for (i = 0; trackBits != 0; track++, i++, trackBits >>= 1)
        {
            if (trackBits & 1)
                track->volX = volume;
        }
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayPitchControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, s16 pitch)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        int i;

        mplayInfo->ident++;
        for (i = 0; trackBits != 0; track++, i++, trackBits >>= 1)
        {
            if (trackBits & 1)
                track->pitch = pitch;
        }
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayPanpotControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, s8 pan)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        int i;

        mplayInfo->ident++;
        for (i = 0; trackBits != 0; track++, i++, trackBits >>= 1)
        {
            if (trackBits & 1)
                track->pan = pan;
        }
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayModDepthSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 modDepth)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        int i;

        mplayInfo->ident++;
        for (i = 0; trackBits != 0; track++, i++, trackBits >>= 1)
        {
            if (trackBits & 1)
                track->modDepth = modDepth;
        }
        mplayInfo->ident = ID_NUMBER;
    }
}

void m4aMPlayLFOSpeedSet(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u8 lfoSpeed)
{
    if (mplayInfo->ident == ID_NUMBER)
    {
        struct MusicPlayerTrack *track = mplayInfo->tracks;
        int i;

        mplayInfo->ident++;
        for (i = 0; trackBits != 0; track++, i++, trackBits >>= 1)
        {
            if (trackBits & 1)
                track->lfoSpeed = lfoSpeed;
        }
        mplayInfo->ident = ID_NUMBER;
    }
}

void DummyFunc(void) {}

struct MusicPlayerInfo *SetPokemonCryTone(struct ToneData *tone)
{
    u32 maxClock = 0;
    s32 maxClockIndex = 0;
    s32 i;
    struct MusicPlayerInfo *mplayInfo;

    for (i = 0; i < MAX_POKEMON_CRIES; i++)
    {
        struct MusicPlayerTrack *track = &gPokemonCryTracks[i * 2];

        if (!track->flags && (!track->chan || track->chan->track != track))
            goto start_song;

        if (maxClock < gPokemonCryMusicPlayers[i].clock)
        {
            maxClock = gPokemonCryMusicPlayers[i].clock;
            maxClockIndex = i;
        }
    }

    i = maxClockIndex;

start_song:
    mplayInfo = &gPokemonCryMusicPlayers[i];
    mplayInfo->ident++;

    gPokemonCrySongs[i] = gPokemonCrySong;

    gPokemonCrySongs[i].tone = tone;
    gPokemonCrySongs[i].part[0] = &gPokemonCrySongs[i].part0;
    gPokemonCrySongs[i].part[1] = &gPokemonCrySongs[i].part1;
    gPokemonCrySongs[i].gotoTarget = (u32)&gPokemonCrySongs[i].cont;

    mplayInfo->ident = ID_NUMBER;

    MPlayStart(mplayInfo, (struct SongHeader *)(&gPokemonCrySongs[i]));

    return mplayInfo;
}

void SetPokemonCryVolume(u8 val)
{
    gPokemonCrySong.volumeValue = val & 0x7F;
}

void SetPokemonCryPanpot(s8 val)
{
    gPokemonCrySong.panValue = (val + C_V) & 0x7F;
}

void SetPokemonCryPitch(s16 val)
{
    gPokemonCrySong.tuneValue2 = (val / 2) & 0xFF;
}

void SetPokemonCryLength(u16 val)
{
    gPokemonCrySong.tieKeyValue = val & 0x3FF;
}

void SetPokemonCryRelease(u8 val)
{
    gPokemonCrySong.releaseValue = val & 0x7F;
}

void SetPokemonCryProgress(u32 val)
{
    gPokemonCrySong.gotoTarget += val;
}

void SetPokemonCryChorus(s8 val)
{
    gPokemonCrySong.unkCmd0DParam = (val << 8) & 0xFF00;
}

void SetPokemonCryStereo(u32 val)
{
    gPokemonCrySong.cont[0] = val;
}

void SetPokemonCryPriority(u8 val)
{
    gPokemonCrySong.priority = val;
}

#endif // PLATFORM_PC

#include "m4a.h"
#include <string.h>
#include <math.h>

#ifdef PLATFORM_PC

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

struct MusicPlayerInfo gMPlayInfo_BGM;
struct MusicPlayerInfo gMPlayInfo_SE1;
struct MusicPlayerInfo gMPlayInfo_SE2;
struct MusicPlayerInfo gMPlayInfo_SE3;
struct SoundInfo gSoundInfo;
struct PokemonCrySong gPokemonCrySongs[MAX_POKEMON_CRIES];
struct PokemonCrySong gPokemonCrySong;
struct MusicPlayerInfo gPokemonCryMusicPlayers[MAX_POKEMON_CRIES];
struct MusicPlayerTrack gPokemonCryTracks[MAX_POKEMON_CRIES * 2];

static ma_device sDevice;
static float sFrequency;
static float sAmplitude;
static float sTargetAmplitude;
static float sPhase;
static int sPlaying;

static void AudioCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    int16_t *out = pOutput;
    (void)pInput;

    for (ma_uint32 i = 0; i < frameCount; i++)
    {
        float sample = 0.0f;

        if (sPlaying && sFrequency > 0.0f)
        {
            sample = sAmplitude * sinf(2.0f * (float)M_PI * sPhase);
            sPhase += sFrequency / (float)pDevice->sampleRate;
            if (sPhase >= 1.0f)
                sPhase -= 1.0f;
        }

        int16_t val = (int16_t)(sample * 32767.0f);
        out[i * 2 + 0] = val;
        out[i * 2 + 1] = val;
    }
}

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

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate        = 44100;
    config.dataCallback      = AudioCallback;

    if (ma_device_init(NULL, &config, &sDevice) == MA_SUCCESS)
        ma_device_start(&sDevice);

    sFrequency = 0.0f;
    sAmplitude = 0.0f;
    sTargetAmplitude = 0.0f;
    sPhase = 0.0f;
    sPlaying = 0;
}

void m4aSoundMain(void)
{
    if (sAmplitude < sTargetAmplitude)
    {
        sAmplitude += 0.01f;
        if (sAmplitude > sTargetAmplitude)
            sAmplitude = sTargetAmplitude;
    }
    else if (sAmplitude > sTargetAmplitude)
    {
        sAmplitude -= 0.01f;
        if (sAmplitude < sTargetAmplitude)
        {
            sAmplitude = sTargetAmplitude;
            if (sAmplitude == 0.0f)
                sPlaying = 0;
        }
    }
}

static void StartSong(struct MusicPlayerInfo *mplayInfo, u16 n)
{
    (void)n;
    mplayInfo->status |= MUSICPLAYER_STATUS_TRACK;
    sFrequency = 440.0f;
    sTargetAmplitude = 0.25f;
    sPlaying = 1;
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
    sTargetAmplitude = 0.0f;
}

void m4aMPlayAllStop(void)
{
    m4aSongNumStop(0);
    gMPlayInfo_SE1.status &= ~MUSICPLAYER_STATUS_TRACK;
    gMPlayInfo_SE2.status &= ~MUSICPLAYER_STATUS_TRACK;
    gMPlayInfo_SE3.status &= ~MUSICPLAYER_STATUS_TRACK;
    sTargetAmplitude = 0.0f;
}

void m4aMPlayStop(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo)
        mplayInfo->status &= ~MUSICPLAYER_STATUS_TRACK;
    sTargetAmplitude = 0.0f;
}

void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo)
        mplayInfo->status |= MUSICPLAYER_STATUS_TRACK;
    sTargetAmplitude = 0.25f;
    sPlaying = 1;
}

void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)mplayInfo;
    (void)speed;
    sTargetAmplitude = 0.0f;
}

void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)mplayInfo;
    (void)speed;
    sTargetAmplitude = 0.0f;
}

void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed)
{
    (void)mplayInfo;
    (void)speed;
    sTargetAmplitude = 0.25f;
    sPlaying = 1;
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
    sFrequency = 880.0f;
    sTargetAmplitude = 0.25f;
    sPlaying = 1;
    return &gPokemonCryMusicPlayers[0];
}

bool32 IsPokemonCryPlaying(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo && (mplayInfo->status & MUSICPLAYER_STATUS_TRACK) && sPlaying)
        return TRUE;
    return FALSE;
}

#endif // PLATFORM_PC


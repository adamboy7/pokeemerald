#include "global.h"
#include "gba/m4a_internal.h"
#include <string.h>

#if PLATFORM_PC
// When building the game for PC we need concrete storage for a few of
// the symbols that are normally resolved by the GBA linker script. The
// music player count and max lines are simple scalar values instead of
// linker constants, so expose them as regular variables.
u16 gNumMusicPlayers = 4;
u32 gMaxLines = 0;
char SoundMainRAM[0x800];

// Zero the jump table.  MPlayExtender (pc_audio.c) fills in the non-null
// entries afterwards, so starting from zero is correct.
void MPlayJumpTableCopy(MPlayFunc *mplayJumpTable)
{
    memset(mplayJumpTable, 0, sizeof(MPlayFunc) * 36);
}

// Register the music player and its track array with the sound engine.
// Inserts mplayInfo at the head of soundInfo->musicPlayerHead so that
// SoundMain can iterate over all active players.
void MPlayOpen(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *tracks, u8 trackCount)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    memset(mplayInfo, 0, sizeof(*mplayInfo));
    mplayInfo->tracks     = tracks;
    mplayInfo->trackCount = trackCount;
    mplayInfo->ident      = ID_NUMBER;

    // Insert at the head of the linked list.
    mplayInfo->musicPlayerNext    = soundInfo->musicPlayerHead;
    soundInfo->musicPlayerHead    = mplayInfo;
}

// Clear all channels owned by a track.
void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    if (!(track->flags & MPT_FLG_EXIST))
        return;

    // Walk the channel chain and silence each channel.
    struct SoundChannel *chan = track->chan;
    while (chan != NULL)
    {
        struct SoundChannel *next = (struct SoundChannel *)chan->nextChannelPointer;
        chan->statusFlags = 0;
        chan = next;
    }
    track->chan  = NULL;
    track->flags = 0;
}

// Initialise a music player to begin playing songHeader.
// Tracks are set up with their command pointers but actual sequencing
// requires SoundMain to process them — for now this gives the game a
// consistent view of player state (started, not paused, correct priority).
void MPlayStart(struct MusicPlayerInfo *mplayInfo, struct SongHeader *songHeader)
{
    if (mplayInfo->ident != ID_NUMBER)
        return;

    mplayInfo->ident++;

    // Silence any channels that the previous song was using.
    for (int i = 0; i < mplayInfo->trackCount; i++)
        TrackStop(mplayInfo, &mplayInfo->tracks[i]);

    mplayInfo->songHeader = songHeader;
    mplayInfo->priority   = songHeader->priority;
    mplayInfo->tone       = songHeader->tone;
    mplayInfo->status     = MUSICPLAYER_STATUS_TRACK;

    u8 nTracks = songHeader->trackCount;
    if (nTracks > mplayInfo->trackCount)
        nTracks = mplayInfo->trackCount;

    for (int i = 0; i < nTracks; i++)
    {
        struct MusicPlayerTrack *trk = &mplayInfo->tracks[i];
        memset(trk, 0, sizeof(*trk));
        trk->flags       = MPT_FLG_EXIST | MPT_FLG_START;
        trk->bendRange   = 2;
        trk->volX        = 64;
        trk->lfoSpeed    = 22;
        trk->cmdPtr      = songHeader->part[i];
    }

    // Clear any leftover tracks beyond what the new song uses.
    for (int i = nTracks; i < mplayInfo->trackCount; i++)
        mplayInfo->tracks[i].flags = 0;

    mplayInfo->tempoD = 150;
    mplayInfo->tempoU = 0;
    mplayInfo->fadeOC = 0;
    mplayInfo->fadeOI = 0;
    mplayInfo->fadeOV = (u16)(FADE_VOL_MAX << FADE_VOL_SHIFT);

    mplayInfo->ident = ID_NUMBER;
}

// Advance the fade-out volume one step toward silence (or full volume for
// FADE_IN).  Called once per fade interval from SoundMain.
void FadeOutBody(struct MusicPlayerInfo *mplayInfo)
{
    u16 fadeVol = mplayInfo->fadeOV;
    u16 step    = (u16)(1 << FADE_VOL_SHIFT);

    if (fadeVol & FADE_IN)
    {
        fadeVol = (u16)(fadeVol + step);
        if ((fadeVol >> FADE_VOL_SHIFT) >= FADE_VOL_MAX)
        {
            // Fade-in complete: clamp and stop the fade counter.
            fadeVol        = (u16)(FADE_VOL_MAX << FADE_VOL_SHIFT);
            mplayInfo->fadeOC = 0;
        }
    }
    else
    {
        if ((fadeVol >> FADE_VOL_SHIFT) == 0)
        {
            // Reached silence.
            if (fadeVol & TEMPORARY_FADE)
                mplayInfo->status |= MUSICPLAYER_STATUS_PAUSE;
            else
                m4aMPlayStop(mplayInfo);
            mplayInfo->fadeOV = (u16)(FADE_VOL_MAX << FADE_VOL_SHIFT);
            mplayInfo->fadeOC = 0;
            return;
        }
        fadeVol = (u16)(fadeVol - step);
    }
    mplayInfo->fadeOV = fadeVol;
}

// No channel volume/pitch adjustment needed when there is no mixer.
void TrkVolPitSet(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    (void)track;
}

// Main audio tick: advance the fade state machine and mix all active
// DirectSound channels into soundInfo->pcmBuffer.
//
// Step derivation: ch->frequency is in units of sampleRate * 2^32 / cpuFreq
// (cpuFreq = 16,777,216 Hz).  Source samples per output sample:
//   step_float = ch->frequency * cpuFreq / 2^32 / pcmFreq
//              = ch->frequency * 16,777,216 / 4,294,967,296 / pcmFreq
//              = ch->frequency / (256 * pcmFreq)
// In 16.16 fixed-point: step_16_16 = ch->frequency * 256 / pcmFreq.
// fw accumulates step_16_16 each output sample; the integer part (fw >> 16)
// gives the number of source samples to advance, and the fractional part
// (fw & 0xFFFF) carries over to the next output sample.
void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (soundInfo->ident < ID_NUMBER || soundInfo->ident > ID_NUMBER + 1)
        return;

    // --- Fade state machine ---
    struct MusicPlayerInfo *mplayInfo = soundInfo->musicPlayerHead;
    while (mplayInfo != NULL)
    {
        if (mplayInfo->ident == ID_NUMBER &&
            (mplayInfo->status & MUSICPLAYER_STATUS_TRACK) &&
            !(mplayInfo->status & MUSICPLAYER_STATUS_PAUSE) &&
            mplayInfo->fadeOC != 0)
        {
            mplayInfo->fadeOC--;
            if (mplayInfo->fadeOC == 0)
            {
                mplayInfo->fadeOC = mplayInfo->fadeOI;
                FadeOutBody(mplayInfo);
            }
        }
        mplayInfo = mplayInfo->musicPlayerNext;
    }

    // --- DirectSound PCM mixer ---
    int nSamples = soundInfo->pcmSamplesPerVBlank;
    if (nSamples <= 0 || nSamples > PCM_DMA_BUF_SIZE)
        return;

    s8 *bufL = soundInfo->pcmBuffer;
    s8 *bufR = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE;

    // Use a s16 accumulation buffer to handle multiple channels without clipping.
    // Stack allocation is fine: PCM_DMA_BUF_SIZE = 1584, so 2 * 1584 * 2 = 6336 bytes.
    static s16 mixL[PCM_DMA_BUF_SIZE];
    static s16 mixR[PCM_DMA_BUF_SIZE];
    memset(mixL, 0, nSamples * sizeof(s16));
    memset(mixR, 0, nSamples * sizeof(s16));

    u32 pcmFreq = (u32)soundInfo->pcmFreq;
    if (pcmFreq == 0)
        pcmFreq = 13379;

    for (int ci = 0; ci < soundInfo->maxChans; ci++)
    {
        struct SoundChannel *ch = &soundInfo->chans[ci];

        // Channel is active when SF_START is set and not SF_STOP.
        if (!(ch->statusFlags & SOUND_CHANNEL_SF_START))
            continue;
        if (ch->statusFlags & SOUND_CHANNEL_SF_STOP)
            continue;
        if (ch->wav == NULL || ch->currentPointer == NULL)
            continue;

        struct WaveData *wav = ch->wav;
        // Loop flag is bit 14 of wav->type in AGB format.
        int loopEnabled = (wav->type & 0xC000) != 0;

        // 16.16 fixed-point step: source samples per output sample.
        u32 step = (u32)(((u64)ch->frequency * 256) / pcmFreq);
        u32 fw   = ch->fw;

        u8 volL = ch->envelopeVolumeLeft;
        u8 volR = ch->envelopeVolumeRight;

        // Apply master volume (0–15 → scale by masterVolume/16).
        volL = (u8)((volL * soundInfo->masterVolume) >> 4);
        volR = (u8)((volR * soundInfo->masterVolume) >> 4);

        s8 *end = wav->data + wav->loopStart + wav->size;

        for (int s = 0; s < nSamples; s++)
        {
            // Advance fractional position.
            fw += step;
            u32 advance = fw >> 16;
            fw &= 0xFFFF;

            if (advance > 0)
            {
                ch->currentPointer += advance;

                // Handle end-of-wave.
                while (ch->currentPointer >= end)
                {
                    if (loopEnabled)
                    {
                        // Wrap back to loop start.
                        u32 overshoot = (u32)(ch->currentPointer - end);
                        ch->currentPointer = wav->data + wav->loopStart + overshoot % wav->size;
                    }
                    else
                    {
                        // Non-looping sample finished.
                        ch->statusFlags &= ~SOUND_CHANNEL_SF_START;
                        ch->statusFlags |= SOUND_CHANNEL_SF_STOP;
                        goto next_channel;
                    }
                }
            }

            s32 sample = (s32)*ch->currentPointer;
            mixL[s] += (s16)((sample * (s32)volL) >> 8);
            mixR[s] += (s16)((sample * (s32)volR) >> 8);
        }
        next_channel:
        ch->fw = fw;
    }

    // Clamp and write to the PCM buffer consumed by SdlAudioCallback.
    for (int s = 0; s < nSamples; s++)
    {
        s32 l = mixL[s], r = mixR[s];
        bufL[s] = (s8)(l >  127 ?  127 : l < -128 ? -128 : l);
        bufR[s] = (s8)(r >  127 ?  127 : r < -128 ? -128 : r);
    }
}
void Clear64byte(void *addr) { memset(addr, 0, 64); }
void CgbSound(void) {}
void CgbOscOff(u8 ch) {}
u32 MidiKeyToCgbFreq(u8 a, u8 b, u8 c) { return 0; }
void ply_memacc(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xcmd(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_note(u32 note_cmd, struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xxx(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xwave(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xtype(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xatta(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xdeca(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xsust(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xrele(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xiecv(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xiecl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xleng(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xswee(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xcmd_0C(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void ply_xcmd_0D(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
bool32 IsPokemonCryPlaying(struct MusicPlayerInfo *mplayInfo) { return FALSE; }
u32 umul3232H32(u32 multiplier, u32 multiplicand) { return (u32)(((u64)multiplier * multiplicand) >> 32); }
#endif

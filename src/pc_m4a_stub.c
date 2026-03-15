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

// Main audio tick.  Without a PCM mixer this only advances the fade state
// machine so that MPlayFadeOut / MPlayFadeIn work correctly from game code.
void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (soundInfo->ident < ID_NUMBER || soundInfo->ident > ID_NUMBER + 1)
        return;

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

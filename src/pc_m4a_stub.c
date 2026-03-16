#include "global.h"
#include "gba/m4a_internal.h"
#include <string.h>

#if PLATFORM_PC

extern const u8 gClockTable[];
extern const u8 gScaleTable[];
extern const u32 gFreqTable[];
extern const XcmdFunc gXcmdTable[];

// Storage for symbols normally provided by the GBA linker script.
u16 gNumMusicPlayers = 4;
u32 gMaxLines = 0;
char SoundMainRAM[0x800];

// ============================================================
// Basic utility
// ============================================================

u32 umul3232H32(u32 multiplier, u32 multiplicand)
{
    return (u32)(((u64)multiplier * multiplicand) >> 32);
}

void Clear64byte(void *addr)
{
    memset(addr, 0, 64);
}

u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 fineAdjust)
{
    u32 fineAdjustShifted = (u32)fineAdjust << 24;

    if (key > 178)
    {
        key = 178;
        fineAdjustShifted = 255u << 24;
    }

    u32 val1 = gScaleTable[key];
    val1 = gFreqTable[val1 & 0xF] >> (val1 >> 4);

    u32 val2 = gScaleTable[key + 1];
    val2 = gFreqTable[val2 & 0xF] >> (val2 >> 4);

    return umul3232H32(wav->freq, val1 + umul3232H32(val2 - val1, fineAdjustShifted));
}

// ============================================================
// CGB stubs (not used for DirectSound music on PC)
// ============================================================

void CgbSound(void) {}
void CgbOscOff(u8 ch) { (void)ch; }
u32 MidiKeyToCgbFreq(u8 a, u8 b, u8 c) { (void)a; (void)b; (void)c; return 0; }

// ============================================================
// Channel linked-list management
// ============================================================

void RealClearChain(void *x)
{
    struct SoundChannel *ch = (struct SoundChannel *)x;
    struct MusicPlayerTrack *track = ch->track;
    if (track == NULL)
        return;

    struct SoundChannel *next = (struct SoundChannel *)ch->nextChannelPointer;
    struct SoundChannel *prev = (struct SoundChannel *)ch->prevChannelPointer;

    if (prev == NULL)
        track->chan = next;
    else
        prev->nextChannelPointer = next;

    if (next != NULL)
        next->prevChannelPointer = prev;

    ch->track = NULL;
}

// ============================================================
// Internal helpers
// ============================================================

void ClearModM(struct MusicPlayerTrack *track)
{
    track->modM = 0;
    track->lfoSpeedC = 0;
    if (track->modT == 0)
        track->flags |= MPT_FLG_PITCHG;
    else
        track->flags |= MPT_FLG_VOLCHG;
}

static void ChnVolSet(struct MusicPlayerTrack *track, struct SoundChannel *ch)
{
    s8 rpan = (s8)ch->rhythmPan;
    u8 vel  = ch->velocity;

    s32 r = ((s32)(0x80 + rpan) * (s32)vel * (s32)track->volMR) >> 14;
    if (r > 0xFF) r = 0xFF;
    ch->rightVolume = (u8)r;

    s32 l = ((s32)(0x7F - rpan) * (s32)vel * (s32)track->volML) >> 14;
    if (l > 0xFF) l = 0xFF;
    ch->leftVolume = (u8)l;

    // PC mixer reads envelopeVolumeRight/Left directly.
    ch->envelopeVolumeRight = ch->rightVolume;
    ch->envelopeVolumeLeft  = ch->leftVolume;
}

// ============================================================
// TrackStop
// ============================================================

void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    if (!(track->flags & MPT_FLG_EXIST))
        return;

    struct SoundChannel *ch = track->chan;
    while (ch != NULL)
    {
        struct SoundChannel *next = (struct SoundChannel *)ch->nextChannelPointer;
        if (ch->statusFlags != 0)
            ch->statusFlags = 0;
        ch->track = NULL;
        ch = next;
    }
    track->chan = NULL;
}

// ============================================================
// TrkVolPitSet  (ported from m4a.c)
// ============================================================

void TrkVolPitSet(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;

    if (track->flags & MPT_FLG_VOLSET)
    {
        s32 x = (s32)((u32)(track->vol * track->volX) >> 5);

        if (track->modT == 1)
            x = (s32)((u32)(x * (track->modM + 128)) >> 7);

        s32 y = 2 * track->pan + track->panX;

        if (track->modT == 2)
            y += track->modM;

        if (y < -128)      y = -128;
        else if (y > 127)  y = 127;

        track->volMR = (u8)((u32)((y + 128) * x) >> 8);
        track->volML = (u8)((u32)((127 - y) * x) >> 8);
    }

    if (track->flags & MPT_FLG_PITSET)
    {
        s32 bend = (s32)track->bend * (s32)track->bendRange;
        s32 x = ((s32)track->tune + bend) * 4
              + ((s32)(s8)track->keyShift  << 8)
              + ((s32)(s8)track->keyShiftX << 8)
              + (s32)track->pitX;

        if (track->modT == 0)
            x += 16 * (s32)track->modM;

        track->keyM = (u8)(x >> 8);
        track->pitM = (u8)x;
    }

    track->flags &= (u8)~(MPT_FLG_PITSET | MPT_FLG_VOLSET);
}

// ============================================================
// FadeOutBody  (ported from m4a.c)
// ============================================================

void FadeOutBody(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo->fadeOI == 0)
        return;
    if (--mplayInfo->fadeOC != 0)
        return;

    mplayInfo->fadeOC = mplayInfo->fadeOI;

    if (mplayInfo->fadeOV & FADE_IN)
    {
        if ((u16)(mplayInfo->fadeOV += (4 << FADE_VOL_SHIFT)) >= (u16)(FADE_VOL_MAX << FADE_VOL_SHIFT))
        {
            mplayInfo->fadeOV = (u16)(FADE_VOL_MAX << FADE_VOL_SHIFT);
            mplayInfo->fadeOI = 0;
        }
    }
    else
    {
        if ((s16)(mplayInfo->fadeOV -= (4 << FADE_VOL_SHIFT)) <= 0)
        {
            s32 i = mplayInfo->trackCount;
            struct MusicPlayerTrack *track = mplayInfo->tracks;
            u16 fadeOV = mplayInfo->fadeOV;

            while (i > 0)
            {
                TrackStop(mplayInfo, track);
                if (!(fadeOV & TEMPORARY_FADE))
                    track->flags = 0;
                i--;
                track++;
            }

            if (mplayInfo->fadeOV & TEMPORARY_FADE)
                mplayInfo->status |= MUSICPLAYER_STATUS_PAUSE;
            else
                mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;

            mplayInfo->fadeOI = 0;
            return;
        }
    }

    s32 i = mplayInfo->trackCount;
    struct MusicPlayerTrack *track = mplayInfo->tracks;
    while (i > 0)
    {
        if (track->flags & MPT_FLG_EXIST)
        {
            track->volX = (u8)(mplayInfo->fadeOV >> FADE_VOL_SHIFT);
            track->flags |= MPT_FLG_VOLCHG;
        }
        i--;
        track++;
    }
}

// ============================================================
// Jump table command handlers
// ============================================================

void ply_fine(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    struct SoundChannel *ch = track->chan;
    while (ch != NULL)
    {
        if (ch->statusFlags & SOUND_CHANNEL_SF_ON)
            ch->statusFlags |= SOUND_CHANNEL_SF_STOP;
        struct SoundChannel *next = (struct SoundChannel *)ch->nextChannelPointer;
        RealClearChain(ch);
        ch = next;
    }
    track->flags = 0;
}

void ply_goto(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 *p = track->cmdPtr;
    u32 target = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
    track->cmdPtr = (u8 *)target;
}

void ply_patt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 level = track->patternLevel;
    if (level >= 3)
    {
        ply_fine(mplayInfo, track);
        return;
    }
    track->patternStack[level] = track->cmdPtr + 4;
    track->patternLevel++;
    ply_goto(mplayInfo, track);
}

void ply_pend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 level = track->patternLevel;
    if (level == 0)
        return;
    track->patternLevel = --level;
    track->cmdPtr = track->patternStack[level];
}

void ply_rept(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 *p = track->cmdPtr;
    u8 count = p[0];

    if (count == 0)
    {
        // Infinite loop: advance past count byte and jump.
        track->cmdPtr = p + 1;
        ply_goto(mplayInfo, track);
        return;
    }

    track->repN++;
    if (track->repN >= count)
    {
        // Loop complete: advance past count + 4-byte address.
        track->repN = 0;
        track->cmdPtr = p + 5;
    }
    else
    {
        // Jump back to loop target.
        track->cmdPtr = p + 1;
        ply_goto(mplayInfo, track);
    }
}

void ply_prio(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->priority = *track->cmdPtr++;
}

void ply_tempo(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 val = *track->cmdPtr++;
    mplayInfo->tempoD = (u16)(val * 2);
    mplayInfo->tempoI = (u16)((u32)mplayInfo->tempoD * mplayInfo->tempoU >> 8);
}

void ply_keysh(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->keyShift = (s8)*track->cmdPtr++;
    track->flags |= MPT_FLG_PITCHG;
}

void ply_voice(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 idx = *track->cmdPtr++;
    struct ToneData *td = &mplayInfo->tone[idx];
    track->tone.type      = td->type;
    track->tone.key       = td->key;
    track->tone.length    = td->length;
    track->tone.pan_sweep = td->pan_sweep;
    track->tone.wav       = td->wav;
    track->tone.attack    = td->attack;
    track->tone.decay     = td->decay;
    track->tone.sustain   = td->sustain;
    track->tone.release   = td->release;
}

void ply_vol(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->vol = *track->cmdPtr++;
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_pan(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->pan = (s8)(*track->cmdPtr++ - C_V);
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_bend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bend = (s8)(*track->cmdPtr++ - C_V);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_bendr(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bendRange = *track->cmdPtr++;
    track->flags |= MPT_FLG_PITCHG;
}

void ply_lfodl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->lfoDelay = *track->cmdPtr++;
}

void ply_modt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 val = *track->cmdPtr++;
    if (track->modT != val)
    {
        track->modT = val;
        track->flags |= MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
    }
}

void ply_tune(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tune = (s8)(*track->cmdPtr++ - C_V);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_port(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    // Writes to GBA hardware registers - no-op on PC, just advance past the 2 parameter bytes.
    (void)mplayInfo;
    track->cmdPtr += 2;
}

// Required by MPlayExtender and gXcmdTable:
void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 val = *track->cmdPtr++;
    track->lfoSpeed = val;
    if (val == 0)
        ClearModM(track);
}

void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 val = *track->cmdPtr++;
    track->mod = val;
    if (val == 0)
        ClearModM(track);
}

void ply_memacc(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 op   = *track->cmdPtr++;
    u8 *addr = mplayInfo->memAccArea + *track->cmdPtr++;
    u8 data  = *track->cmdPtr++;

    bool32 cond = FALSE;
    bool32 do_cond = FALSE;

    switch (op)
    {
    case 0:  *addr = data; return;
    case 1:  *addr += data; return;
    case 2:  *addr -= data; return;
    case 3:  *addr = mplayInfo->memAccArea[data]; return;
    case 4:  *addr += mplayInfo->memAccArea[data]; return;
    case 5:  *addr -= mplayInfo->memAccArea[data]; return;
    case 6:  cond = (*addr == data); do_cond = TRUE; break;
    case 7:  cond = (*addr != data); do_cond = TRUE; break;
    case 8:  cond = (*addr >  data); do_cond = TRUE; break;
    case 9:  cond = (*addr >= data); do_cond = TRUE; break;
    case 10: cond = (*addr <= data); do_cond = TRUE; break;
    case 11: cond = (*addr <  data); do_cond = TRUE; break;
    case 12: cond = (*addr == mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    case 13: cond = (*addr != mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    case 14: cond = (*addr >  mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    case 15: cond = (*addr >= mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    case 16: cond = (*addr <= mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    case 17: cond = (*addr <  mplayInfo->memAccArea[data]); do_cond = TRUE; break;
    default: return;
    }

    if (do_cond)
    {
        if (cond)
            // Index 1 in the jump table = ply_goto
            ((void(*)(struct MusicPlayerInfo*, struct MusicPlayerTrack*))gMPlayJumpTable[1])(mplayInfo, track);
        else
            track->cmdPtr += 4; // skip the 4-byte conditional jump target
    }
}

void ply_xcmd(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u32 n = *track->cmdPtr++;
    gXcmdTable[n](mplayInfo, track);
}

void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u8 key;
    if (*track->cmdPtr < 0x80)
    {
        key = *track->cmdPtr++;
        track->key = key;
    }
    else
    {
        key = track->key;
    }

    struct SoundChannel *ch = track->chan;
    while (ch != NULL)
    {
        if ((ch->statusFlags & (SOUND_CHANNEL_SF_START | SOUND_CHANNEL_SF_ENV))
            && !(ch->statusFlags & SOUND_CHANNEL_SF_STOP)
            && ch->midiKey == key)
        {
            ch->statusFlags |= SOUND_CHANNEL_SF_STOP;
            break;
        }
        ch = (struct SoundChannel *)ch->nextChannelPointer;
    }
}

// ============================================================
// Extended command handlers (gXcmdTable entries)
// ============================================================

void ply_xxx(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    // Unknown xcmd: treat as ply_fine.
    ((void(*)(struct MusicPlayerInfo*, struct MusicPlayerTrack*))gMPlayJumpTable[0])(mplayInfo, track);
}

void ply_xwave(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u32 wav = (u32)track->cmdPtr[0]
            | ((u32)track->cmdPtr[1] << 8)
            | ((u32)track->cmdPtr[2] << 16)
            | ((u32)track->cmdPtr[3] << 24);
    track->tone.wav = (struct WaveData *)wav;
    track->cmdPtr += 4;
}

void ply_xtype(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.type = *track->cmdPtr++;
}

void ply_xatta(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.attack = *track->cmdPtr++;
}

void ply_xdeca(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.decay = *track->cmdPtr++;
}

void ply_xsust(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.sustain = *track->cmdPtr++;
}

void ply_xrele(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.release = *track->cmdPtr++;
}

void ply_xiecv(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->pseudoEchoVolume = *track->cmdPtr++;
}

void ply_xiecl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->pseudoEchoLength = *track->cmdPtr++;
}

void ply_xleng(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.length = *track->cmdPtr++;
}

void ply_xswee(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tone.pan_sweep = *track->cmdPtr++;
}

void ply_xwait(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    // cmdPtr points at the 2-byte length (ply_xcmd already advanced past opcode).
    u32 len = (u32)track->cmdPtr[0] | ((u32)track->cmdPtr[1] << 8);
    if (track->timer < (u16)len)
    {
        track->timer++;
        track->cmdPtr -= 2; // back up to the XCMD command byte so it re-runs next tick
        track->wait = 1;
    }
    else
    {
        track->timer = 0;
        track->cmdPtr += 2;
    }
}

void ply_xcmd_0D(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    u32 val = (u32)track->cmdPtr[0]
            | ((u32)track->cmdPtr[1] << 8)
            | ((u32)track->cmdPtr[2] << 16)
            | ((u32)track->cmdPtr[3] << 24);
    track->unk_3C = val;
    track->cmdPtr += 4;
}

// Alias kept for compatibility with any stub references.
void ply_xcmd_0C(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    ply_xwait(mplayInfo, track);
}

// ============================================================
// ply_note  —  allocate a DirectSound channel and start a note
// ============================================================

void ply_note(u32 note_cmd, struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    // gateTime from gClockTable, then read optional key/velocity/gate-extra bytes.
    track->gateTime = gClockTable[note_cmd];

    u8 *p = track->cmdPtr;
    if (p[0] < 0x80)
    {
        track->key = p[0];
        p++;
        if (p[0] < 0x80)
        {
            track->velocity = p[0];
            p++;
            if (p[0] < 0x80)
            {
                track->gateTime = (u8)(track->gateTime + p[0]);
                p++;
            }
        }
    }
    track->cmdPtr = p;

    // Resolve the ToneData pointer (handle SPL / RHY sub-tables).
    struct ToneData *tone = &track->tone;
    u8 toneType = tone->type;
    u8 noteKey  = track->key;
    s8 rhythmPan = 0;

    if (toneType & (TONEDATA_TYPE_SPL | TONEDATA_TYPE_RHY))
    {
        // SPL: key split table; RHY: direct index into sub-tone array.
        u8 subIdx;
        if (toneType & TONEDATA_TYPE_SPL)
        {
            u8 *splitTable = (u8 *)tone->wav;
            subIdx = splitTable[noteKey];
        }
        else
        {
            subIdx = noteKey;
        }

        struct ToneData *subToneArray = (struct ToneData *)tone->wav;
        struct ToneData *sub = &subToneArray[subIdx];

        // Reject doubly-nested SPL/RHY.
        if (sub->type & (TONEDATA_TYPE_SPL | TONEDATA_TYPE_RHY))
            return;

        if (toneType & TONEDATA_TYPE_RHY)
        {
            // RHY: fixed pitch from sub-tone key, optional rhythmPan from pan_sweep.
            if (sub->pan_sweep & 0x80)
                rhythmPan = (s8)((sub->pan_sweep - TONEDATA_P_S_PAN) << 1);
            noteKey = sub->key;
        }

        tone = sub;
    }

    // Compute combined priority.
    u32 priority = (u32)mplayInfo->priority + (u32)track->priority;
    if (priority > 0xFF) priority = 0xFF;

    u8 chanType = tone->type & TONEDATA_TYPE_CGB;

    // Find a channel to use.
    struct SoundChannel *ch = NULL;

    if (chanType)
    {
        // CGB channel: select by type index.
        if (soundInfo->cgbChans == NULL)
            return;
        ch = &((struct SoundChannel *)soundInfo->cgbChans)[chanType - 1];
    }
    else
    {
        // DirectSound: search soundInfo->chans[] for the best candidate.
        u8  bestPri  = (u8)priority;
        struct MusicPlayerTrack *bestTrack = track;
        int foundStopping = 0;
        struct SoundChannel *best = NULL;

        for (int ci = 0; ci < soundInfo->maxChans; ci++)
        {
            struct SoundChannel *c = &soundInfo->chans[ci];

            if (!(c->statusFlags & SOUND_CHANNEL_SF_ON))
            {
                // Idle channel — use immediately.
                best = c;
                goto use_channel;
            }

            if (c->statusFlags & SOUND_CHANNEL_SF_STOP)
            {
                // Stopping channel.
                if (!foundStopping)
                {
                    foundStopping = 1;
                    bestPri   = c->priority;
                    bestTrack = c->track;
                    best = c;
                }
                else
                {
                    if (c->priority < bestPri
                        || (c->priority == bestPri && (uintptr_t)c->track >= (uintptr_t)bestTrack))
                    {
                        bestPri   = c->priority;
                        bestTrack = c->track;
                        best = c;
                    }
                }
            }
            else if (!foundStopping)
            {
                // Active channel, only consider if no stopping candidate yet.
                if (c->priority < bestPri
                    || (c->priority == bestPri && (uintptr_t)c->track >= (uintptr_t)bestTrack))
                {
                    bestPri   = c->priority;
                    bestTrack = c->track;
                    best = c;
                }
            }
        }

        if (best == NULL)
            return;

    use_channel:
        ch = best;
    }

    // Unlink the channel from its previous owner and link to this track.
    RealClearChain(ch);
    ch->prevChannelPointer = NULL;
    ch->nextChannelPointer = track->chan;
    if (track->chan != NULL)
        track->chan->prevChannelPointer = ch;
    track->chan = ch;
    ch->track = track;

    // Reset LFO delay counter.
    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelay != 0)
        ClearModM(track);

    // Compute volume / pitch modifiers before setting channel fields.
    TrkVolPitSet(mplayInfo, track);

    // Populate channel fields (mirrors the 4-byte trick in the ARM asm).
    ch->gateTime = track->gateTime;
    ch->midiKey  = noteKey;            // key as used in track data
    ch->velocity = track->velocity;
    ch->priority = (u8)priority;

    ch->rhythmPan       = (u8)rhythmPan;
    ch->type            = tone->type;
    ch->wav             = tone->wav;
    ch->attack          = tone->attack;
    ch->decay           = tone->decay;
    ch->sustain         = tone->sustain;
    ch->release         = tone->release;
    ch->pseudoEchoVolume = track->pseudoEchoVolume;
    ch->pseudoEchoLength = track->pseudoEchoLength;

    ChnVolSet(track, ch);

    // Final MIDI key = note key + keyM offset.
    s32 finalKey = (s32)noteKey + (s8)track->keyM;
    if (finalKey < 0) finalKey = 0;

    ch->key        = (u8)finalKey;
    ch->count      = track->unk_3C;
    ch->frequency  = MidiKeyToFreq(ch->wav, (u8)finalKey, track->pitM);

    // PC mixer: initialise playback position.
    if (ch->wav != NULL)
        ch->currentPointer = ch->wav->data;
    ch->fw = 0;

    ch->statusFlags = SOUND_CHANNEL_SF_START;

    // Clear the lower nibble of track flags (vol/pitch change bits consumed).
    track->flags &= 0xF0;
}

// ============================================================
// MPlayMain  —  music sequencer tick
// ============================================================

void MPlayMain(struct MusicPlayerInfo *mplayInfo)
{
    if (mplayInfo->ident != ID_NUMBER)
        return;
    mplayInfo->ident++;

    // Recurse for the next player in the linked chain BEFORE processing this one.
    if (mplayInfo->MPlayMainNext != NULL)
        mplayInfo->MPlayMainNext(mplayInfo->musicPlayerNext);

    // Paused?
    if ((s32)mplayInfo->status < 0)
        goto done;

    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    FadeOutBody(mplayInfo);
    if ((s32)mplayInfo->status < 0)
        goto done;

    // Tempo accumulator loop: process one game-tick each time tempoC >= 150.
    u16 tempoC = mplayInfo->tempoC + mplayInfo->tempoI;
    mplayInfo->tempoC = tempoC;

    while (tempoC >= 150)
    {
        // ---------- Process all tracks for one music tick ----------
        u32 activeTracks = 0;

        for (int ti = 0; ti < mplayInfo->trackCount; ti++)
        {
            struct MusicPlayerTrack *track = &mplayInfo->tracks[ti];

            if (!(track->flags & MPT_FLG_EXIST))
                continue;

            activeTracks |= (1u << ti);

            // Walk channel chain: decrement gate time, stop channels at 0,
            // and clear dead channels.
            {
                struct SoundChannel *ch = track->chan;
                while (ch != NULL)
                {
                    struct SoundChannel *nextCh = (struct SoundChannel *)ch->nextChannelPointer;
                    if (ch->statusFlags & SOUND_CHANNEL_SF_ON)
                    {
                        if (ch->gateTime != 0 && --ch->gateTime == 0)
                            ch->statusFlags |= SOUND_CHANNEL_SF_STOP;
                    }
                    else
                    {
                        RealClearChain(ch);
                    }
                    ch = nextCh;
                }
            }

            // First-time track initialisation (MPT_FLG_START).
            if (track->flags & MPT_FLG_START)
            {
                // Clear64byte zeros offsets 0-63; cmdPtr (offset 64) is preserved.
                Clear64byte(track);
                track->flags     = MPT_FLG_EXIST;
                track->bendRange = 2;
                track->volX      = 0x40;
                track->lfoSpeed  = 22;
                track->tone.type = 1;
            }

            // Command decode loop: run commands while wait == 0.
            while (track->wait == 0)
            {
                u8 byte = *track->cmdPtr;
                u8 cmd;

                if (byte < 0x80)
                {
                    // Running status: reuse last explicit command byte.
                    cmd = track->runningStatus;
                }
                else
                {
                    track->cmdPtr++;
                    cmd = byte;
                    // Voice commands (>= 0xBD) update running status.
                    if (cmd >= 0xBD)
                        track->runningStatus = cmd;
                }

                if (cmd >= 0xCF)
                {
                    // Note command (0xCF = TIE, 0xD0+ = timed notes).
                    soundInfo->plynote(cmd - 0xCF, mplayInfo, track);
                }
                else if (cmd > 0xB0)
                {
                    // Jump-table command.
                    u8 idx = cmd - 0xB1;
                    mplayInfo->cmd = idx;
                    MPlayFunc fn = soundInfo->MPlayJumpTable[idx];
                    if (fn != NULL)
                        ((void(*)(struct MusicPlayerInfo*, struct MusicPlayerTrack*))fn)(mplayInfo, track);
                    // ply_fine (and similar) sets flags to 0 to signal track end.
                    if (track->flags == 0)
                        goto next_track;
                }
                else
                {
                    // Wait command: 0x80–0xB0.
                    track->wait = gClockTable[cmd - 0x80];
                }
            }

            // Consume one wait tick and run the LFO oscillator.
            track->wait--;

            if (track->lfoSpeed != 0 && track->mod != 0)
            {
                if (track->lfoDelayC != 0)
                {
                    track->lfoDelayC--;
                }
                else
                {
                    u8 lsc = (u8)(track->lfoSpeedC + track->lfoSpeed);
                    track->lfoSpeedC = lsc;

                    // Triangle wave: rise 0→63, fall 64→(-127).
                    s8 tri = (lsc < 0x40) ? (s8)lsc : (s8)(u8)(0x80 - lsc);
                    s8 newModM = (s8)(((s32)(u8)track->mod * (s32)tri) >> 6);

                    if (track->modM != newModM)
                    {
                        track->modM = newModM;
                        track->flags |= (track->modT == 0) ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
                    }
                }
            }

        next_track:;
        }

        // Update clock and status.
        mplayInfo->clock++;

        if (activeTracks == 0)
        {
            mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;
            goto done;
        }
        mplayInfo->status = activeTracks;

        tempoC -= 150;
        mplayInfo->tempoC = tempoC;
    }

    // ---------- Volume / pitch update pass ----------
    for (int ti = 0; ti < mplayInfo->trackCount; ti++)
    {
        struct MusicPlayerTrack *track = &mplayInfo->tracks[ti];

        // Only process tracks that exist and have pending vol/pitch changes.
        if (!(track->flags & MPT_FLG_EXIST))
            continue;
        if (!(track->flags & (MPT_FLG_VOLCHG | MPT_FLG_PITCHG)))
            continue;

        TrkVolPitSet(mplayInfo, track);

        struct SoundChannel *ch = track->chan;
        while (ch != NULL)
        {
            struct SoundChannel *nextCh = (struct SoundChannel *)ch->nextChannelPointer;

            if (!(ch->statusFlags & SOUND_CHANNEL_SF_ON))
            {
                RealClearChain(ch);
                ch = nextCh;
                continue;
            }

            u8 isCGB = ch->type & TONEDATA_TYPE_CGB;

            if (track->flags & MPT_FLG_VOLCHG)
            {
                ChnVolSet(track, ch);
                // envelopeVolumeRight/Left already updated inside ChnVolSet for PC.
                (void)isCGB;
            }

            if (track->flags & MPT_FLG_PITCHG)
            {
                if (!isCGB && ch->wav != NULL)
                {
                    s32 key = (s32)(u8)ch->key + (s8)track->keyM;
                    if (key < 0) key = 0;
                    ch->frequency = MidiKeyToFreq(ch->wav, (u8)key, track->pitM);
                }
            }

            ch = nextCh;
        }

        // Clear the lower nibble (vol/pitch change flags consumed).
        track->flags &= 0xF0;
    }

done:
    mplayInfo->ident = ID_NUMBER;
}

// ============================================================
// MPlayJumpTableCopy
// ============================================================

void MPlayJumpTableCopy(MPlayFunc *t)
{
    memset(t, 0, sizeof(MPlayFunc) * 36);

    // Entries overridden by MPlayExtender: 8, 17, 19, 28, 29, 30, 31, 32, 33.
#define SET(i, fn) t[i] = (MPlayFunc)(fn)
    SET(0,  ply_fine);
    SET(1,  ply_goto);
    SET(2,  ply_patt);
    SET(3,  ply_pend);
    SET(4,  ply_rept);
    SET(5,  ply_fine);
    SET(6,  ply_fine);
    SET(7,  ply_fine);
    // [8]  = ply_memacc  (MPlayExtender)
    SET(9,  ply_prio);
    SET(10, ply_tempo);
    SET(11, ply_keysh);
    SET(12, ply_voice);
    SET(13, ply_vol);
    SET(14, ply_pan);
    SET(15, ply_bend);
    SET(16, ply_bendr);
    // [17] = ply_lfos    (MPlayExtender)
    SET(18, ply_lfodl);
    // [19] = ply_mod     (MPlayExtender)
    SET(20, ply_modt);
    SET(21, ply_fine);
    SET(22, ply_fine);
    SET(23, ply_tune);
    SET(24, ply_fine);
    SET(25, ply_fine);
    SET(26, ply_fine);
    SET(27, ply_port);
    // [28] = ply_xcmd    (MPlayExtender)
    // [29] = ply_endtie  (MPlayExtender)
    // [30] = SampleFreqSet (MPlayExtender)
    // [31] = TrackStop   (MPlayExtender)
    // [32] = FadeOutBody (MPlayExtender)
    // [33] = TrkVolPitSet (MPlayExtender)
    SET(34, RealClearChain);
    SET(35, Clear64byte);
#undef SET
}

// ============================================================
// MPlayOpen  (fixed: sets up MPlayMainHead/MPlayMainNext chain)
// ============================================================

void MPlayOpen(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *tracks, u8 trackCount)
{
    if (trackCount == 0)
        return;
    if (trackCount > MAX_MUSICPLAYER_TRACKS)
        trackCount = MAX_MUSICPLAYER_TRACKS;

    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    if (soundInfo->ident != ID_NUMBER)
        return;

    soundInfo->ident++;

    Clear64byte(mplayInfo);

    mplayInfo->tracks     = tracks;
    mplayInfo->trackCount = trackCount;
    mplayInfo->status     = MUSICPLAYER_STATUS_PAUSE;

    for (int i = 0; i < trackCount; i++)
        tracks[i].flags = 0;

    // Link this player into the MPlayMain head chain.
    if (soundInfo->MPlayMainHead != NULL)
    {
        mplayInfo->MPlayMainNext = soundInfo->MPlayMainHead;
        mplayInfo->musicPlayerNext = soundInfo->musicPlayerHead;
        soundInfo->MPlayMainHead = NULL; // cleared before reassigning (matches GBA)
    }

    soundInfo->musicPlayerHead = mplayInfo;
    soundInfo->MPlayMainHead   = MPlayMain;

    soundInfo->ident    = ID_NUMBER;
    mplayInfo->ident    = ID_NUMBER;
}

// ============================================================
// MPlayStart  (fixed: set tempoI=150, tempoU=0x100)
// ============================================================

void MPlayStart(struct MusicPlayerInfo *mplayInfo, struct SongHeader *songHeader)
{
    if (mplayInfo->ident != ID_NUMBER)
        return;

    u8 unk_B = mplayInfo->unk_B;

    if (!unk_B
        || ((!mplayInfo->songHeader || !(mplayInfo->tracks[0].flags & MPT_FLG_START))
            && ((mplayInfo->status & MUSICPLAYER_STATUS_TRACK) == 0
                || (mplayInfo->status & MUSICPLAYER_STATUS_PAUSE)))
        || (mplayInfo->priority <= songHeader->priority))
    {
        mplayInfo->ident++;
        mplayInfo->status     = 0;
        mplayInfo->songHeader = songHeader;
        mplayInfo->tone       = songHeader->tone;
        mplayInfo->priority   = songHeader->priority;
        mplayInfo->clock      = 0;
        mplayInfo->tempoD     = 150;
        mplayInfo->tempoI     = 150;
        mplayInfo->tempoU     = 0x100;
        mplayInfo->tempoC     = 0;
        mplayInfo->fadeOI     = 0;

        int i = 0;
        struct MusicPlayerTrack *track = mplayInfo->tracks;

        while (i < songHeader->trackCount && i < mplayInfo->trackCount)
        {
            TrackStop(mplayInfo, track);
            track->flags  = MPT_FLG_EXIST | MPT_FLG_START;
            track->chan   = NULL;
            track->cmdPtr = songHeader->part[i];
            i++;
            track++;
        }
        while (i < mplayInfo->trackCount)
        {
            TrackStop(mplayInfo, track);
            track->flags = 0;
            i++;
            track++;
        }

        mplayInfo->ident = ID_NUMBER;
    }
}

// ============================================================
// SoundMain  —  called each audio frame from SdlAudioCallback
// ============================================================

void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (soundInfo->ident < ID_NUMBER || soundInfo->ident > ID_NUMBER + 1)
        return;

    // Advance the music sequencer (all players via the MPlayMainHead chain).
    if (soundInfo->MPlayMainHead != NULL)
        soundInfo->MPlayMainHead(soundInfo->musicPlayerHead);

    // ---------- DirectSound PCM mixer ----------
    int nSamples = soundInfo->pcmSamplesPerVBlank;
    if (nSamples <= 0 || nSamples > PCM_DMA_BUF_SIZE)
        return;

    s8 *bufL = soundInfo->pcmBuffer;
    s8 *bufR = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE;

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

        if (!(ch->statusFlags & SOUND_CHANNEL_SF_START))
            continue;
        if (ch->statusFlags & SOUND_CHANNEL_SF_STOP)
            continue;
        if (ch->wav == NULL || ch->currentPointer == NULL)
            continue;

        struct WaveData *wav = ch->wav;
        int loopEnabled = (wav->type & 0xC000) != 0;

        // 16.16 fixed-point step: source samples per output sample.
        u32 step = (u32)(((u64)ch->frequency * 256) / pcmFreq);
        u32 fw   = ch->fw;

        u8 volL = ch->envelopeVolumeLeft;
        u8 volR = ch->envelopeVolumeRight;

        volL = (u8)((volL * soundInfo->masterVolume) >> 4);
        volR = (u8)((volR * soundInfo->masterVolume) >> 4);

        s8 *end = wav->data + wav->loopStart + wav->size;

        for (int s = 0; s < nSamples; s++)
        {
            fw += step;
            u32 advance = fw >> 16;
            fw &= 0xFFFF;

            if (advance > 0)
            {
                ch->currentPointer += advance;

                while (ch->currentPointer >= end)
                {
                    if (loopEnabled)
                    {
                        u32 overshoot = (u32)(ch->currentPointer - end);
                        ch->currentPointer = wav->data + wav->loopStart + overshoot % wav->size;
                    }
                    else
                    {
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

    for (int s = 0; s < nSamples; s++)
    {
        s32 l = mixL[s], r = mixR[s];
        bufL[s] = (s8)(l >  127 ?  127 : l < -128 ? -128 : l);
        bufR[s] = (s8)(r >  127 ?  127 : r < -128 ? -128 : r);
    }
}

// ============================================================
// IsPokemonCryPlaying
// ============================================================

bool32 IsPokemonCryPlaying(struct MusicPlayerInfo *mplayInfo)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    for (int ci = 0; ci < soundInfo->maxChans; ci++)
    {
        struct SoundChannel *ch = &soundInfo->chans[ci];
        if (!(ch->statusFlags & SOUND_CHANNEL_SF_ON))
            continue;
        if (ch->statusFlags & SOUND_CHANNEL_SF_STOP)
            continue;
        if (ch->track != NULL && ch->track >= mplayInfo->tracks
            && ch->track < mplayInfo->tracks + mplayInfo->trackCount)
            return TRUE;
    }
    return FALSE;
}

#endif // PLATFORM_PC

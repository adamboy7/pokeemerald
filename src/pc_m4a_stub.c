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

void SoundMain(void) {}
void MPlayJumpTableCopy(MPlayFunc *mplayJumpTable) { memset(mplayJumpTable, 0, sizeof(MPlayFunc) * 36); }
void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void FadeOutBody(struct MusicPlayerInfo *mplayInfo) {}
void TrkVolPitSet(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track) {}
void MPlayOpen(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *tracks, u8 trackCount) {}
void MPlayStart(struct MusicPlayerInfo *mplayInfo, struct SongHeader *songHeader) {}
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

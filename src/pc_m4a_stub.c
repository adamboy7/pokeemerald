#include "global.h"
#include "gba/m4a_internal.h"
#include <string.h>

#if PLATFORM_PC
char gNumMusicPlayers = 4;
char gMaxLines = 0;
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
#endif

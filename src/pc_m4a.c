#include "m4a.h"
#ifdef PLATFORM_PC
struct MusicPlayerInfo gMPlayInfo_BGM;
struct MusicPlayerInfo gMPlayInfo_SE1;
struct MusicPlayerInfo gMPlayInfo_SE2;
struct MusicPlayerInfo gMPlayInfo_SE3;
struct SoundInfo gSoundInfo;

void m4aSoundVSync(void) {}
void m4aSoundVSyncOn(void) {}
void m4aSoundInit(void) {}
void m4aSoundMain(void) {}
void m4aSongNumStart(u16 n) {(void)n;}
void m4aSongNumStartOrChange(u16 n) {(void)n;}
void m4aSongNumStop(u16 n) {(void)n;}
void m4aMPlayAllStop(void) {}
void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo) {(void)mplayInfo;}
void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed) {(void)mplayInfo; (void)speed;}
void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed) {(void)mplayInfo; (void)speed;}
void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed) {(void)mplayInfo; (void)speed;}
void m4aMPlayImmInit(struct MusicPlayerInfo *mplayInfo) {(void)mplayInfo;}
#endif

#include "siirtc.h"

#ifdef PLATFORM_PC
#include <time.h>

static u8 BinaryToBcd(int value) { return ((value / 10) << 4) | (value % 10); }

void SiiRtcUnprotect(void) {}

void SiiRtcProtect(void) {}

u8 SiiRtcProbe(void) { return 1; }

bool8 SiiRtcReset(void) { return TRUE; }

bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc) {
  rtc->status = SIIRTCINFO_24HOUR;
  return TRUE;
}

static void ReadSysTime(struct SiiRtcInfo *rtc) {
  time_t now = time(NULL);
  struct tm t;
#if defined(_WIN32)
  localtime_s(&t, &now);
#else
  localtime_r(&now, &t);
#endif
  rtc->year = BinaryToBcd((t.tm_year + 1900) % 100);
  rtc->month = BinaryToBcd(t.tm_mon + 1);
  rtc->day = BinaryToBcd(t.tm_mday);
  rtc->dayOfWeek = t.tm_wday;
  rtc->hour = BinaryToBcd(t.tm_hour);
  rtc->minute = BinaryToBcd(t.tm_min);
  rtc->second = BinaryToBcd(t.tm_sec);
}

bool8 SiiRtcGetDateTime(struct SiiRtcInfo *rtc) {
  ReadSysTime(rtc);
  return TRUE;
}

bool8 SiiRtcSetStatus(struct SiiRtcInfo *rtc) {
  (void)rtc;
  return TRUE;
}

bool8 SiiRtcSetDateTime(struct SiiRtcInfo *rtc) {
  (void)rtc;
  return TRUE;
}

bool8 SiiRtcGetTime(struct SiiRtcInfo *rtc) {
  ReadSysTime(rtc);
  return TRUE;
}

bool8 SiiRtcSetTime(struct SiiRtcInfo *rtc) {
  (void)rtc;
  return TRUE;
}

bool8 SiiRtcSetAlarm(struct SiiRtcInfo *rtc) {
  (void)rtc;
  return TRUE;
}

#endif // PLATFORM_PC

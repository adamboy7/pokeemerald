#include "siirtc.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static int bcd_to_binary(int bcd)
{
    return ((bcd >> 4) & 0xF) * 10 + (bcd & 0xF);
}

int main(void)
{
    struct SiiRtcInfo rtc;
    SiiRtcGetDateTime(&rtc);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    assert(bcd_to_binary(rtc.year) == ((t->tm_year + 1900) % 100));
    assert(bcd_to_binary(rtc.month) == t->tm_mon + 1);
    assert(bcd_to_binary(rtc.day) == t->tm_mday);
    assert(bcd_to_binary(rtc.hour) == t->tm_hour);
    assert(bcd_to_binary(rtc.minute) == t->tm_min);

    int rtcSec = bcd_to_binary(rtc.second);
    int sysSec = t->tm_sec;
    assert(abs(rtcSec - sysSec) <= 1);

    printf("RTC matches system time\n");
    return 0;
}

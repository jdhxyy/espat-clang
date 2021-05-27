#include <stdio.h>
#include <windows.h>
#include <time.h>

#include "tzat.h"
#include "lagan.h"
#include "pt.h"
#include "tztime.h"
#include "tzmalloc.h"
#include "async.h"
#include "tztype.h"
#include "espat.h"

#define RAM_INTERNAL 0

static int gMid = -1;
static intptr_t handle = 0;

static void print(uint8_t* bytes, int size);
static LaganTime getLaganTime(void);
static uint64_t getTime(void);

static void writeResetIO(bool level);
static void setBaudRate(int baudRate);
static void tzatSend(uint8_t* bytes, int size);
static bool tzatIsAllowSend(void);

int main() {
    LaganLoad(print, getLaganTime);

    TZTimeLoad(getTime);
    TZMallocLoad(RAM_INTERNAL, 20, 100 * 1024, malloc(100 * 1024));
    gMid = TZMallocRegister(RAM_INTERNAL, "test", 4096);

    EspATLoadParam param;
    param.BaudRate = 115200;
    strcpy(param.WifiSsid, "ABCDEFG");
    strcpy(param.WifiPwd, "12345678");
    param.WriteResetIO = writeResetIO;
    param.SetBaudRate = setBaudRate;
    param.Send = tzatSend;
    param.IsAllowSend = tzatIsAllowSend;
    EspATLoad(&param);

    while (1) {
        AsyncRun();
    }

    return 0;
}

static void print(uint8_t* bytes, int size) {
    printf("%s", bytes);
}

static LaganTime getLaganTime(void) {
    SYSTEMTIME t1;
    GetSystemTime(&t1);

    LaganTime time;
    time.Year = t1.wYear;
    time.Month = t1.wMonth;
    time.Day = t1.wDay;
    time.Hour = t1.wHour;
    time.Minute = t1.wMinute;
    time.Second = t1.wSecond;
    time.Us = t1.wMilliseconds * 1000;
    return time;
}

static uint64_t getTime(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

static void writeResetIO(bool level) {
    printf("write reset io:%d\n", level);
}

static void setBaudRate(int baudRate) {
    printf("set baud rate:%d\n", baudRate);
}

static void tzatSend(uint8_t* bytes, int size) {
    printf("tzat send:%d %d %s\n", size, (int)strlen((char*)bytes), (char*)bytes);
}

static bool tzatIsAllowSend(void) {
    return true;
}

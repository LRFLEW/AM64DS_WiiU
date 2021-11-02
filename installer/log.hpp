#ifndef LOG_HPP
#define LOG_HPP

#if DEBUG_LOG >= 2

#include <whb/crash.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_udp.h>

#define LOGINIT() do { WHBLogUdpInit(); WHBLogConsoleInit(); WHBInitCrashHandler(); } while(0)
#define LOGFINISH() do { WHBLogUdpDeinit(); WHBLogConsoleFree(); } while(0)
#define LOG(...) do { WHBLogPrintf(__VA_ARGS__); WHBLogConsoleDraw(); } while(0)

#elif DEBUG_LOG

#include <whb/crash.h>
#include <whb/log.h>
#include <whb/log_udp.h>

#define LOGINIT() do { WHBLogUdpInit(); WHBInitCrashHandler(); } while(0)
#define LOGFINISH() WHBLogUdpDeinit()
#define LOG(...) WHBLogPrintf(__VA_ARGS__)

#else // No Logging

// Definitions have void bodies to ensure their usage is (mostly) correct
#define LOGINIT() ((void) 0)
#define LOGFINISH() ((void) 0)
#define LOG(...) ((void) (__VA_ARGS__))

#endif // DEBUG_LOG

#endif // LOG_HPP

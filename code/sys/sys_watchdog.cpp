/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with OpenMOHAA Plus source code.  If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/

// sys_watchdog.cpp -- Added in OpenMOHAA Plus: freeze watchdog and crash reports
//
// com_freezeWatch <seconds>: a background thread reports when the main loop has not completed a
// frame for that long, with the main thread's backtrace on POSIX systems, and when it recovers.
// On a fatal signal, Sys_WriteCrashReport appends version, map, signal and backtrace to
// <fs_homestatepath>/crash.log. Reports go to stderr and to the files, never through Com_Printf
// from the watchdog thread (the console is not thread-safe).

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_version.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#ifndef _WIN32
#    include <execinfo.h>
#    include <fcntl.h>
#    include <pthread.h>
#    include <signal.h>
#    include <unistd.h>
#endif

static std::atomic<unsigned int> watchdogFrames(0);
static std::atomic<int>          watchdogSeconds(0);
static std::atomic<bool>         watchdogRunning(false);
static std::thread              *watchdogThread = NULL;
static cvar_t                   *com_freezeWatch;
static cvar_t                   *watchdogMapname;

// Written once from the main thread, read from signal handlers
static char crashLogPath[MAX_OSPATH];
static char freezeLogPath[MAX_OSPATH];

#ifndef _WIN32
static pthread_t mainThread;

static void Sys_WriteString(int fd, const char *s)
{
    if (fd >= 0 && s) {
        ssize_t unused = write(fd, s, strlen(s));
        (void)unused;
    }
}

static void Sys_WriteInt(int fd, long value)
{
    char  buf[24];
    char *p   = buf + sizeof(buf) - 1;
    bool  neg = value < 0;

    *p = 0;
    if (neg) {
        value = -value;
    }
    do {
        *--p = '0' + value % 10;
        value /= 10;
    } while (value && p > buf + 1);
    if (neg) {
        *--p = '-';
    }
    Sys_WriteString(fd, p);
}

static void Sys_WriteBacktrace(int fd)
{
    void *frames[64];
    int   count = backtrace(frames, ARRAY_LEN(frames));

    // backtrace_symbols_fd does not allocate, unlike backtrace_symbols
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    if (fd >= 0) {
        backtrace_symbols_fd(frames, count, fd);
    }
}

static void Sys_FreezeSignal(int signum)
{
    int fd = freezeLogPath[0] ? open(freezeLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;

    Sys_WriteString(STDERR_FILENO, "---- main thread backtrace (freeze) ----\n");
    Sys_WriteString(fd, "---- main thread backtrace ----\n");
    Sys_WriteBacktrace(fd);
    Sys_WriteString(STDERR_FILENO, "----\n");
    if (fd >= 0) {
        close(fd);
    }
}
#endif

static void Sys_WatchdogLog(const char *text)
{
    fputs(text, stderr);
    fflush(stderr);
    if (freezeLogPath[0]) {
        FILE *f = fopen(freezeLogPath, "a");
        if (f) {
            fputs(text, f);
            fclose(f);
        }
    }
}

static void Sys_WatchdogLoop()
{
    using clock = std::chrono::steady_clock;

    unsigned int      lastFrame   = watchdogFrames.load();
    clock::time_point lastAdvance = clock::now();
    bool              reported    = false;
    char              text[256];

    while (watchdogRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const unsigned int frame   = watchdogFrames.load();
        const int          limit   = watchdogSeconds.load();
        const auto         now     = clock::now();
        const double       stalled = std::chrono::duration<double>(now - lastAdvance).count();

        if (frame != lastFrame) {
            if (reported) {
                Com_sprintf(text, sizeof(text), "WATCHDOG: main loop recovered after %.1f seconds\n", stalled);
                Sys_WatchdogLog(text);
                reported = false;
            }
            lastFrame   = frame;
            lastAdvance = now;
            continue;
        }

        if (limit <= 0 || reported || stalled < limit) {
            continue;
        }

        reported = true;
        Com_sprintf(
            text,
            sizeof(text),
            "WATCHDOG: main loop frozen for %.1f seconds (%s %s, map %s)\n",
            stalled,
            PRODUCT_NAME,
            PRODUCT_VERSION_FULL,
            watchdogMapname ? watchdogMapname->string : "?"
        );
        Sys_WatchdogLog(text);
#ifndef _WIN32
        pthread_kill(mainThread, SIGUSR2);
#endif
    }
}

extern "C" void Sys_Watchdog_Init(void)
{
    com_freezeWatch = Cvar_Get("com_freezeWatch", "0", CVAR_ARCHIVE);
    Cvar_CheckRange(com_freezeWatch, 0, 3600, qtrue);
    watchdogMapname = Cvar_Get("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);

    const char *home = Cvar_VariableString("fs_homestatepath");
    if (*home) {
        Com_sprintf(crashLogPath, sizeof(crashLogPath), "%s%ccrash.log", home, PATH_SEP);
        Com_sprintf(freezeLogPath, sizeof(freezeLogPath), "%s%cfreeze.log", home, PATH_SEP);
    }

#ifndef _WIN32
    mainThread = pthread_self();
    signal(SIGUSR2, Sys_FreezeSignal);
#endif

    watchdogSeconds.store(com_freezeWatch->integer);
    watchdogRunning.store(true);
    watchdogThread = new std::thread(Sys_WatchdogLoop);
}

extern "C" void Sys_Watchdog_Frame(void)
{
    watchdogFrames.fetch_add(1);
    if (com_freezeWatch) {
        watchdogSeconds.store(com_freezeWatch->integer);
    }
}

extern "C" void Sys_Watchdog_Shutdown(void)
{
    if (!watchdogThread) {
        return;
    }
    watchdogRunning.store(false);
    watchdogThread->join();
    delete watchdogThread;
    watchdogThread = NULL;
}

extern "C" void Sys_WriteCrashReport(int signum)
{
#ifndef _WIN32
    // Only async-signal-safe calls from here on
    int fd = crashLogPath[0] ? open(crashLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;

    Sys_WriteString(fd, "==== crash: signal ");
    Sys_WriteInt(fd, signum);
    Sys_WriteString(fd, " ====\n" PRODUCT_NAME " " PRODUCT_VERSION_FULL " " PLATFORM_STRING " " PRODUCT_VERSION_DATE "\nmap: ");
    Sys_WriteString(fd, watchdogMapname ? watchdogMapname->string : "?");
    Sys_WriteString(fd, "\n");
    Sys_WriteString(STDERR_FILENO, "----\nBacktrace:\n");
    Sys_WriteBacktrace(fd);
    Sys_WriteString(STDERR_FILENO, "----\n");
    Sys_WriteString(fd, "\n");
    if (fd >= 0) {
        close(fd);
        Sys_WriteString(STDERR_FILENO, "Crash report written to ");
        Sys_WriteString(STDERR_FILENO, crashLogPath);
        Sys_WriteString(STDERR_FILENO, "\n");
    }
#endif
}

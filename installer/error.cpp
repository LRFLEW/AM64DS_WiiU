#include "error.hpp"

#include <coreinit/exit.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "log.hpp"
#include "messages.hpp"
#include "proc.hpp"
#include "screen.hpp"

namespace {
    template<typename Func>
    void handle_and_close(Func func) __attribute__((noreturn));

    template<typename Func>
    void handle_and_close(Func func) {
        WUProc &proc = WUProc::get_global();
        Screen &screen = Screen::get_global();

        func(proc, screen);
        proc.release_home();
        while (proc.update())
            OSSleepTicks(OSMillisecondsToTicks(25));

        LOGFINISH();
        screen.~Screen();
        proc.~WUProc();
        _Exit(0);
    }
}

void handle_error(const char *msg) {
    handle_and_close([msg](WUProc &proc, Screen &screen) {
        LOG("Error: %s", msg);
        Messages::except(screen, msg, proc.is_hbl());
    });
}

void handle_no_iosuhax() {
    handle_and_close([](WUProc &proc, Screen &screen) {
        LOG("IOSUHAX Not Running");
        Messages::no_iosuhax(screen, proc.is_hbl());
    });
}

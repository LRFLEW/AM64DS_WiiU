#include "proc.hpp"

#include <cstdint>

#include <coreinit/foreground.h>
#include <coreinit/launch.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>
#include <coreinit/debug.h>

#include <proc_ui/procui.h>

#include <sysapp/launch.h>

#include "log.hpp"

WUProc::WUProc() {
    ProcUIInitEx(+[](void *) -> std::uint32_t {
            OSSavesDone_ReadyToRelease();
            return 0;
        }, nullptr);
    OSEnableHomeButtonMenu(FALSE);

    ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED,
        +[](void *param) -> std::uint32_t {
            WUProc *proc = reinterpret_cast<WUProc *>(param);
            if (!proc->wantToExit) {
                SYSLaunchMenu();
                proc->wantToExit = true;
            }
            return 0;
        }, this, 100);
    
}

WUProc::~WUProc() {
    ProcUIShutdown();
}

bool WUProc::update() {
    switch(ProcUIProcessMessages(true)) {
        case PROCUI_STATUS_EXITING:
            running = false;
            break;
        case PROCUI_STATUS_RELEASE_FOREGROUND:
            ProcUIDrawDoneRelease();
            break;
        default:
            break;
    }

    return running;
}

void WUProc::block_home() {
}

void WUProc::release_home() {
}

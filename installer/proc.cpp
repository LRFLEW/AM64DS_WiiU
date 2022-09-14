#include "proc.hpp"

#include <cstdint>

#include <coreinit/foreground.h>
#include <coreinit/launch.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>

#include <proc_ui/procui.h>

#include <sysapp/launch.h>

#include "log.hpp"

namespace {
    constexpr std::uint64_t HBL_TITLE_ID           = 0x00050000'13374842;
    constexpr std::uint64_t MII_MAKER_JPN_TITLE_ID = 0x00050010'1004A000;
    constexpr std::uint64_t MII_MAKER_USA_TITLE_ID = 0x00050010'1004A100;
    constexpr std::uint64_t MII_MAKER_EUR_TITLE_ID = 0x00050010'1004A200;
}

WUProc::WUProc() {
    uint64_t titleID = OSGetTitleID();
    hbc = (titleID == HBL_TITLE_ID          ) || (titleID == MII_MAKER_JPN_TITLE_ID) ||
          (titleID == MII_MAKER_USA_TITLE_ID) || (titleID == MII_MAKER_EUR_TITLE_ID);

    OSEnableHomeButtonMenu(false);
    ProcUIInitEx(+[](void *) -> std::uint32_t {
            OSSavesDone_ReadyToRelease();
            return 0;
        }, nullptr);
    ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED,
        +[](void *param) -> std::uint32_t {
            WUProc *proc = reinterpret_cast<WUProc *>(param);
            if (proc->home) proc->running = false;
            return 0;
        }, this, 100);
}

WUProc::~WUProc() {
    if (dirty) OSForceFullRelaunch();
    if (!hbc || dirty) {
        SYSLaunchMenu();
        running = true;
        while (update());
    }
    ProcUIShutdown();
    if (hbc && !dirty) SYSRelaunchTitle(0, nullptr);
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
    home = false;
}

void WUProc::release_home() {
    home = true;
}

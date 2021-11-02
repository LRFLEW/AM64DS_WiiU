#include "controls.hpp"

#include <padscore/kpad.h>
#include <padscore/wpad.h>

#include <vpad/input.h>

#include "log.hpp"

extern "C" void KPADShutdown();

namespace {
    constexpr int KMAX = 4;
}

Controls::Controls() {
    VPADInit();
    KPADInit();
}

Controls::~Controls() {
    KPADShutdown();
    VPADShutdown();
}

Controls::Input Controls::get() const {
    VPADStatus vpad;
    VPADReadError verr = VPAD_READ_NO_SAMPLES;
    KPADStatus kpad[KMAX];
    std::int32_t vcount, kcount[KMAX], kerr[KMAX];

    vcount = VPADRead(VPAD_CHAN_0, &vpad, 1, &verr);
    for (int chan = 0; chan < KMAX; ++chan)
        kcount[chan] = KPADReadEx(KPADChan(chan), &kpad[chan], 1, &kerr[chan]);
    LOG("VPAD %d, KPAD0 %d, KPAD1 %d, KPAD2 %d, KPAD3 %d",
        vcount, kcount[0], kcount[1], kcount[2], kcount[3]);

    if (vcount == 1 && verr == VPAD_READ_SUCCESS) {
        if (vpad.trigger & VPAD_BUTTON_A) return Controls::Input::A;
        if (vpad.trigger & VPAD_BUTTON_B) return Controls::Input::B;
        if (vpad.trigger & (VPAD_BUTTON_UP | VPAD_STICK_L_EMULATION_UP))
            return Controls::Input::Up;
        if (vpad.trigger & (VPAD_BUTTON_DOWN | VPAD_STICK_L_EMULATION_DOWN))
            return Controls::Input::Down;
    }

    for (int chan = 0; chan < 4; ++chan) {
        KPADStatus &pad = kpad[chan];
        if (kcount[chan] == 1 && kerr[chan] == KPAD_ERROR_OK) {
            if (pad.trigger & WPAD_BUTTON_A) return Controls::Input::A;
            if (pad.trigger & WPAD_BUTTON_B) return Controls::Input::B;
            if (pad.trigger & WPAD_BUTTON_UP) return Controls::Input::Up;
            if (pad.trigger & WPAD_BUTTON_DOWN) return Controls::Input::Down;
            LOG("KPAD ext %d", pad.extensionType);
            switch (pad.extensionType) {
                case WPAD_EXT_NUNCHUK:
                case WPAD_EXT_MPLUS_NUNCHUK:
                    if (pad.nunchuck.trigger & WPAD_NUNCHUK_STICK_EMULATION_UP)
                        return Controls::Input::Up;
                    if (pad.nunchuck.trigger & WPAD_NUNCHUK_STICK_EMULATION_DOWN)
                        return Controls::Input::Down;
                    break;
                case WPAD_EXT_CLASSIC:
                case WPAD_EXT_MPLUS_CLASSIC:
                    if (pad.classic.trigger & WPAD_CLASSIC_BUTTON_A)
                        return Controls::Input::A;
                    if (pad.classic.trigger & WPAD_CLASSIC_BUTTON_B)
                        return Controls::Input::B;
                    if (pad.classic.trigger & (WPAD_CLASSIC_BUTTON_UP |
                                               WPAD_CLASSIC_STICK_L_EMULATION_UP))
                        return Controls::Input::Up;
                    if (pad.classic.trigger & (WPAD_CLASSIC_BUTTON_DOWN |
                                               WPAD_CLASSIC_STICK_L_EMULATION_DOWN))
                        return Controls::Input::Down;
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if (pad.pro.trigger & WPAD_PRO_BUTTON_A)
                        return Controls::Input::A;
                    if (pad.pro.trigger & WPAD_PRO_BUTTON_B)
                        return Controls::Input::B;
                    if (pad.pro.trigger & (WPAD_PRO_BUTTON_UP |
                                           WPAD_PRO_STICK_L_EMULATION_UP))
                        return Controls::Input::Up;
                    if (pad.pro.trigger & (WPAD_PRO_BUTTON_DOWN |
                                           WPAD_PRO_STICK_L_EMULATION_DOWN))
                        return Controls::Input::Down;
                    break;
                default:
                    break;
            }
        }
    }

    return Controls::Input::None;
}

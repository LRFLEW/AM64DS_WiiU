#include "vpad.hpp"

#include <vpad/input.h>

Vpad::Vpad() {
    VPADInit();
}

Vpad::~Vpad() {
    VPADShutdown();
}

Vpad::Input Vpad::get() const {
    VPADStatus vpad;
    VPADReadError err = VPAD_READ_NO_SAMPLES;

    VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
    if (err != VPAD_READ_SUCCESS) return Vpad::Input::None;

    if (vpad.trigger & VPAD_BUTTON_A) return Vpad::Input::A;
    if (vpad.trigger & VPAD_BUTTON_B) return Vpad::Input::B;
    if (vpad.trigger & (VPAD_BUTTON_UP | VPAD_STICK_L_EMULATION_UP))
        return Vpad::Input::Up;
    if (vpad.trigger & (VPAD_BUTTON_DOWN | VPAD_STICK_L_EMULATION_DOWN))
        return Vpad::Input::Down;
    return Vpad::Input::None;
}

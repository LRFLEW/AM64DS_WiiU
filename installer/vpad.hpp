#ifndef VPAD_HPP
#define VPAD_HPP

#include <cstdint>

class Vpad {
public:
    Vpad();
    ~Vpad();

    enum class Input : std::uint_fast8_t {
        None,
        A,
        B,
        Up,
        Down,
    };
    Input get() const;
};

#endif // VPAD_HPP

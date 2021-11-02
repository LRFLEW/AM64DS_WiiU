#ifndef CONTROLS_HPP
#define CONTROLS_HPP

#include <cstdint>

class Controls {
public:
    Controls();
    ~Controls();

    enum class Input : std::uint_fast8_t {
        None,
        A,
        B,
        Up,
        Down,
    };
    Input get() const;
};

#endif // CONTROLS_HPP

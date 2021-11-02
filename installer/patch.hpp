#ifndef PATCH_HPP
#define PATCH_HPP

#include <cstdint>

class Patch {
public:
    enum class Status : std::int_fast8_t {
        UNTESTED = 0,

        RPX_ONLY = 1,
        IS_JPN = 2,
        IS_USA = 3,
        IS_EUR = 4,
        IS_KOR = 5,

        PATCHED = -1,
        MISSING_RPX = -2,
        INVALID_RPX = -3,
        INVALID_NTR = -4,
        IS_HAXCHI = -5,
        INVALID_ZIP = -6,
        UNKNOWN_ERR = -7,
    };

    virtual ~Patch() = default;

    virtual void Read() = 0;
    virtual void Modify() = 0;
    virtual void Write() = 0;
};

#endif // PATCH_HPP

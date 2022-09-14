#ifndef SCREEN_HPP
#define SCREEN_HPP

#include <array>
#include <cstdint>
#include <string>

#include "aligned.hpp"

class Screen {
public:
    struct Line {
        std::uint32_t row, column;
        const char *msg;
    };

    Screen();
    ~Screen();

    Screen(const Screen &) = delete;
    Screen(Screen &&) = delete;
    Screen &operator=(const Screen &) = delete;
    Screen &operator=(Screen &&) = delete;

    void put(std::uint32_t row, std::uint32_t column, const char *str);
    void put(std::uint32_t row, std::uint32_t column, const std::string &str)
        { put(row, column, str.c_str()); }
    void put(Line line) { put(line.row, line.column, line.msg); }
    void swap();

private:
};

#endif // SCREEN_HPP

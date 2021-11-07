#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include "screen.hpp"
#include "title.hpp"

namespace Messages {
    void scanning(Screen &screen, bool full = false);
    void select(Screen &screen, const Title::Filtered &titles,
                std::size_t selected, bool full, bool haxchi,
                bool patched, bool hbl);
    void confirm(Screen &screen, Title &title, bool hbl);
    void full_warn(Screen &screen, bool hbl);
    void patch(Screen &screen, int step);
    void post_patch(Screen &screen);
    void except(Screen &screen, const char *msg, bool hbl);
    void no_iosuhax(Screen &screen, bool hbl);
}

#endif // MESSAGES_HPP

#include "save_clean.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <nn/act.h>

#include "exception.hpp"
#include "iosufsa.hpp"
#include "log.hpp"
#include "patch.hpp"
#include "util.hpp"

using namespace std::string_view_literals;

namespace {
    class act_guard final {
    public:
        ~act_guard() { nn::act::Finalize(); }
    };

    const char *exist_str(bool b) { return b ? "Removed" : "n/a"; }

    constexpr std::string_view state_suffix =
        "/user/00000000/S_MARIO64DSASMX01_0123456789012345678901234567890123456789_0.state"sv;
    constexpr std::array<char, 4> region_letters = { 'J', 'E', 'P', 'K' };
    constexpr std::size_t user_offset = 6;
    constexpr std::size_t region_offset = 29;
    constexpr std::size_t type_offset = 74;
    constexpr std::string_view title_dir = "/title/"sv;
    constexpr std::string_view save_dir = "/save/"sv;
}

void save_clean(const IOSUFSA &fsa, std::string_view title) {
    std::size_t title_off = title.rfind(title_dir);
    if (title_off == std::string_view::npos) throw error("Save: User");

    const std::size_t suffix_offset = title.length() + save_dir.length() - title_dir.length();
    std::string state_path;
    state_path.reserve(suffix_offset + state_suffix.length());
    state_path.append(title.substr(0, title_off)).append(save_dir);
    state_path.append(title.substr(title_off + title_dir.length())).append(state_suffix);

    if (nn::act::Initialize().IsFailure()) throw error("Save: NN_Act Init");
    act_guard guard;
    nn::act::SlotNo user_count = nn::act::GetNumOfAccounts();
    LOG("Users Count: %d", user_count);

    for (nn::act::SlotNo i = 0, k = 0; k < user_count && i <= 12; ++i) {
        if (nn::act::IsSlotOccupied(i)) {
            nn::act::PersistentId userId = nn::act::GetPersistentIdEx(i);
            LOG("User ID: %08X", userId);
            util::write_hex(userId, state_path, suffix_offset + user_offset);

            for (char region : region_letters) {
                LOG("Savestate Region: %c", region);
                state_path[suffix_offset + region_offset] = region;
                state_path[suffix_offset + type_offset] = '0';
                bool exist0 = fsa.remove(state_path);
                state_path[suffix_offset + type_offset] = '1';
                bool exist1 = fsa.remove(state_path);
                LOG("0.state: %s - 1.state: %s", exist_str(exist0), exist_str(exist1));
            }
            ++k;
        }
    }
}

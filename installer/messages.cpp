#include "messages.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "log.hpp"
#include "patch.hpp"
#include "util.hpp"

using namespace std::string_view_literals;

namespace {
    constexpr std::uint32_t bottom = 17;
    constexpr std::size_t select_count = 5;

    constexpr Screen::Line title_line = { 0, 5, "AM64DS Wii U Patching Tool by LRFLEW" };

    constexpr Screen::Line home_hbl = { bottom - 1, 2,
        "Press HOME to return to the Homebrew Launcher" };
    constexpr Screen::Line home_menu = { bottom - 1, 2,
        "Press HOME to return to the Wii U Menu" };

    constexpr Screen::Line wait = { 2, 0, "Scanning your system,"};
    constexpr Screen::Line wait_part = { 2, 22, "please wait..." };
    constexpr Screen::Line wait_full = { 2, 22, "this may take a few minutes." };

    constexpr Screen::Line list_head = { 2, 0, "Select the title to patch:" };
    constexpr std::string_view title_template = "  00000000:00000000 [XXX]"sv;
    constexpr std::string_view scan_template = "  Perform Full System Scan"sv;
    constexpr std::size_t reg_len = 3;
    constexpr std::array<char[reg_len + 1], 4> regions = { "JPN", "USA", "EUR", "KOR" };
    constexpr char select_char = '>';
    constexpr std::size_t high_off = 2;
    constexpr std::size_t low_off = 11;
    constexpr std::size_t reg_off = 21;
    constexpr std::size_t list_row = 4;
    constexpr std::size_t list_column = 2;
    constexpr Screen::Line more_up = { 3, 9, "^^^^^^^^^^^" };
    constexpr Screen::Line more_down = { 9, 9, "vvvvvvvvvvv" };
    constexpr Screen::Line select_a = { bottom - 2, 2, "Press A to patch the selected title" };

    constexpr Screen::Line no_list = { 2, 0,
        "Super Mario 64 DS was not found on your Wii U." };
    constexpr Screen::Line patched_list = { 2, 0,
        "All copies of Super Mario 64 DS found are already patched." };
    constexpr Screen::Line rec_full = { 3, 0,
        "If it's installed in a non-standard location,\n"
        "press A to perform a full-system scan." };
    constexpr Screen::Line no_a = { bottom - 2, 2, "Press A to perform a Full System Scan" };
    constexpr Screen::Line haxchi_warn = { 6, 0,
        "It appears you're using Haxchi installed to an eShop copy\n"
        "of Super Mario 64 DS.  AM64DS cannot be installed on the same\n"
        "title as Haxchi." };

    constexpr Screen::Line install_head = { 2, 2, "Install the AM64DS patch to this title?" };
    constexpr std::size_t install_title_row = 3;
    constexpr std::size_t install_title_column = 2;
    constexpr Screen::Line install_msg = { 5, 0,
        "Once the patch is installed, you will need to use a CFW when\n"
        "launching the game. If you want to uninstall the patch, you\n"
        "will need to delete the game in System Settings under\n"
        "Data Management and reinstall it." };
    constexpr Screen::Line install_a = { bottom - 3, 2, "Press A to patch the game" };
    constexpr Screen::Line install_b = { bottom - 2, 2, "Press B to go back" };

    constexpr Screen::Line full_head = { 2, 2, "Scan all installed titles?" };
    constexpr Screen::Line full_msg = { 4, 0,
        "If you installed Super Mario 64 DS as a ROM injection title,\n"
        "it may not be in the same location as an eShop copy. A Full\n"
        "System Scan is required to find these titles to patch them.\n"
        "The scan may take a few minutes depending on how much is\n"
        "installed to the Wii U." };
    constexpr Screen::Line full_a = { bottom - 3, 2, "Press A to perform a Full System Scan" };

    constexpr Screen::Line patch_head = { 2, 2, "Patching SM64DS. This may take a minute..." };
    constexpr Screen::Line patch_rpx_read = { 4, 0, "Read Hachi RPX" };
    constexpr Screen::Line patch_rpx_modify = { 5, 0, "Patch Hachi RPX" };
    constexpr Screen::Line patch_rpx_write = { 6, 0, "Write Hachi RPX" };
    constexpr Screen::Line patch_ntr_read = { 7, 0, "Read NTR ROM" };
    constexpr Screen::Line patch_ntr_modify = { 8, 0, "Patch NTR ROM" };
    constexpr Screen::Line patch_ntr_write = { 9, 0, "Write NTR ROM" };
    constexpr Screen::Line patch_final = { 10, 0, "Finalizing Patch" };

    constexpr Screen::Line post_head = { 2, 2, "Patching Complete" };
    constexpr Screen::Line post_msg = { 4, 0,
        "If you want to patch more titles, press B to go back to\n"
        "the list of titles. Otherwise, press HOME to return to the\n"
        "Wii U Main Menu" };
    constexpr Screen::Line post_b = { bottom - 2, 2, "Press B to select another title to patch" };

    constexpr Screen::Line except_head = { 2, 2, "An unexpected error has occured" };
    constexpr Screen::Line except_msg = { 4, 0,
        "An unexpected error occured in the program. Please restart\n"
        "this program and try again. If the error keeps occuring,\n"
        "please report the error to the developer." };
    constexpr Screen::Line except_label = { 8, 2, "Error Message:" };
    constexpr std::size_t except_row = 9;
    constexpr std::size_t except_column = 5;

    constexpr Screen::Line hax_head = { 2, 2, "There is no CFW running" };
    constexpr Screen::Line hax_msg = { 4, 0,
        "This patching tool requires a CFW such as Mocha or Haxhi\n"
        "to be running for the installer to work. Please make sure\n"
        "your CFW is running and restart this tool." };
}

void Messages::scanning(Screen &screen, bool full) {
    screen.put(title_line);
    screen.put(wait);
    if (full) screen.put(wait_full);
    else screen.put(wait_part);
    screen.swap();
}

void Messages::select(Screen &screen, const Title::Filtered &titles,
                      std::size_t selected, bool full, bool haxchi,
                      bool patched, bool hbl) {
    screen.put(title_line);
    if (titles.empty()) {
        if (patched) screen.put(patched_list);
        else screen.put(no_list);
        if (!full) {
            screen.put(rec_full);
            screen.put(no_a);
        }
        if (haxchi) screen.put(haxchi_warn);
    } else {
        screen.put(list_head);
        std::string line;
        line.reserve(std::max(title_template.length(), scan_template.length()));
        line = title_template;
        std::size_t count = titles.size() + (!full);
        std::size_t show = std::min(count, select_count);
        std::size_t start;
        if (count <= select_count) {
            start = 0;
        } else if (selected <= select_count / 2) {
            start = 0;
            screen.put(more_down);
        } else if (selected >= count - (select_count / 2) - 1) {
            start = count - select_count;
            screen.put(more_up);
        } else {
            start = selected - (select_count / 2);
            screen.put(more_down);
            screen.put(more_up);
        }
        for (std::size_t i = 0; i < show; ++i) {
            std::size_t off = start + i;
            if (off == titles.size()) {
                line = scan_template;
            } else {
                Title &title = titles[off];
                util::write_hex(title.get_id() >> 32, line, high_off);
                util::write_hex(title.get_id(), line, low_off);
                std::size_t reg_ind = static_cast<std::size_t>(title.get_status_raw()) -
                                      static_cast<std::size_t>(Patch::Status::IS_JPN);
                for (std::size_t j = 0; j < reg_len; ++j)
                    line[reg_off + j] = regions[reg_ind][j];
            }
            if (off == selected) line[0] = select_char;
            else line[0] = title_template[0];
            screen.put(list_row + i, list_column, line);
        }
        screen.put(select_a);
    }
    if (hbl) screen.put(home_hbl);
    else screen.put(home_menu);
    screen.swap();
}

void Messages::confirm(Screen &screen, Title &title, bool hbl) {
    screen.put(title_line);
    screen.put(install_head);

    std::string line(title_template);
    util::write_hex(title.get_id() >> 32, line, high_off);
    util::write_hex(title.get_id(), line, low_off);
    std::size_t reg_ind = static_cast<std::size_t>(title.get_status_raw()) -
                          static_cast<std::size_t>(Patch::Status::IS_JPN);
    for (std::size_t j = 0; j < reg_len; ++j)
        line[reg_off + j] = regions[reg_ind][j];
    screen.put(install_title_row, install_title_column, line);

    screen.put(install_msg);
    screen.put(install_a);
    screen.put(install_b);
    if (hbl) screen.put(home_hbl);
    else screen.put(home_menu);
    screen.swap();
}

void Messages::full_warn(Screen &screen, bool hbl) {
    screen.put(title_line);
    screen.put(full_head);
    screen.put(full_msg);
    screen.put(full_a);
    screen.put(install_b);
    if (hbl) screen.put(home_hbl);
    else screen.put(home_menu);
    screen.swap();
}

void Messages::patch(Screen &screen, int step) {
    screen.put(title_line);
    screen.put(patch_head);
    if (step >= 1) screen.put(patch_rpx_read);
    if (step >= 2) screen.put(patch_rpx_modify);
    if (step >= 3) screen.put(patch_rpx_write);
    if (step >= 4) screen.put(patch_ntr_read);
    if (step >= 5) screen.put(patch_ntr_modify);
    if (step >= 6) screen.put(patch_ntr_write);
    if (step >= 7) screen.put(patch_final);
    screen.swap();
}

void Messages::post_patch(Screen &screen) {
    screen.put(title_line);
    screen.put(post_head);
    screen.put(post_msg);
    screen.put(post_b);
    screen.put(home_menu);
    screen.swap();
}

void Messages::except(Screen &screen, const char *msg, bool hbl) {
    screen.put(title_line);
    screen.put(except_head);
    screen.put(except_msg);
    screen.put(except_label);
    screen.put(except_row, except_column, msg);
    if (hbl) screen.put(home_hbl);
    else screen.put(home_menu);
    screen.swap();
}

void Messages::no_iosuhax(Screen &screen, bool hbl) {
    screen.put(title_line);
    screen.put(hax_head);
    screen.put(hax_msg);
    if (hbl) screen.put(home_hbl);
    else screen.put(home_menu);
    screen.swap();
}

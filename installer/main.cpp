#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "controls.hpp"
#include "hachi_patch.hpp"
#include "iosufsa.hpp"
#include "log.hpp"
#include "messages.hpp"
#include "ntr_patch.hpp"
#include "patch.hpp"
#include "proc.hpp"
#include "save_clean.hpp"
#include "screen.hpp"
#include "title.hpp"
#include "util.hpp"
#include "zlib.hpp"

using namespace std::string_view_literals;

namespace {
    constexpr std::uint64_t SM64DS_JPN_TITLE_ID = 0x00050000'101C3300;
    constexpr std::uint64_t SM64DS_USA_TITLE_ID = 0x00050000'101C3400;
    constexpr std::uint64_t SM64DS_EUR_TITLE_ID = 0x00050000'101C3500;

    enum class ControlState {
        SELECT,
        CONFIRM,
        CLEAR,
    };

    bool check_titleid(const Title &title) {
        return title.get_id() == SM64DS_JPN_TITLE_ID ||
               title.get_id() == SM64DS_USA_TITLE_ID ||
               title.get_id() == SM64DS_EUR_TITLE_ID;
    }

    Title::Filtered scan_titles(std::vector<Title> &titles, bool full) {
        Title::Filtered filtered;
        LOG("Init IOSUHAX...");
        IOSUFSA fsa;
        fsa.open();

        filtered = Title::filter(titles,
            [&fsa, full](Title &title) -> bool {
                return (!full && !check_titleid(title)) ||
                       title.get_status(fsa) <= Patch::Status::UNTESTED;
            });

        LOG("Closing IOSUHAX");
        fsa.close();
        return filtered;
    }

    void patch_title(Screen &screen, Title &title) {
        Messages::patch(screen, 0);
        LOG("Init IOSUHAX...");
        IOSUFSA fsa;
        fsa.open();

        LOG("Patching Hachi...");
        std::unique_ptr<Patch> hachi = hachi_patch(fsa, title.get_path());
        LOG("Read Hachi");
        Messages::patch(screen, 1);
        hachi->Read();
        LOG("Patch Hachi");
        Messages::patch(screen, 2);
        hachi->Modify();
        LOG("Write Hachi");
        Messages::patch(screen, 3);
        hachi->Write();
        hachi.release();

        LOG("Patching NTR...");
        std::unique_ptr<Patch> ntr = ntr_patch(fsa, title.get_path());
        LOG("Read NTR");
        Messages::patch(screen, 4);
        ntr->Read();
        LOG("Patch NTR");
        Messages::patch(screen, 5);
        ntr->Modify();
        LOG("Write NTR");
        Messages::patch(screen, 6);
        ntr->Write();
        ntr.release();

        LOG("Start Savestate Cleaning...");
        Messages::patch(screen, 7);
        save_clean(fsa, title.get_path());
        LOG("Savestate Cleaning Done");

        std::string dev = util::concat_sv({ "/vol/storage_"sv, title.get_dev(), "01"sv });
        LOG("Flush Volume %s ...", dev.c_str());
        bool status = fsa.flush_volume(dev);

        if (status) {
            LOG("Flush Volume Successful");
        } else {
            LOG("FLUSH VOLUME FAILURE");
        }
        title.flag_patched();

        LOG("Closing IOSUHAX");
        fsa.close();
    }

    bool has_status(const std::vector<Title> &titles, Patch::Status status) {
        return std::any_of(titles.begin(), titles.end(),
            [status](const Title &title) -> bool {
                return title.get_status_raw() == status;
            });
    }
}

int main(int argc, char **argv) {
    WUProc proc;
    Screen screen;
    Controls controls;
    LOGINIT();

    std::vector<Title> titles;
    Title::Filtered filtered;
    std::size_t selected = 0;
    ControlState state = ControlState::SELECT;
    bool full = false, haxchi = false, patched = false;

    try {
        {
            WUHomeLock home_lock(proc, controls);
            Messages::scanning(screen, full);

            titles = Title::get_titles();
            filtered = scan_titles(titles, full);

            for (Title &title : filtered) {
                LOG("FOUND: %s", title.get_path().c_str());
                LOG("FOUND NAME: %s", title.get_name().c_str());
            }
        }

        haxchi = has_status(titles, Patch::Status::IS_HAXCHI);
        patched = has_status(titles, Patch::Status::PATCHED);

        Messages::select(screen, filtered, selected, full, haxchi, patched, proc.is_hbl());

        LOG("Entering Proc Loop...");
        while (proc.update()) {
            OSSleepTicks(OSMillisecondsToTicks(25));

            switch (state) {
                case ControlState::SELECT:
                    switch (controls.get()) {
                        case Controls::Input::A:
                            if (selected < filtered.size())
                                Messages::confirm(screen, filtered[selected], proc.is_hbl());
                            else if (!full)
                                Messages::full_warn(screen, proc.is_hbl());
                            state = ControlState::CONFIRM;
                            break;
                        case Controls::Input::Up:
                            if (selected > 0) {
                                Messages::select(screen, filtered, --selected,
                                                 full, haxchi, patched, proc.is_hbl());
                            }
                            break;
                        case Controls::Input::Down:
                            if (selected < (filtered.size() + !full) - 1) {
                                Messages::select(screen, filtered, ++selected,
                                                 full, haxchi, patched, proc.is_hbl());
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case ControlState::CONFIRM:
                    switch (controls.get()) {
                        case Controls::Input::A:
                            if (selected < filtered.size()) {
                                WUHomeLock home_lock(proc, controls);
                                proc.flag_dirty();
                                patch_title(screen, filtered[selected]);
                                filtered = scan_titles(titles, full);
                                patched = true;
                                Messages::post_patch(screen);
                                state = ControlState::CLEAR;
                            } else {
                                WUHomeLock home_lock(proc, controls);
                                full = true;
                                Messages::scanning(screen, full);
                                filtered = scan_titles(titles, full);
                                patched = has_status(titles, Patch::Status::PATCHED);
                                selected = 0;
                                Messages::select(screen, filtered, selected,
                                                 full, haxchi, patched, proc.is_hbl());
                                state = ControlState::SELECT;
                            }
                            break;
                        case Controls::Input::B:
                            Messages::select(screen, filtered, selected,
                                             full, haxchi, patched, proc.is_hbl());
                            state = ControlState::SELECT;
                            break;
                        default:
                            break;
                    }
                    break;
                case ControlState::CLEAR:
                    switch (controls.get()) {
                        case Controls::Input::B:
                            Messages::select(screen, filtered, (selected = 0),
                                             full, haxchi, patched, proc.is_hbl());
                            state = ControlState::SELECT;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }
    } catch (error &e) {
        LOG("Error: %s", e.what());
        Messages::except(screen, e.what(), proc.is_hbl());
        proc.release_home();
        while (proc.update())
            OSSleepTicks(OSMillisecondsToTicks(25));
    } catch (IOSUFSA::no_iosuhax &e) {
        LOG("IOSUHAX Not Running");
        Messages::no_iosuhax(screen, proc.is_hbl());
        proc.release_home();
        while (proc.update())
            OSSleepTicks(OSMillisecondsToTicks(25));
    }

    LOG("Exiting... good bye.");

    LOGFINISH();
    return 0;
}

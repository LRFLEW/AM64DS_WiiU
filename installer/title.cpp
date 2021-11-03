#include "title.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

#include <coreinit/mcp.h>
#include <coreinit/userconfig.h>

#include <nn/acp/title.h>

#include "aligned.hpp"
#include "exception.hpp"
#include "hachi_patch.hpp"
#include "log.hpp"
#include "ntr_patch.hpp"

namespace {
    class MCP {
    public:
        MCP() { fd = MCP_Open(); if (fd < 0) throw error("MCP: Open"); }
        ~MCP() { MCP_Close(fd); }
        operator std::int32_t() { return fd; }

    private:
        std::int32_t fd;
    };

    class UC {
    public:
        UC() { fd = UCOpen(); if (fd < 0) throw error("UC: Open"); }
        ~UC() { UCClose(fd); }
        operator std::int32_t() { return fd; }

    private:
        std::int32_t fd;
    };

    constexpr std::uint32_t max_languages = 12;

    int get_sys_language() {
        // Cache result so it's only queried once
        static std::uint32_t language = 0xFF;
        if (language < max_languages) return language;

        UC uc;
        alignas(0x40) UCSysConfig request = {
            "cafe.language", 0x777, UC_DATATYPE_UNSIGNED_INT, 0,
            sizeof(std::uint32_t), &language
        };
        std::int32_t res = UCReadSysConfig(uc, 1, &request);
        if (res < 0 || language >= max_languages) {
            language = 0xFF;
            throw error("Lang: bad UCReadSysConfig");
        }
        LOG("Got language: %d", language);
        return language;
    }
}

std::vector<Title> Title::get_titles() {
    MCP mcp;

    std::uint32_t count = MCP_TitleCount(mcp);
    LOG("Prior Count: %d", count);
    aligned::vector<MCPTitleListType, 0x40> titles(count);
    int res = MCP_TitleListByAppType(mcp, MCP_APP_TYPE_GAME,
                                     &count, titles.data(),
                                     sizeof(MCPTitleListType) * titles.size());
    if (res < 0) throw error("MCP: bad list");
    titles.resize(count);
    LOG("Titles Count: %d", count);

    std::vector<Title> processed;
    processed.reserve(count);
    for (MCPTitleListType &title : titles)
        processed.push_back({ title.titleId, title.path, title.indexedDevice });
    return processed;
}

std::string Title::get_name_impl() {
    std::uint32_t language = get_sys_language();
    alignas(0x40) ACPMetaXml meta;
    if (ACPGetTitleMetaXml(id, &meta) < 0) throw error("ACP: bad meta");

    const char (*names)[512] = &meta.longname_ja;
    //const char (*names)[256] = &meta.shortname_ja;
    if (names[language][0] != '\0') return names[language];
    // Fallback if no native name is available
    for (std::size_t i = 0; i < 12; ++i)
        if (names[i][0] != '\0') return names[i];
    return "Unknown Application";
}

Patch::Status Title::get_status_impl(const IOSUFSA &fsa) {
    Patch::Status res;
    LOG("Checking title: %s", path.c_str());

    LOG("Checking Hachi...");
    res = hachi_check(fsa, path);
    if (res < Patch::Status::UNTESTED) return res;

    LOG("Checking NTR...");
    res = ntr_check(fsa, path);
    return res;
}

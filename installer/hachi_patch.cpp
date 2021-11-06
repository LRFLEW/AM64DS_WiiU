#include "hachi_patch.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <elf.h>

#include "exception.hpp"
#include "iosufsa.hpp"
#include "log.hpp"
#include "util.hpp"
#include "zlib.hpp"

#include "inject_bin.h"
#include "inject_s.h"

using namespace std::string_view_literals;

namespace {
    constexpr Elf32_Half RPX_TYPE = 0xFE01;
    constexpr unsigned char RPX_ABI1 = 0xCA;
    constexpr unsigned char RPX_ABI2 = 0xFE;
    constexpr Elf32_Word RPX_CRCS = 0x80000003;
    constexpr std::uint32_t ZLIB_SECT = 0x08000000;

    constexpr std::string_view hachi_file = "/code/hachihachi_ntr.rpx"sv;
    constexpr std::uint32_t magic_amds = util::magic_const("AMDS");

    static_assert(sizeof(Elf32_Ehdr) == 52);
    static_assert(sizeof(Elf32_Shdr) == 40);

    constexpr Elf32_Ehdr expected_ehdr = {
        {
            ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS32,
            ELFDATA2MSB, EV_CURRENT, RPX_ABI1, RPX_ABI2
        },
        RPX_TYPE, EM_PPC, EV_CURRENT, 0x02026798, 0x00, 0x40, 0,
        sizeof(Elf32_Ehdr), 0, 0, sizeof(Elf32_Shdr), 29, 26,
    };
    constexpr std::array<std::uint32_t, expected_ehdr.e_shnum> expected_crcs = {
        0x00000000, 0x14596B94, 0x165C39F2, 0xFA312336,
        0x9BF039EE, 0xB3241733, 0x00000000, 0x60DA42CF,
        0xEB7267F9, 0xF124402E, 0x01F80C21, 0x5E06092F,
        0xD4CE0752, 0x72FA23E0, 0x940942DB, 0xBCFBF24D,
        0x6B6574C9, 0x8D5FEDD5, 0x040E8EE3, 0xD5CEFF4A,
        0xCFB2B47E, 0xE94CF6E0, 0x085C47BB, 0x279F690F,
        0x598C85C9, 0x82F59D73, 0x2316975E, 0x00000000,
        0x7D6C2996,
    };

    bool good_layout(const std::vector<Elf32_Shdr> &shdr, const std::vector<std::size_t> &sorted) {
        std::uint32_t last_off = 0x40 + sizeof(Elf32_Shdr) * shdr.size();
        for (std::size_t i : sorted) {
            const Elf32_Shdr &sect = shdr[i];
            LOG("Check Header %d", i);
            if (sect.sh_offset - last_off >= 0x40) return false;
            last_off = sect.sh_offset + sect.sh_size;
        }
        return true;
    }

    bool decompress_sect(Elf32_Shdr &shdr, std::vector<std::uint8_t> &data) {
        if (!(shdr.sh_flags & ZLIB_SECT)) return true;
        if (shdr.sh_size != data.size()) return false;
        if (shdr.sh_size < 4) return false;
        std::uint32_t dec_len = *reinterpret_cast<const std::uint32_t *>(data.data());

        LOG("Inflate");
        data = Zlib::decompress(data, dec_len, true);

        shdr.sh_size = dec_len;
        shdr.sh_flags &= ~ZLIB_SECT;
        return true;
    }

    bool compress_sect(Elf32_Shdr &shdr, std::vector<std::uint8_t> &data) {
        if (shdr.sh_flags & ZLIB_SECT) return true;
        if (shdr.sh_size != data.size()) return false;

        std::vector<std::uint8_t> cmp;
        LOG("Deflate");
        cmp = Zlib::compress(data, true);

        if (cmp.size() < data.size()) {
            shdr.sh_size = cmp.size();
            data = std::move(cmp);
            shdr.sh_flags |= ZLIB_SECT;
        }
        return true;
    }

    void shift_for_resize(std::size_t resized, std::vector<Elf32_Shdr> &shdr,
                                 const std::vector<std::size_t> &sorted) {
        auto it = std::find(sorted.begin(), sorted.end(), resized);
        if (it == sorted.end()) return;

        Elf32_Shdr &resized_sect = shdr[*it];
        std::uint32_t last_off = resized_sect.sh_offset + resized_sect.sh_size;
        for (++it; it != sorted.end(); ++it) {
            Elf32_Shdr &sect = shdr[*it];
            sect.sh_offset = (last_off + 0x3F) & ~0x3F;
            last_off = sect.sh_offset + sect.sh_size;
        }
    }

    void make_b(std::vector<std::uint8_t> &data, std::size_t offset, std::size_t target) {
        std::uint32_t inst = (0x48000000 | (target - offset)) & 0xFFFFFFFC;
        *reinterpret_cast<std::uint32_t *>(data.data() + offset) = inst;
    }

    void make_u16(std::vector<std::uint8_t> &data, std::size_t offset, std::uint16_t value) {
        *reinterpret_cast<std::uint16_t *>(data.data() + offset) = value;
    }

    const std::uint8_t zero_pad[0x40] = { };

    class HachiPatch : public Patch {
    public:
        HachiPatch(const IOSUFSA &fsa, std::string_view title) :
            fsa(fsa), path(util::concat_sv({ title, hachi_file })) { }
        virtual ~HachiPatch() override = default;

        virtual void Read() override {
            LOG("Open RPX");
            IOSUFSA::File rpx(fsa);
            if (!rpx.open(path, "rb")) throw error("RPX: Read FileOpen");

            LOG("Read Header");
            if (!rpx.readall(&ehdr, sizeof(ehdr))) throw error("RPX: Read Header");
            if (!util::memequal(ehdr, expected_ehdr)) throw error("RPX: Invalid Header");

            LOG("Read Sections Table");
            shdr.resize(ehdr.e_shnum);
            LOG("Read Sections Table - seek %X", ehdr.e_shoff);
            if (!rpx.seek(ehdr.e_shoff)) throw error("RPX: Seek Sections");
            LOG("Read Sections Table - read");
            if (!rpx.readall(shdr)) throw error("RPX: Read Sections");
            LOG("Read Sections Table - done");

            LOG("Get Sorted Sections");
            sorted_sects.resize(ehdr.e_shnum);
            std::iota(sorted_sects.begin(), sorted_sects.end(), 0);
            sorted_sects.erase(std::remove_if(sorted_sects.begin(), sorted_sects.end(),
                               [this](std::size_t i) -> bool { return shdr[i].sh_offset == 0; }),
                               sorted_sects.end());
            std::sort(sorted_sects.begin(), sorted_sects.end(),
                      [this](std::size_t a, std::size_t b) -> bool
                      { return shdr[a].sh_offset < shdr[b].sh_offset; });
            LOG("Validate Sorted Sections");
            if (!good_layout(shdr, sorted_sects)) throw error("RPX: Bad Layout");

            sections.resize(ehdr.e_shnum);
            for (std::size_t i : sorted_sects) {
                if (shdr[i].sh_size > 0) {
                    LOG("Read Section %d", i);
                    sections[i].resize(shdr[i].sh_size);
                    if (!rpx.seek(shdr[i].sh_offset)) throw error("RPX: Seek Sect");
                    if (!rpx.readall(sections[i])) throw error("RPX: Read Sect");
                }
            }

            LOG("Close RPX");
            if (!rpx.close()) throw error("RPX: Read CloseFile");
        }

        virtual void Modify() override {
            Elf32_Shdr &text_hdr = shdr[2];
            std::vector<std::uint8_t> &text = sections[2];

            LOG("Decompress Text");
            if (!decompress_sect(text_hdr, text)) throw error("RPX: Decompress Text");

            LOG("Patch Loaded Text");
            make_u16(text, 0x006CEA, 0x6710);
            make_b(  text, 0x00CC1C, text.size() + off_inject_apply_angle - off_inject_start);
            make_b(  text, 0x01DA28, text.size() + off_inject_comp_angle - off_inject_start);
            make_u16(text, 0x01DA42, 0x0018); // vpad->rstick.y offset
            make_u16(text, 0x01DAD2, 0x0014); // vpad->rstick.x offset
            make_u16(text, 0x03ED0E, 0x0BB0);
            make_b(  text, 0x050938, text.size() + off_inject_init_angle - off_inject_start);
            make_b(  text, 0x053F70, text.size() + off_inject_get_angle - off_inject_start);
            text.insert(text.end(), inject_bin, inject_bin_end);
            text_hdr.sh_size += inject_bin_size;

            LOG("CRC Calc");
            std::uint32_t crc = Zlib::crc32(text);
            reinterpret_cast<std::uint32_t *>(sections[27].data())[2] = crc;

            LOG("Compress Text");
            if (!compress_sect(text_hdr, text)) throw error("RPX: Compress Text");

            LOG("Shift for Resize");
            shift_for_resize(2, shdr, sorted_sects);
        }

        virtual void Write() override {
            LOG("Open RPX Write");
            IOSUFSA::File rpx(fsa);
            if (!rpx.open(path, "wb"))  throw error("RPX: Write FileOpen");

            LOG("Write Header");
            if (!rpx.writeall(&ehdr, sizeof(ehdr))) throw error("RPX: Write Header");

            LOG("Write Pad");
            if (!rpx.writeall(&magic_amds, sizeof(magic_amds))) throw error("RPX: Write Magic");
            if (!rpx.writeall(zero_pad, ehdr.e_shoff - sizeof(ehdr) - sizeof(magic_amds)))
                throw error("RPX: Write ShPad");

            LOG("Write Section Table");
            if (!rpx.writeall(shdr)) throw error("RPX: Write Sections");

            std::uint32_t last_off = 0x40 + sizeof(Elf32_Shdr) * shdr.size();
            for (std::size_t i : sorted_sects) {
                if (shdr[i].sh_size > 0) {
                    LOG("Write Section %d", i);
                    const Elf32_Shdr &sect = shdr[i];
                    if (!rpx.writeall(zero_pad, sect.sh_offset - last_off))
                        throw error("RPX: Write StPad");
                    if (!rpx.writeall(sections[i])) throw error("RPX: Write Sect");
                    last_off = sect.sh_offset + sect.sh_size;
                }
            }
            if (!rpx.writeall(zero_pad, -last_off & 0x3F)) throw error("RPX: Write FlPad");

            LOG("Close RPX Write");
            if (!rpx.close()) throw error("RPX: Write FileClose");
        }

    private:
        const IOSUFSA &fsa;
        std::string path;

        Elf32_Ehdr ehdr;
        std::vector<Elf32_Shdr> shdr;
        std::vector<std::size_t> sorted_sects;
        std::vector<std::vector<std::uint8_t>> sections;
    };
}

#define ret(X) do { rpx.close(); return X; } while(0)

Patch::Status hachi_check(const IOSUFSA &fsa, std::string_view title) {
    std::string rpx_path = util::concat_sv({ title, hachi_file });
    LOG("Open RPX");
    IOSUFSA::File rpx(fsa);
    if (!rpx.open(rpx_path, "rb")) ret(Patch::Status::MISSING_RPX);

    LOG("Read Header");
    Elf32_Ehdr ehdr;
    if (!rpx.readall(&ehdr, sizeof(ehdr))) ret(Patch::Status::INVALID_RPX);
    if (!util::memequal(ehdr, expected_ehdr)) ret(Patch::Status::INVALID_RPX);

    LOG("Read Patch Signature");
    std::uint32_t sig;
    if (!rpx.readall(&sig, sizeof(sig))) ret(Patch::Status::INVALID_RPX);
    if (sig == magic_amds) ret(Patch::Status::PATCHED);

    LOG("Read CRC Section Header");
    Elf32_Shdr crc_shdr;
    if (!rpx.seek(ehdr.e_shoff + 27 * sizeof(Elf32_Shdr))) ret(Patch::Status::INVALID_RPX);
    if (!rpx.readall(&crc_shdr, sizeof(crc_shdr))) ret(Patch::Status::INVALID_RPX);
    if (crc_shdr.sh_type != RPX_CRCS) ret(Patch::Status::INVALID_RPX);
    if (crc_shdr.sh_size != sizeof(expected_crcs)) ret(Patch::Status::INVALID_RPX);

    LOG("Read CRC Data");
    std::array<std::uint32_t, expected_ehdr.e_shnum> crcs;
    if (!rpx.seek(crc_shdr.sh_offset)) ret(Patch::Status::INVALID_RPX);
    if (!rpx.readall(&crcs, sizeof(crcs))) ret(Patch::Status::INVALID_RPX);
    if (!util::memequal(crcs, expected_crcs)) ret(Patch::Status::INVALID_RPX);

    LOG("HACHI GOOD");
    ret(Patch::Status::RPX_ONLY);
}

std::unique_ptr<Patch> hachi_patch(const IOSUFSA &fsa, std::string_view title) {
    return std::make_unique<HachiPatch>(fsa, title);
}

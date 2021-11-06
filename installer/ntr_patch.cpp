#include "ntr_patch.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <machine/endian.h>

#include "exception.hpp"
#include "iosufsa.hpp"
#include "log.hpp"
#include "util.hpp"
#include "zlib.hpp"

#include "any_pat_bin.h"
#include "get_analog_bin.h"

using namespace std::string_view_literals;

namespace {
    // Overloads for automatically selecting the correct sized bswap
    inline __attribute__((always_inline)) std::uint16_t
        bswap(std::uint16_t v) { return __bswap16(v); }
    inline __attribute__((always_inline)) std::uint32_t
        bswap(std::uint32_t v) { return __bswap32(v); }

    struct sm64ds_offsets {
        std::uint32_t touch_buttons;
        std::uint32_t direction_input;
        std::uint32_t dpad_mapping;
        std::uint32_t draw_target;
        std::uint32_t draw_touch_buttons;
    };

    constexpr std::string_view zip_file = "/content/0010/rom.zip"sv;

    constexpr sm64ds_offsets patch_offsets[6] {
        { 0x020244EC, 0x0202B320, 0x02073324, 0x020F0D88, 0x020F2584 }, // ASMEr0
        { 0x02024724, 0x0202B5A8, 0x020738C8, 0x020F0588, 0x020F1D98 }, // ASMJr0
        { 0x02024A68, 0x0202B3E4, 0x0206FF14, 0x020F9780, 0x020FAFB4 }, // ASMK
        { 0x02024D60, 0x0202C404, 0x02075658, 0x020FA7C0, 0x020FC0C0 }, // ASMP
        { 0x02024760, 0x0202B5E4, 0x02074044, 0x020F2848, 0x020F4058 }, // ASMEr1
        { 0x0202475C, 0x0202B5E0, 0x02074168, 0x020F1768, 0x020F2F78 }, // ASMJr1
    };

    struct zip_local {
        std::uint32_t signature;
        std::uint16_t min_ver;
        std::uint16_t flags;
        std::uint16_t method;
        std::uint16_t mod_time;
        std::uint16_t mod_date;
        std::uint32_t crc;
        std::uint32_t cmp_size;
        std::uint32_t dec_size;
        std::uint16_t name_len;
        std::uint16_t extra_len;
    } __attribute__((packed));
    static_assert(sizeof(zip_local) == 30);
    constexpr std::uint32_t zip_local_magic = util::magic_const("PK\03\04");

    struct zip_central {
        std::uint32_t signature;
        std::uint16_t gen_ver;
        std::uint16_t min_ver;
        std::uint16_t flags;
        std::uint16_t method;
        std::uint16_t mod_time;
        std::uint16_t mod_date;
        std::uint32_t crc;
        std::uint32_t cmp_size;
        std::uint32_t dec_size;
        std::uint16_t name_len;
        std::uint16_t extra_len;
        std::uint16_t comment_len;
        std::uint16_t disk_start;
        std::uint16_t int_attr;
        std::uint32_t ext_attr;
        std::uint32_t local_offset;
    } __attribute__((packed));
    static_assert(sizeof(zip_central) == 46);
    constexpr std::uint32_t zip_central_magic = util::magic_const("PK\01\02");

    struct zip_end {
        std::uint32_t signature;
        std::uint16_t disk;
        std::uint16_t central_disk;
        std::uint16_t central_count;
        std::uint16_t central_count_total;
        std::uint32_t central_size;
        std::uint32_t central_offset;
        std::uint16_t comment_len;
    } __attribute__((packed));
    static_assert(sizeof(zip_end) == 22);
    constexpr std::uint32_t zip_end_magic = util::magic_const("PK\05\06");

    void make_b(std::vector<std::uint8_t> &data, std::size_t data_offset,
                       std::uint32_t target_inst_offset) {
        std::uint32_t inst = 0xEA000000 | target_inst_offset;
        *reinterpret_cast<std::uint32_t *>(data.data() + data_offset) = bswap(inst);
    }

    void make_mov(std::vector<std::uint8_t> &data, std::size_t offset,
                         std::uint8_t dest, std::uint8_t rot, std::uint8_t imm) {
        std::uint32_t inst = 0xE3A00000 | (dest << 12) | ((rot / 2) << 8) | imm;
        *reinterpret_cast<std::uint32_t *>(data.data() + offset) = bswap(inst);
    }

    void make_u16(std::vector<std::uint8_t> &data, std::size_t offset, std::uint16_t value) {
        *reinterpret_cast<std::uint16_t *>(data.data() + offset) = bswap(value);
    }

    void make_u32(std::vector<std::uint8_t> &data, std::size_t offset, std::uint32_t value) {
        *reinterpret_cast<std::uint32_t *>(data.data() + offset) = bswap(value);
    }

    // Start of padding shared by all versions
    constexpr std::size_t any_pat_off = 0x65A0;
    // Shared padding length, maximum length for anypat injection
    constexpr std::size_t any_pat_len = 0xA60;
    // Bitmask indicating an overlay instruction patch
    constexpr std::uint32_t overlay_pat = 0x80000000;

    class NtrPatch : public Patch {
    public:
        NtrPatch(const IOSUFSA &fsa, std::string_view title) :
             fsa(fsa), path(util::concat_sv({ title, zip_file })) { }
        virtual ~NtrPatch() override = default;

        virtual void Read() override {
            LOG("Open ZIP");
            IOSUFSA::File zip(fsa);
            if (!zip.open(path, "rb")) throw error("NTR: Read FileOpen");

            LOG("Read Local Header");
            if (!zip.readall(&local, sizeof(local))) throw error("NTR: Read Local");
            if (local.signature != zip_local_magic) throw error("NTR: Bad Local");
            if (bswap(local.method) != 0 && bswap(local.method) != 8)
                throw error("NTR: Bad Local");
            local_name.resize(bswap(local.name_len));
            if (!zip.readall(local_name)) throw error("NTR: Read Local Name");
            local_extra.resize(bswap(local.extra_len));
            if (!zip.readall(local_extra)) throw error("NTR: Read Local Extra");

            LOG("Read NTR");
            data.resize(bswap(local.cmp_size));
            if (!zip.readall(data)) throw error("NTR: Read NTR");

            LOG("Read Central");
            if (!zip.readall(&central, sizeof(central))) throw error("NTR: Read Central");
            if (central.signature != zip_central_magic) throw error("NTR: Bad Central");
            central_name.resize(bswap(central.name_len));
            if (!zip.readall(central_name)) throw error("NTR: Read Central Name");
            central_extra.resize(bswap(central.extra_len));
            if (!zip.readall(central_extra)) throw error("NTR: Read Central Extra");
            central_comment.resize(bswap(central.comment_len));
            if (!zip.readall(central_comment)) throw error("NTR: Read Central Comment");

            LOG("Read End");
            if (!zip.readall(&end, sizeof(end))) throw error("NTR: Read End");
            if (end.signature != zip_end_magic) throw error("NTR: Bad End");
            end.comment_len = bswap(std::uint16_t{0});

            LOG("Close NTR");
            if (!zip.close()) throw error("NTR: Read CloseFile");
        }

        virtual void Modify() override {
            if (bswap(local.method) == 8) {
                LOG("Decompress NTR");
                data = Zlib::decompress(data, bswap(local.dec_size), false);
            }

            LOG("Identify NTR");
            // Magic Hash
            const sm64ds_offsets &offsets = patch_offsets[((data[0x0F] - 1) & 0x3) | (data[0x1E] << 2)];

            LOG("Patch NTR");
            make_b(data, 0x495C, (any_pat_off - 0x4964) / 4);
            std::memcpy(data.data() + any_pat_off, any_pat_bin, any_pat_bin_size);
            std::size_t off = any_pat_off + any_pat_bin_size;

            make_u32(data, off, 1);
            make_u32(data, off + 4, offsets.touch_buttons);
            make_b(  data, off + 8, 0x0C);
            off += 12;

            make_u32(data, off, get_analog_bin_size / 4);
            make_u32(data, off + 4, offsets.direction_input);
            std::memcpy(data.data() + off + 8, get_analog_bin, get_analog_bin_size);
            off += 8 + get_analog_bin_size;

            make_u32(data, off, 4 / 2);
            make_u32(data, off + 4, offsets.dpad_mapping);
            make_u16(data, off +  8, 0x0100);
            make_u16(data, off + 10, 0x0200);
            make_u16(data, off + 12, 0x0000);
            make_u16(data, off + 14, 0x0000);
            off += 16;

            make_u32(data, off, overlay_pat | 1);
            make_u32(data, off +  4, offsets.draw_target);
            make_u32(data, off +  8, 0xE7D22001); // Overwritten Instruction
            make_mov(data, off + 12, 2, 0, 0); // mov r2, #0
            off += 16;

            make_u32(data, off, overlay_pat | 1);
            make_u32(data, off +  4, offsets.draw_touch_buttons);
            make_u32(data, off +  8, 0xE19100B0); // Overwritten Instruction
            make_b(  data, off + 12, 0x70);
            off += 16;

            LOG("Calc CRC");
            std::uint32_t crc = Zlib::crc32(data);
            local.crc = central.crc = bswap(crc);

            LOG("Compress NTR");
            {
                std::vector<std::uint8_t> cmp = Zlib::compress(data, false);
                if (cmp.size() < data.size()) {
                    data = std::move(cmp);
                    local.method = central.method = bswap(std::uint16_t{8});
                } else {
                    local.method = central.method = bswap(std::uint16_t{0});
                }
            }
            local.cmp_size = central.cmp_size = bswap(std::uint32_t{data.size()});

            central.local_offset = bswap(std::uint32_t{0});
            std::uint32_t central_off = sizeof(local) + local_name.size() +
                                        local_extra.size() + data.size();
            end.central_offset = bswap(central_off);
        }

        virtual void Write() override {
            LOG("Open ZIP Write");
            IOSUFSA::File zip(fsa);
            if (!zip.open(path, "wb")) throw error("NTR: Write OpenFile");

            LOG("Write Local");
            if (!zip.writeall(&local, sizeof(local))) throw error("NTR: Write Local");
            if (!zip.writeall(local_name)) throw error("NTR: Write Local Name");
            if (!zip.writeall(local_extra)) throw error("NTR: Write Local Extra");

            LOG("Write NTR");
            if (!zip.writeall(data)) throw error("NTR: Write Data");

            LOG("Write Central");
            if (!zip.writeall(&central, sizeof(central))) throw error("NTR: Write Central");
            if (!zip.writeall(central_name)) throw error("NTR: Write Central Name");
            if (!zip.writeall(central_extra)) throw error("NTR: Write Central Extra");
            if (!zip.writeall(central_comment)) throw error("NTR: Write Central Comment");

            LOG("Write End");
            if (!zip.writeall(&end, sizeof(end))) throw error("NTR: Write End");

            LOG("Close ZIP Write");
            if (!zip.close()) throw error("NTR: Write CloseFile");
        }

    private:
        const IOSUFSA &fsa;
        std::string path;

        zip_local local;
        std::vector<std::uint8_t> local_name;
        std::vector<std::uint8_t> local_extra;
        std::vector<std::uint8_t> data;
        zip_central central;
        std::vector<std::uint8_t> central_name;
        std::vector<std::uint8_t> central_extra;
        std::vector<std::uint8_t> central_comment;
        zip_end end;
    };
}

#define ret(X) do { zip.close(); return X; } while(0)

Patch::Status ntr_check(const IOSUFSA &fsa, std::string_view title) {
    std::string zip_path = util::concat_sv({ title, zip_file });
    LOG("Open ZIP");
    IOSUFSA::File zip(fsa);
    if (!zip.open(zip_path, "rb")) ret(Patch::Status::INVALID_ZIP);

    LOG("Read Local Header");
    zip_local local;
    if (!zip.readall(&local, sizeof(local))) ret(Patch::Status::INVALID_ZIP);
    if (local.signature != zip_local_magic) ret(Patch::Status::INVALID_ZIP);
    if (bswap(local.method) != 0 && bswap(local.method) != 8) ret(Patch::Status::INVALID_ZIP);
    if (!zip.skip(bswap(local.name_len) + bswap(local.extra_len))) ret(Patch::Status::INVALID_ZIP);

    LOG("Read NTR");
    std::vector<std::uint8_t> data(bswap(local.cmp_size));
    if (!zip.readall(data)) ret(Patch::Status::INVALID_ZIP);

    LOG("Read Central");
    zip_central central;
    if (!zip.readall(&central, sizeof(central))) ret(Patch::Status::INVALID_ZIP);
    if (central.signature != zip_central_magic) ret(Patch::Status::INVALID_ZIP);
    if (!zip.skip(bswap(central.name_len) + bswap(central.extra_len) +
                  bswap(central.comment_len))) ret(Patch::Status::INVALID_ZIP);

    LOG("Read End");
    zip_end end;
    if (!zip.readall(&end, sizeof(end))) ret(Patch::Status::INVALID_ZIP);
    if (end.signature != zip_end_magic) ret(Patch::Status::INVALID_ZIP);

    LOG("Close NTR");
    if (!zip.close()) ret(Patch::Status::INVALID_ZIP);

    if (bswap(local.method) != bswap(central.method)) ret(Patch::Status::INVALID_ZIP);
    if (bswap(local.method) == 8) {
        LOG("Decompress NTR");
        data = Zlib::decompress(data, bswap(local.dec_size), false);
    } else if (bswap(local.method) != 0) ret(Patch::Status::INVALID_ZIP);

    LOG("Check ROM Title");
    std::uint32_t code = *reinterpret_cast<std::uint32_t *>(data.data() + 0x0C);
    std::uint16_t maker = *reinterpret_cast<std::uint16_t *>(data.data() + 0x10);
    std::uint8_t revision = *reinterpret_cast<std::uint8_t *>(data.data() + 0x1E);
    switch (code) {
        case util::magic_const("ASMJ"):
        case util::magic_const("ASME"):
            if ((maker != 0x3031) || ((revision != 0) && (revision != 1)))
                ret(Patch::Status::INVALID_NTR);
            break;
        case util::magic_const("ASMP"):
        case util::magic_const("ASMK"):
            if ((maker != 0x3031) || (revision != 0)) ret(Patch::Status::INVALID_NTR);
            break;
        case util::magic_const("HAXX"):
            ret(Patch::Status::IS_HAXCHI);
        default:
            ret(Patch::Status::INVALID_NTR);
    }

    LOG("Check Padding");
    for (std::size_t i = 0; i < any_pat_len; ++i) {
        if (data[any_pat_off + i] != 0) ret(Patch::Status::INVALID_NTR);
    }

    LOG("NTR GOOD");
    switch (code) {
        case util::magic_const("ASMJ"): ret(Patch::Status::IS_JPN);
        case util::magic_const("ASME"): ret(Patch::Status::IS_USA);
        case util::magic_const("ASMP"): ret(Patch::Status::IS_EUR);
        case util::magic_const("ASMK"): ret(Patch::Status::IS_KOR);
    }
    ret(Patch::Status::INVALID_NTR);
};

std::unique_ptr<Patch> ntr_patch(const IOSUFSA &fsa, std::string_view title) {
    return std::make_unique<NtrPatch>(fsa, title);
}

#include "iosufsa.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include <coreinit/ios.h>
#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "aligned.hpp"
#include "log.hpp"

namespace {
    constexpr std::int32_t IOCTL_CHECK_IF_IOSUHAX = 0x5B;
    constexpr std::int32_t IOSUHAX_MAGIC_WORD = 0x4E696365;

    constexpr std::int32_t IOCTL_FSA_OPEN = 0x40;
    constexpr std::int32_t IOCTL_FSA_CLOSE = 0x41;

    constexpr std::int32_t IOCTL_FSA_REMOVE = 0x50;
    constexpr std::int32_t IOCTL_FSA_FLUSHVOLUME = 0x59;

    constexpr std::int32_t IOCTL_FSA_OPENFILE = 0x49;
    constexpr std::int32_t IOCTL_FSA_CLOSEFILE = 0x4D;
    constexpr std::int32_t IOCTL_FSA_READFILE = 0x4A;
    constexpr std::int32_t IOCTL_FSA_WRITEFILE = 0x4B;
    constexpr std::int32_t IOCTL_FSA_SETFILEPOS = 0x4E;

    constexpr std::int32_t ERROR_INVALID_ARG = -0x1D;

    constexpr std::size_t max_io = 0x10'0000; // 1MiB

    template<std::size_t Align>
    aligned::vector<std::uint8_t, Align> make_msg_strings(
            std::int32_t fsa_fd, std::initializer_list<std::string_view> strings) {

        const std::size_t header_size = sizeof(std::int32_t) * (1 + strings.size());
        std::size_t size = header_size + strings.size();
        for (std::string_view str : strings) size += str.length();

        aligned::vector<std::uint8_t, Align> msg(size);
        std::int32_t *header = reinterpret_cast<std::int32_t *>(msg.data());
        header[0] = fsa_fd;

        std::size_t i = 1;
        std::int32_t off = header_size;
        for (std::string_view str : strings) {
            header[i++] = off;
            std::memcpy(msg.data() + off, str.data(), str.length());
            msg[off + str.length()] = '\0';
            off += str.length() + 1;
        }

        return msg;
    }

    void nullfuncvp(IOSError, void *) { }
}

IOSUFSA::~IOSUFSA() {
    if (is_open()) try {
        LOG("IOSUFSA destructed while open");
        close();
    } catch (error &e) {
        LOG("ERROR in ~IOSUFSA: %s", e.what());
    }
}

bool IOSUFSA::open_dev() {
    iosu_fd = IOS_Open("/dev/iosuhax", (IOSOpenMode) 0);
    return (iosu_fd >= 0);
}

bool IOSUFSA::close_dev() {
    int res = IOS_Close(iosu_fd);
    iosu_fd = -1;
    return (res >= 0);
}

bool IOSUFSA::open_mcp() {
    mcp_fd = MCP_Open();
    if (mcp_fd < 0) return false;

    IOS_IoctlAsync(mcp_fd, 0x62, nullptr, 0, nullptr, 0, nullfuncvp, nullptr);
    OSSleepTicks(OSSecondsToTicks(1));

    iosu_fd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
    if (iosu_fd < 0) {
        // Cleanup MCP
        MCP_Close(mcp_fd);
        mcp_fd = -1;
        return false;
    }

    // Check if actually IOSUHAX
    alignas(0x40) std::int32_t recv[1];
    recv[0] = 0;
    IOS_Ioctl(iosu_fd, IOCTL_CHECK_IF_IOSUHAX, nullptr, 0, recv, sizeof(recv));
    if (recv[0] != IOSUHAX_MAGIC_WORD) {
        close_mcp();
        return false;
    }

    return true;
}

bool IOSUFSA::close_mcp() {
    bool good = true;

    int res = IOS_Close(iosu_fd);
    iosu_fd = -1;
    good &= (res >= 0);
    OSSleepTicks(OSSecondsToTicks(1));

    MCP_Close(mcp_fd);
    mcp_fd = -1;
    good &= (res >= 0);

    return good;
}

void IOSUFSA::open() {
    if (is_open()) return;

    bool iosu = open_dev();
    if (!iosu) iosu = open_mcp();
    if (!iosu) throw no_iosuhax{};

    // Init FSA
    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(iosu_fd, IOCTL_FSA_OPEN, nullptr, 0, recv, sizeof(recv));
    if (res < 0 || recv[0] < 0) {
        // Cleanup IOSUHAX
        if (mcp_fd) close_mcp();
        else close_dev();
        throw error("IOSUHAX: Open FSA");
    }
    fsa_fd = recv[0];
}

void IOSUFSA::close() {
    if (!is_open()) return;

    alignas(0x40) std::int32_t recv[0x20 >> 2];
    recv[0] = fsa_fd;
    int res = IOS_Ioctl(iosu_fd, IOCTL_FSA_CLOSE, recv, sizeof(recv), recv, sizeof(recv));
    fsa_fd = -1;
    bool fsa_good = (res >= 0);

    bool iosu_good;
    if (mcp_fd >= 0) iosu_good = close_mcp();
    else iosu_good = close_dev();

    if (!iosu_good) throw error("IOSUHAX: Close HAX");
    if (!fsa_good) throw error("IOSUHAX: Close FSA");
}

bool IOSUFSA::remove(std::string_view path) const {
    if (!is_open()) throw error("IOSUHAX: Remove: Not Open");

    aligned::vector<std::uint8_t, 0x40> msg = make_msg_strings<0x40>(fsa_fd, { path });

    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(iosu_fd, IOCTL_FSA_REMOVE, msg.data(), msg.size(), recv, sizeof(recv));
    if (res < 0) throw error("IOSUHAX: Remove: IOS_Ioctl Failed");

    return (recv[0] >= 0);
}

bool IOSUFSA::flush_volume(std::string_view path) const {
    if (!is_open()) throw error("IOSUHAX: FlushVolume: Not Open");
    if (mcp_fd < 0) return true;

    aligned::vector<std::uint8_t, 0x40> msg = make_msg_strings<0x40>(fsa_fd, { path });

    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(iosu_fd, IOCTL_FSA_FLUSHVOLUME, msg.data(), msg.size(), recv, sizeof(recv));
    // Mocha lacks the command, so soft-fail in this case
    if (res == ERROR_INVALID_ARG) return false;
    if (res < 0) throw error("IOSUHAX: FlushVolume: IOS_Ioctl Failed");

    return (recv[0] >= 0);
}

IOSUFSA::File::~File() {
    if (is_open()) try {
        LOG("File destructed while open");
        close();
    } catch (error &e) {
        LOG("ERROR in ~File: %s", e.what());
    }
}

bool IOSUFSA::File::open(std::string_view path, std::string_view mode) {
    if (!fsa.is_open()) throw error("IOSUHAX: FileOpen: FSA Not Open");
    if (file_fd >= 0) close();

    aligned::vector<std::uint8_t, 0x40> msg = make_msg_strings<0x40>(fsa.fsa_fd, { path, mode });

    alignas(0x40) std::int32_t recv[2];
    int res = IOS_Ioctl(fsa.iosu_fd, IOCTL_FSA_OPENFILE, msg.data(), msg.size(), recv, sizeof(recv));
    if (res < 0) throw error("IOSUHAX: FileOpen: IOS_Ioctl Failed");

    if (recv[0] >= 0) {
        if (recv[1] < 0) throw error("IOSUHAX: FileOpen: Bad Handle");
        file_fd = recv[1];
        return true;
    } else return false;
}

bool IOSUFSA::File::close() {
    if (!is_open()) return true;
    if (!fsa.is_open()) throw error("IOSUHAX: FileClose: FSA Not Open");

    alignas(0x40) std::int32_t msg[2];
    msg[0] = fsa.fsa_fd;
    msg[1] = file_fd;

    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(fsa.iosu_fd, IOCTL_FSA_CLOSEFILE, msg, sizeof(msg), recv, sizeof(recv));
    file_fd = -1;

    if (res < 0) throw error("IOSUHAX: FileClose: IOS_Ioctl Failed");
    return (recv[0] >= 0);
}

std::int32_t IOSUFSA::File::read_impl(void *data, std::size_t size, std::size_t count,
                                      aligned::vector<std::uint8_t, 0x40> &buffer) const {
    alignas(0x40) std::int32_t msg[5];
    msg[0] = fsa.fsa_fd;
    msg[1] = size;
    msg[2] = count;
    msg[3] = file_fd;
    msg[4] = 0;

    aligned::vector<std::uint8_t, 0x40> &recv = buffer;
    recv.resize((size * count + 0x7F) & ~0x3F);
    int res = IOS_Ioctl(fsa.iosu_fd, IOCTL_FSA_READFILE, msg, sizeof(msg), recv.data(), recv.size());
    if (res < 0) throw error("IOSUHAX: FileRead: IOS_Ioctl Failed");

    std::int32_t out = reinterpret_cast<std::int32_t *>(recv.data())[0];
    if (data && out > 0) std::memcpy(data, recv.data() + 0x40, size * count);
    return out;
}

std::int32_t IOSUFSA::File::read(void *data, std::size_t size, std::size_t count) const {
    if (!is_open()) throw error("IOSUHAX: FileRead: Not Open");

    aligned::vector<std::uint8_t, 0x40> buffer;
    return read_impl(data, size, count, buffer);
}

bool IOSUFSA::File::readall(void *data, std::size_t size) const {
    if (!is_open()) throw error("IOSUHAX: FileReadAll: Not Open");

    aligned::vector<std::uint8_t, 0x40> buffer;
    unsigned char *bdata = reinterpret_cast<unsigned char *>(data);
    while (size > 0) {
        std::int32_t count = read_impl(bdata, 1, std::min(max_io, size), buffer);
        if (count <= 0) return false;
        bdata += count;
        std::size_t uread = static_cast<std::size_t>(count);
        size = __builtin_expect(uread <= size, true) ? (size - uread) : 0;
    }
    return true;
}

bool IOSUFSA::File::skip(std::size_t size) const {
    if (!is_open()) throw error("IOSUHAX: FileSkip: Not Open");

    aligned::vector<std::uint8_t, 0x40> buffer;
    while (size > 0) {
        std::int32_t count = read_impl(nullptr, 1, std::min(max_io, size), buffer);
        if (count <= 0) return false;
        std::size_t uread = static_cast<std::size_t>(count);
        size = __builtin_expect(uread <= size, true) ? (size - uread) : 0;
    }
    return true;
}

std::int32_t IOSUFSA::File::write_impl(const void *data, std::size_t size, std::size_t count,
                                       aligned::vector<std::uint8_t, 0x40> &buffer) const {
    aligned::vector<std::uint8_t, 0x40> &msg = buffer;
    msg.resize((sizeof(std::int32_t) * 5 + size * count + 0x7F) & ~0x3F);
    std::int32_t *header = reinterpret_cast<std::int32_t *>(msg.data());
    header[0] = fsa.fsa_fd;
    header[1] = size;
    header[2] = count;
    header[3] = file_fd;
    header[4] = 0;
    std::memcpy(msg.data() + 0x40, data, size * count);

    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(fsa.iosu_fd, IOCTL_FSA_WRITEFILE, msg.data(), msg.size(), recv, sizeof(recv));
    if (res < 0) throw error("IOSUHAX: FileWrite: IOS_Ioctl Failed");

    return recv[0];
}

std::int32_t IOSUFSA::File::write(const void *data, std::size_t size, std::size_t count) const {
    if (!is_open()) throw error("IOSUHAX: FileWrite: Not Open");

    aligned::vector<std::uint8_t, 0x40> buffer;
    return write_impl(data, size, count, buffer);
}

bool IOSUFSA::File::writeall(const void *data, std::size_t size) const {
    if (!is_open()) throw error("IOSUHAX: FileWriteAll: Not Open");

    aligned::vector<std::uint8_t, 0x40> buffer;
    const unsigned char *bdata = reinterpret_cast<const unsigned char *>(data);
    while (size > 0) {
        std::int32_t count = write_impl(bdata, 1, std::min(max_io, size), buffer);
        if (count <= 0) return false;
        bdata += count;
        std::size_t uwrote = static_cast<std::size_t>(count);
        size = __builtin_expect(uwrote <= size, true) ? (size - uwrote) : 0;
    }
    return true;
}

bool IOSUFSA::File::seek(std::size_t position) const {
    if (!is_open()) throw error("IOSUHAX: FileSeek: Not Open");

    alignas(0x40) std::int32_t msg[3];
    msg[0] = fsa.fsa_fd;
    msg[1] = file_fd;
    msg[2] = position;

    alignas(0x40) std::int32_t recv[1];
    int res = IOS_Ioctl(fsa.iosu_fd, IOCTL_FSA_SETFILEPOS, msg, sizeof(msg), recv, sizeof(recv));
    if (res < 0) throw error("IOSUHAX: FileSeek: IOS_Ioctl Failed");

    return (recv[0] >= 0);
}

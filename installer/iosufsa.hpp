#ifndef IOSUFSA_HPP
#define IOSUFSA_HPP

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "aligned.hpp"
#include "error.hpp"

class IOSUFSA {
public:
    IOSUFSA() = default;
    ~IOSUFSA() { if (is_open()) close(); }

    // IOSUFSA instances need stable addresses for consistent references,
    // so the class is non-copyable and non-movable.
    IOSUFSA(const IOSUFSA &) = delete;
    IOSUFSA &operator=(const IOSUFSA &) = delete;
    IOSUFSA(IOSUFSA &&) = delete;
    IOSUFSA &operator=(IOSUFSA &&) = delete;

    void open();
    void close();
    bool is_open() const noexcept { return fsa_fd >= 0; }

    bool remove(std::string_view path) const;
    bool flush_volume(std::string_view path) const;

    class File {
    public:
        explicit File(const IOSUFSA &fsa) : fsa(fsa) { }
        ~File() { if (is_open()) close(); }

        // Move Only
        File(const File &) = delete;
        File &operator=(const File &) = delete;
        File(File &&o) : fsa(o.fsa), file_fd(o.file_fd) { o.file_fd = -1; }
        File &operator=(File &&o) {
            if (std::addressof(o.fsa) != std::addressof(fsa))
                handle_error("Can't Move Files Between FSAs");
            close(); file_fd = o.file_fd; o.file_fd = -1; return *this;
        }

        bool open(std::string_view path, std::string_view mode);
        bool close();
        bool is_open() const noexcept { return file_fd >= 0; }

        std::int32_t read(void *data, std::size_t size, std::size_t count) const;
        bool readall(void *data, std::size_t size) const;
        template<typename T> bool readall(std::vector<T> &v) const
            { return readall(v.data(), sizeof(T) * v.size()); }
        bool skip(std::size_t size) const;

        std::int32_t write(const void *data, std::size_t size, std::size_t count) const;
        bool writeall(const void *data, std::size_t size) const;
        template<typename T> bool writeall(const std::vector<T> &v) const
            { return writeall(v.data(), sizeof(T) * v.size()); }

        bool seek(std::size_t position) const;

    private:
        const IOSUFSA &fsa;
        int file_fd = -1;

        std::int32_t read_impl(void *data, std::size_t size, std::size_t count,
                               aligned::vector<std::uint8_t, 0x40> &buffer) const;
        std::int32_t write_impl(const void *data, std::size_t size, std::size_t count,
                                aligned::vector<std::uint8_t, 0x40> &buffer) const;
    };

private:
    int iosu_fd = -1;
    int mcp_fd = -1;
    int fsa_fd = -1;

    bool open_dev();
    bool close_dev();
    bool open_mcp();
    bool close_mcp();
};

#endif // IOSUFSA_HPP

#ifndef TITLE_HPP
#define TITLE_HPP

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "iosufsa.hpp"
#include "patch.hpp"

class Title {
public:
    Title(std::uint64_t id, std::string path, std::string dev) :
        id(id), path(std::move(path)), dev(std::move(dev)) { }

    std::uint64_t get_id() const noexcept { return id; }
    const std::string &get_path() const noexcept { return path; }
    const std::string &get_dev() const noexcept { return dev; }
    const std::string &get_name() {
        if (!name.empty()) return name;
        else return (name = get_name_impl());
    }
    Patch::Status get_status(const IOSUFSA &fsa) {
        if (status != Patch::Status::UNTESTED) return status;
        else return (status = get_status_impl(fsa));
    }
    Patch::Status get_status_raw() const { return status; }
    void flag_patched() { status = Patch::Status::PATCHED; }

    static std::vector<Title> get_titles();

    using Filtered = std::vector<std::reference_wrapper<Title>>;
    template<typename Func>
    static Filtered filter(std::vector<Title> &titles, Func op) {
        Filtered filtered(titles.begin(), titles.end());
        filtered.erase(std::remove_if(filtered.begin(), filtered.end(), op), filtered.end());
        return filtered;
    }

private:
    const std::uint64_t id;
    const std::string path;
    const std::string dev;
    std::string name;
    Patch::Status status = Patch::Status::UNTESTED;

    std::string get_name_impl();
    Patch::Status get_status_impl(const IOSUFSA &fsa);
};

#endif // TITLE_HPP

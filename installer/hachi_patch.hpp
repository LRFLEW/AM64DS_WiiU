#ifndef HACHI_PATCH_HPP
#define HACHI_PATCH_HPP

#include <memory>
#include <string_view>

#include "iosufsa.hpp"
#include "patch.hpp"

Patch::Status hachi_check(const IOSUFSA &fsa, std::string_view title);
std::unique_ptr<Patch> hachi_patch(const IOSUFSA &fsa, std::string_view title);

#endif // HACHI_PATCH_HPP

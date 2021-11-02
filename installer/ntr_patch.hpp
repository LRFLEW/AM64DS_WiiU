#ifndef NTR_PATCH_HPP
#define NTR_PATCH_HPP

#include <string_view>

#include "iosufsa.hpp"
#include "patch.hpp"

Patch::Status ntr_check(const IOSUFSA &fsa, std::string_view title);
std::unique_ptr<Patch> ntr_patch(const IOSUFSA &fsa, std::string_view title);

#endif // NTR_PATCH_HPP

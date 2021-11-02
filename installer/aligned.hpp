#ifndef ALIGNED_HPP
#define ALIGNED_HPP

#include <cstdint>
#include <cstring>
#include <new>
#include <numeric>
#include <vector>

namespace aligned {

    template<typename T, std::size_t Align>
    class allocator {
    public:
        using value_type = T;

        allocator() = default;
        allocator(const allocator &) = default;
        allocator &operator=(const allocator &) = default;

        template<typename T2>
        allocator(const allocator<T2, Align> &) { }

        T *allocate(std::size_t count) {
            return static_cast<T *>(::operator new[](count * sizeof(T), type_align));
        }

        void deallocate(T *ptr, std::size_t count) {
            ::operator delete[](ptr, count * sizeof(T), type_align);
        }

        friend bool operator==(const allocator &, const allocator &) {
            return true;
        }
        friend bool operator!=(const allocator &, const allocator &) {
            return false;
        }

        template<typename U>
        class rebind {
        public:
            using other = allocator<U, Align>;
        };

    private:
        static constexpr std::align_val_t type_align {
            std::lcm(std::lcm(Align, alignof(T)), __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        };
    };

    template<typename T, std::size_t Align>
    using vector = ::std::vector<T, allocator<T, Align>>;
}

#endif // ALIGNED_HPP

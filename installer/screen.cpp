#include "screen.hpp"

#include <coreinit/cache.h>
#include <coreinit/screen.h>

#include <coreinit/memheap.h>
#include <coreinit/memfrmheap.h>

// Macro for calling functions on both screens
#define S(X) for (OSScreenID s = SCREEN_TV; s <= SCREEN_DRC; s = (OSScreenID) (s + 1)) { X }

namespace {
    constexpr std::uint32_t CONSOLE_FRAME_HEAP_TAG = 0x000DECAF;

    Screen *global_screen = nullptr;
}

Screen::Screen() {
    global_screen = this;
    OSScreenInit();
    S( len[s] = OSScreenGetBufferSizeEx(s); )
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    S( buf[s] = MEMAllocFromFrmHeapEx(heap, len[s], 0x100); )
    S( OSScreenSetBufferEx(s, buf[s]); )
    S( OSScreenEnableEx(s, true); )
}

Screen::~Screen() {
    S( OSScreenEnableEx(s, false); )
    OSScreenShutdown();
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    MEMFreeByStateToFrmHeap(heap, CONSOLE_FRAME_HEAP_TAG);
}

void Screen::put(std::uint32_t row, std::uint32_t column, const char *str) {
    // For some reason the row and column arguments are swapped in WUT
    S( OSScreenPutFontEx(s, column, row, str); )
}

void Screen::swap() {
    S( DCFlushRange(buf[s], len[s]); )
    S( OSScreenFlipBuffersEx(s); )
    S( OSScreenClearBufferEx(s, 0x000000); )
}

Screen &Screen::get_global() noexcept {
    return *global_screen;
}

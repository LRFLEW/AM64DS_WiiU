#include "screen.hpp"

#include <coreinit/cache.h>
#include <coreinit/screen.h>

#include <coreinit/memheap.h>
#include <coreinit/memfrmheap.h>

#include <proc_ui/procui.h>

// Macro for calling functions on both screens
#define S(X) for (OSScreenID s = SCREEN_TV; s <= SCREEN_DRC; s = (OSScreenID) (s + 1)) { X }

namespace {
    constexpr std::uint32_t CONSOLE_FRAME_HEAP_TAG = 0x000DECAF;
}

static void *sBufferTV = NULL, *sBufferDRC = NULL;
static uint32_t sBufferSizeTV = 0, sBufferSizeDRC = 0;
static BOOL sConsoleHasForeground = TRUE;

uint32_t
ConsoleProcCallbackAcquired(void *context)
{
   MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
   if (sBufferSizeTV) {
      sBufferTV = MEMAllocFromFrmHeapEx(heap, sBufferSizeTV, 4);
   }

   if (sBufferSizeDRC) {
      sBufferDRC = MEMAllocFromFrmHeapEx(heap, sBufferSizeDRC, 4);
   }

   sConsoleHasForeground = TRUE;
   OSScreenSetBufferEx(SCREEN_TV, sBufferTV);
   OSScreenSetBufferEx(SCREEN_DRC, sBufferDRC);
   return 0;
}

uint32_t
ConsoleProcCallbackReleased(void *context)
{
   MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
   MEMFreeByStateToFrmHeap(heap, CONSOLE_FRAME_HEAP_TAG);
   sConsoleHasForeground = FALSE;
   return 0;
}


Screen::Screen() {
   OSScreenInit();
   sBufferSizeTV = OSScreenGetBufferSizeEx(SCREEN_TV);
   sBufferSizeDRC = OSScreenGetBufferSizeEx(SCREEN_DRC);

   ConsoleProcCallbackAcquired(NULL);
   OSScreenEnableEx(SCREEN_TV, 1);
   OSScreenEnableEx(SCREEN_DRC, 1);

   ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, ConsoleProcCallbackAcquired, NULL, 100);
   ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, ConsoleProcCallbackReleased, NULL, 100);
}

Screen::~Screen() {
   if (sConsoleHasForeground) {
      OSScreenShutdown();
      ConsoleProcCallbackReleased(NULL);
   }
}

void Screen::put(std::uint32_t row, std::uint32_t column, const char *str) {
    if (!sConsoleHasForeground) {
      return;
    }
    // For some reason the row and column arguments are swapped in WUT
    S( OSScreenPutFontEx(s, column, row, str); )
}

void Screen::swap() {
    if (!sConsoleHasForeground) {
      return;
    }
   DCFlushRange(sBufferTV, sBufferSizeTV);
   DCFlushRange(sBufferDRC, sBufferSizeDRC);
   OSScreenFlipBuffersEx(SCREEN_TV);
   OSScreenFlipBuffersEx(SCREEN_DRC);   
   OSScreenClearBufferEx(SCREEN_TV, 0x000000);
   OSScreenClearBufferEx(SCREEN_DRC, 0x000000);
}

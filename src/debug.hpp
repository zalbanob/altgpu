#ifndef ALTGPU_DEBUG_H
#define ALTGPU_DEBUG_H

#include <atomic>
#include <mutex>
#include <cstdarg>
#include <cstdio>

#include "altgpu.h"

static std::atomic<bool> g_debug_enabled{false};
static std::mutex g_debug_mutex;

static void debug_printf(const char* fmt, ...) {
  if (!g_debug_enabled.load(std::memory_order_relaxed)) return;
  std::lock_guard<std::mutex> guard(g_debug_mutex);
  constexpr int BUF_SIZE = 1024;
  char buf[BUF_SIZE];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, BUF_SIZE, fmt, args);
  va_end(args);
  Rprintf("%s", buf);
}


static void set_debug_option() {
  SEXP opt = Rf_GetOption1(Rf_install("altgpu.debug"));
  if (!Rf_isNull(opt) && Rf_isLogical(opt) && LOGICAL(opt)[0] == TRUE) {
    g_debug_enabled.store(true, std::memory_order_relaxed);
  } else {
    g_debug_enabled.store(false, std::memory_order_relaxed);
  }
}

#endif //ALTGPU_DEBUG_H

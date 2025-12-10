#ifndef REPLAYBUFFER_TIMER_HPP
#define REPLAYBUFFER_TIMER_HPP

#if defined(GEODE_IS_WINDOWS64)

#include <Windows.h>

struct Timer {
  LARGE_INTEGER frequency{};
  LARGE_INTEGER begin{};

  Timer();
  void start();
  int64_t stop() const;
};

#else

struct Timer {
  std::chrono::high_resolution_clock begin;

  Timer();
  void start();
  int64_t stop() const;
};

#endif

#endif
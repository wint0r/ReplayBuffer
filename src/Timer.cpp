#include "Timer.hpp"

#if defined(GEODE_IS_WINDOWS64)

Timer::Timer() {
  QueryPerformanceFrequency(&this->frequency);
}

void Timer::start() {
  QueryPerformanceCounter(&this->begin);
}

int64_t Timer::stop() const {
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  return (end.QuadPart - this->begin.QuadPart) * 1000000 / this->frequency.QuadPart;
}

#else

Timer::Timer() {

}

void Timer::start() {
  this->begin = std::chrono::high_resolution_clock::now();
}

int64_t Timer::stop() const {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<int64_t, std::micro> duration_ms = end - this->begin;
  return duration_ms.count();
}

#endif
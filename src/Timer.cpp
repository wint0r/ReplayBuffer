#include "Timer.hpp"

#if defined(GEODE_IS_WINDOWS64)

Timer::Timer() {
  QueryPerformanceFrequency(&this->frequency);
}

void Timer::start() {
  QueryPerformanceCounter(&this->begin);
}

double Timer::stop() const {
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  return static_cast<double>(end.QuadPart - this->begin.QuadPart) * 1000.0 / this->frequency.QuadPart;
}

#else

Timer::Timer() {

}

void Timer::start() {
  t->start = std::chrono::high_resolution_clock::now();
}

double Timer::stop() const {
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration_ms = end - t->start;
  return duration_ms.count();
}

#endif
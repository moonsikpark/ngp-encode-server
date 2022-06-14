// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_SCOPED_TIMER_
#define NES_BASE_SCOPED_TIMER_

#include <chrono>

class ScopedTimer {
 public:
  using clock = std::chrono::steady_clock;
  using time_format = std::chrono::milliseconds;
  ScopedTimer() : _start(clock::now()) {}
  inline time_format elapsed() {
    return std::chrono::duration_cast<time_format>(clock::now() - _start);
  }

 private:
  std::chrono::time_point<clock> _start;
};

#endif  // NES_BASE_SCOPED_TIMER_

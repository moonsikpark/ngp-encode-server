// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_SCOPED_TIMER_
#define NES_BASE_SCOPED_TIMER_

#include <chrono>

// A timer operating inside a scope.
class ScopedTimer {
 public:
  using clock = std::chrono::steady_clock;
  using time_format = std::chrono::milliseconds;
  // Start the timer in a scope.
  ScopedTimer() : _start(clock::now()) {}

  // Returns the time elapsed since the timer start.
  inline time_format elapsed() {
    return std::chrono::duration_cast<time_format>(clock::now() - _start);
  }

 private:
  std::chrono::time_point<clock> _start;
};

#endif  // NES_BASE_SCOPED_TIMER_

// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_EXCEPTIONS_LOCK_TIMEOUT_
#define NES_BASE_EXCEPTIONS_LOCK_TIMEOUT_

#include <exception>

// Timeout reached while waiting for a lock.
class LockTimeout : public std::exception {
  virtual const char *what() const throw() {
    return "Waiting for lock timed out.";
  }
};

#endif  // NES_BASE_EXCEPTIONS_LOCK_TIMEOUT_

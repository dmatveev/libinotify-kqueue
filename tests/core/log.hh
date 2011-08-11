#ifndef __LOG_HH__
#define __LOG_HH__

#include <iostream>

void acquire_log_lock ();
void release_log_lock ();

unsigned int current_thread ();


#ifdef ENABLE_LOGGING
#  define LOG(X)                              \
      do {                                    \
          acquire_log_lock ();                \
          std::cout                           \
              << current_thread () << "    "  \
              << X                            \
              << std::endl;                   \
          release_log_lock ();                \
      } while (0)
#else
#  define LOG(X)
#endif // ENABLE_LOGGING

#endif // __LOG_HH__

/*******************************************************************************
  Copyright (c) 2011 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

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
#  define VAR(X) \
     '[' << #X << ": " << X << "] "
#else
#  define LOG(X)
#  define VAR(X)
#endif // ENABLE_LOGGING

#endif // __LOG_HH__

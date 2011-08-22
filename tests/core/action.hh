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

#ifndef __ACTION_HH__
#define __ACTION_HH__

#include "platform.hh"

class action {
    void init();

    pthread_mutex_t action_mutex;
    pthread_mutex_t cond_mutex;
    pthread_cond_t cond;

    const std::string name;
    volatile bool interrupted;
    volatile bool waiting;

public:
    action (const std::string &name_);
    virtual ~action () = 0;

    bool wait ();
    void interrupt ();
    void reset ();

    std::string named () const;
};

#endif // __ACTION_HH__

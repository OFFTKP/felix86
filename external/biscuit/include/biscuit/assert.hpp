#pragma once

// The asserting mechanism for biscuit can be overridden.
//
// The default assertion mechanism is simply a printf of the failing
// condition followed by an abort. However depending on one's particular
// setup, this might be unnecessary, odd, lacking, or some other
// undesirable quality.
//
// This allows the user to specify their own assertion handler to be used
// in place of the default. The user can even opt to define an empty handler
// should the checking prove to slow down performance (and you're reeeeally
// sure you won't make any mistakes with the API, or you're making a release
// build).
//
// However keep in mind that defining an empty handler means you're also
// signing up to debug your own missteps, should they happen, that would've
// likely been caught otherwise.
#ifndef BISCUIT_ASSERT

#include <cstdio>
#include <cstdlib>

#define BISCUIT_ASSERT(condition)                                        \
  do {                                                                   \
    if (!(condition)) {                                                  \
      std::printf("Assertion failed (%s)\nin %s, function %s line %i\n", \
                  #condition,                                            \
                  __FILE__, __func__, __LINE__);                         \
      std::abort();                                                      \
    }                                                                    \
  } while (false)

#endif

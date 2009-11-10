/**
 * @brief Hierarchia wyjatkow
 * @author Piotr Truszkowski
 */

#ifndef __EXCEPTION_HH__
#define __EXCEPTION_HH__

#include <exception>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <stdarg.h>

// @brief: glowny wyjatek, po nim dziedzicza inne
class Exception : public std::exception {
  public:
    Exception(void) throw() { }
    const char *what(void) const throw() { return "Exception"; }
};

// @brief: szablon dla wyjatkow prostych
#define DEF_EXC(name, up)                       \
  class name : public up {                      \
    public:                                     \
      name(void) throw() { }                    \
                                                \
      const char *what(void) const throw()      \
      {                                         \
        return #name;                           \
      }                                         \
  }

// @brief: szablon dla wyjatkow z opisem
#define DEF_EXC_WITH_DESCR(name, up)            \
  class name : public up {                      \
    public:                                     \
      name(void) throw()                        \
      {                                         \
        snprintf(exc, maxexclen, #name);        \
      }                                         \
                                                \
      name(const char *fmt, ...) throw()        \
      {                                         \
        va_list args;                           \
        va_start(args, fmt);                    \
        vsnprintf(exc, maxexclen, fmt, args);   \
        va_end(args);                           \
      }                                         \
                                                \
      const char *what(void) const throw()      \
      {                                         \
        return exc;                             \
      }                                         \
                                                \
    private:                                    \
      static const size_t maxexclen = 256;      \
      char exc[maxexclen];                      \
  }


// @brief: blad wewnetrzny, aplikacji lub blad w programie
DEF_EXC_WITH_DESCR(EInternal, Exception);
// @brief: blad zewnetrzy, bledne argumenty, itp...
DEF_EXC_WITH_DESCR(EExternal, Exception);

#endif


#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>

namespace Liquid {
    struct CoreException : public std::exception {
        std::string internal;

        CoreException() { }
        CoreException(const char* format, ...) {
            char buffer[512];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            internal = buffer;
        }
        const char * what () const throw () {
            return internal.c_str();
        }
    };
}

#endif

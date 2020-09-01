#ifndef PROTOCOLS_HPP
#define PROTOCOLS_HPP

#include <cstdint>

namespace Protocols {
    extern const char *protocols[];

    static inline const char *get_protocol(uint8_t number) { return protocols[number]; }
};

#endif // PROTOCOLS_HPP
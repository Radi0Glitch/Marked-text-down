#pragma once

#include <string>

namespace md {

inline bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

// Byte index of the character following position `pos`.
inline int next_char(const std::string& s, int pos) {
    ++pos;
    while (pos < static_cast<int>(s.size()) && is_continuation(s[pos])) {
        ++pos;
    }
    return pos;
}

// Byte index of the character preceding position `pos`.
inline int prev_char(const std::string& s, int pos) {
    if (pos <= 0) return 0;
    --pos;
    while (pos > 0 && is_continuation(s[pos])) {
        --pos;
    }
    return pos;
}

} // namespace md

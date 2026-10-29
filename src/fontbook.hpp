#pragma once

#include <SDL3_ttf/SDL_ttf.h>

#include <map>
#include <string>
#include <tuple>

namespace md {

enum class Family { Sans = 0, Mono = 1 };

// Loads font faces on demand and caches them by (family, bold, italic, strike, size).
// Owns every TTF_Font it returns.
class FontBook {
public:
    bool init(const std::string& dir);
    void clear();

    TTF_Font* get(Family family, bool bold, bool italic, float size, bool strike = false);

private:
    std::string dir_;
    std::map<std::tuple<int, bool, bool, bool, int>, TTF_Font*> cache_;
};

} // namespace md

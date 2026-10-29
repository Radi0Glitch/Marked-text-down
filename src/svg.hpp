#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace md {

struct SvgTexture {
    SDL_Texture* tex = nullptr;
    int w = 0;
    int h = 0;
};

// Render SVG data to SDL texture using librsvg+cairo. Returns true on success.
bool svg_to_texture(SDL_Renderer* ren, const std::string& svg_data, SvgTexture& out);

} // namespace md

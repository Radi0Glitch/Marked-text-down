#include "fontbook.hpp"

#include <algorithm>

namespace md {

bool FontBook::init(const std::string& dir) {
    dir_ = dir;
    // Fail early when the font files are missing.
    return get(Family::Sans, false, false, 17.0f)
        && get(Family::Sans, true, false, 17.0f)
        && get(Family::Sans, false, true, 17.0f)
        && get(Family::Sans, true, true, 17.0f)
        && get(Family::Mono, false, false, 15.0f)
        && get(Family::Mono, true, false, 15.0f);
}

void FontBook::clear() {
    for (auto& [key, font] : cache_) {
        TTF_CloseFont(font);
    }
    cache_.clear();
    for (auto& [key, faces] : fallbacks_) {
        for (TTF_Font* face : faces) TTF_CloseFont(face);
    }
    fallbacks_.clear();
}

void FontBook::set_zoom(float zoom) {
    zoom_ = std::clamp(zoom, 0.5f, 2.0f);
}

TTF_Font* FontBook::get(Family family, bool bold, bool italic, float size, bool strike) {
    const float zoomed = size * zoom_;
    const auto key = std::make_tuple(static_cast<int>(family), bold, italic, strike,
                                     static_cast<int>(zoomed * 4.0f));
    if (const auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }

    std::string file;
    switch (family) {
        case Family::Sans:
            file = bold && italic ? "FiraSans-BoldItalic.ttf"
                 : bold           ? "FiraSans-Bold.ttf"
                 : italic         ? "FiraSans-Italic.ttf"
                                  : "FiraSans-Regular.ttf";
            break;
        case Family::Mono:
            // Fira Mono has no italic faces; fall back to upright.
            file = bold ? "FiraMono-Bold.ttf" : "FiraMono-Regular.ttf";
            break;
        case Family::Serif:
            file = bold && italic ? "PT_Serif-Web-BoldItalic.ttf"
                 : bold           ? "PT_Serif-Web-Bold.ttf"
                 : italic         ? "PT_Serif-Web-Italic.ttf"
                                  : "PT_Serif-Web-Regular.ttf";
            break;
    }

    TTF_Font* font = TTF_OpenFont((dir_ + "/" + file).c_str(), zoomed);
    if (font) {
        TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
        int style = 0;
        if (bold) style |= TTF_STYLE_BOLD;
        if (italic) style |= TTF_STYLE_ITALIC;
        if (strike) style |= TTF_STYLE_STRIKETHROUGH;
        if (style) TTF_SetFontStyle(font, static_cast<TTF_FontStyleFlags>(style));

        // Glyphs the face lacks (arrows, modifier letters) come from these faces.
        // They are opened at the same size and style, as SDL_ttf requires.
        auto& faces = fallbacks_[std::make_tuple(bold, italic, strike, static_cast<int>(zoomed * 4.0f))];
        if (faces.empty()) {
            for (const char* name : {"NotoSansMath-Regular.ttf", "NotoSans-Regular.ttf"}) {
                TTF_Font* face = TTF_OpenFont((dir_ + "/" + name).c_str(), zoomed);
                if (!face) continue;
                if (style) TTF_SetFontStyle(face, static_cast<TTF_FontStyleFlags>(style));
                faces.push_back(face);
            }
        }
        for (TTF_Font* face : faces) TTF_AddFallbackFont(font, face);
        cache_.emplace(key, font);
    }
    return font;
}

} // namespace md

#include "fontbook.hpp"

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
}

TTF_Font* FontBook::get(Family family, bool bold, bool italic, float size, bool strike) {
    const auto key = std::make_tuple(static_cast<int>(family), bold, italic, strike,
                                     static_cast<int>(size * 4.0f));
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
    }

    TTF_Font* font = TTF_OpenFont((dir_ + "/" + file).c_str(), size);
    if (font) {
        TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
        int style = 0;
        if (bold) style |= TTF_STYLE_BOLD;
        if (italic) style |= TTF_STYLE_ITALIC;
        if (strike) style |= TTF_STYLE_STRIKETHROUGH;
        if (style) TTF_SetFontStyle(font, static_cast<TTF_FontStyleFlags>(style));
        cache_.emplace(key, font);
    }
    return font;
}

} // namespace md

#include "ui.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {
SDL_FColor to_fcolor(SDL_Color c) {
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

void set_draw_color(Context& ctx, SDL_Color c) {
    SDL_SetRenderDrawColor(ctx.renderer, c.r, c.g, c.b, c.a);
}
} // namespace

bool contains(Rect r, float px, float py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

Palette dark_palette() {
    return Palette{
        .window_bg = {30, 27, 24, 255},
        .panel_bg = {38, 34, 30, 255},
        .panel_border = {64, 57, 49, 255},
        .text = {236, 228, 214, 255},
        .text_dim = {150, 139, 124, 255},
        .button_bg = {30, 27, 24, 255},
        .button_hover = {56, 50, 43, 255},
        .button_pressed = {70, 62, 54, 255},
        .button_active = {78, 46, 36, 255},
        .accent = {222, 112, 84, 255},
        .link = {120, 190, 255, 255},
        .popup_bg = {46, 41, 36, 255},
        .popup_border = {72, 64, 55, 255},
    };
}

Palette light_palette() {
    return Palette{
        .window_bg = {246, 243, 236, 255},
        .panel_bg = {240, 236, 226, 255},
        .panel_border = {222, 214, 198, 255},
        .text = {38, 34, 30, 255},
        .text_dim = {128, 118, 104, 255},
        .button_bg = {246, 243, 236, 255},
        .button_hover = {236, 230, 216, 255},
        .button_pressed = {226, 218, 202, 255},
        .button_active = {236, 214, 204, 255},
        .accent = {192, 67, 38, 255},
        .link = {18, 92, 214, 255},
        .popup_bg = {252, 250, 245, 255},
        .popup_border = {222, 214, 198, 255},
    };
}

bool is_light_palette(const Palette& p) {
    return p.window_bg.r > 128;
}

void begin_frame(Context& ctx) {
    ctx.popup = Context::Popup{};
}

// A list opens under its anchor, or above it when there is no room below.
static Rect popup_list_rect(const Context& ctx, Rect anchor, float list_w, float row_h, size_t count) {
    const float h = row_h * static_cast<float>(count);
    const float below = anchor.y + anchor.h + 4.0f;
    const float above = anchor.y - 4.0f - h;
    const bool fits_below = ctx.screen_h <= 0.0f || below + h <= ctx.screen_h;
    const float y = (!fits_below && above >= 0.0f) ? above : below;
    return Rect{anchor.x, y, list_w, h};
}

void end_frame(Context& ctx) {
    const Context::Popup& p = ctx.popup;
    if (p.id.empty() || ctx.open_popup != p.id || p.items.empty()) {
        return;
    }

    const Rect list = popup_list_rect(ctx, p.anchor, p.width, p.row_h, p.items.size());
    fill_rounded_rect(ctx, list, 6.0f, ctx.palette.popup_bg);
    stroke_rect(ctx, list, ctx.palette.popup_border);

    TTF_Font* font = ctx.fonts->regular;
    for (size_t i = 0; i < p.items.size(); ++i) {
        const Rect row{list.x, list.y + p.row_h * static_cast<float>(i),
                       list.w, p.row_h};
        if (contains(row, ctx.input.mouse_x, ctx.input.mouse_y)) {
            fill_rounded_rect(ctx, row, 6.0f, ctx.palette.button_hover);
        }
        const bool selected = static_cast<int>(i) == p.current;
        const SDL_Color color = selected ? ctx.palette.accent : ctx.palette.text;
        draw_text(ctx, font, p.items[i], row.x + 12.0f,
                  row.y + (row.h - text_height(font)) / 2.0f, color);
    }
}

void fill_rect(Context& ctx, Rect r, SDL_Color color) {
    set_draw_color(ctx, color);
    const SDL_FRect fr{r.x, r.y, r.w, r.h};
    SDL_RenderFillRect(ctx.renderer, &fr);
}

void stroke_rect(Context& ctx, Rect r, SDL_Color color) {
    set_draw_color(ctx, color);
    const SDL_FRect fr{r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f};
    SDL_RenderRect(ctx.renderer, &fr);
}

void fill_rounded_rect(Context& ctx, Rect r, float radius, SDL_Color color) {
    radius = std::max(0.0f, std::min({radius, r.w / 2.0f, r.h / 2.0f}));
    if (radius <= 0.0f) {
        fill_rect(ctx, r, color);
        return;
    }

    // The edge is feathered over one pixel: an outer ring of the same shape, one
    // pixel larger and fully transparent, anti-aliases the outline.
    constexpr int kSegments = 12;
    constexpr float kFeather = 1.0f;
    const SDL_FColor inside = to_fcolor(color);
    SDL_FColor outside = inside;
    outside.a = 0.0f;

    struct Corner { float cx, cy, a0, a1; };
    const Corner corners[4] = {
        {r.x + radius, r.y + radius, 180.0f, 270.0f},             // top-left
        {r.x + r.w - radius, r.y + radius, 270.0f, 360.0f},       // top-right
        {r.x + r.w - radius, r.y + r.h - radius, 0.0f, 90.0f},    // bottom-right
        {r.x + radius, r.y + r.h - radius, 90.0f, 180.0f},        // bottom-left
    };

    constexpr int kPerimeter = 4 * (kSegments + 1);
    SDL_Vertex verts[1 + 2 * kPerimeter];
    int count = 0;
    // Vertex 0 is the fan centre, 1..N the inner outline, N+1..2N the feathered outline.
    verts[count++] = SDL_Vertex{{r.x + r.w / 2.0f, r.y + r.h / 2.0f}, inside, {0, 0}};
    for (const Corner& c : corners) {
        for (int i = 0; i <= kSegments; ++i) {
            const float deg = c.a0 + (c.a1 - c.a0) * static_cast<float>(i) / kSegments;
            const float rad = deg * 3.14159265358979323846f / 180.0f;
            const float cs = std::cos(rad);
            const float sn = std::sin(rad);
            verts[count++] = SDL_Vertex{{c.cx + radius * cs, c.cy + radius * sn}, inside, {0, 0}};
        }
    }
    for (const Corner& c : corners) {
        for (int i = 0; i <= kSegments; ++i) {
            const float deg = c.a0 + (c.a1 - c.a0) * static_cast<float>(i) / kSegments;
            const float rad = deg * 3.14159265358979323846f / 180.0f;
            const float cs = std::cos(rad);
            const float sn = std::sin(rad);
            verts[count++] = SDL_Vertex{
                {c.cx + (radius + kFeather) * cs, c.cy + (radius + kFeather) * sn}, outside, {0, 0}};
        }
    }

    int indices[kPerimeter * 9];
    int idx = 0;
    for (int i = 0; i < kPerimeter; ++i) {
        const int a = 1 + i;
        const int b = 1 + (i + 1) % kPerimeter;
        const int c = 1 + kPerimeter + i;
        const int d = 1 + kPerimeter + (i + 1) % kPerimeter;
        indices[idx++] = 0;
        indices[idx++] = a;
        indices[idx++] = b;
        indices[idx++] = a;
        indices[idx++] = b;
        indices[idx++] = d;
        indices[idx++] = a;
        indices[idx++] = d;
        indices[idx++] = c;
    }

    SDL_RenderGeometry(ctx.renderer, nullptr, verts, count, indices, idx);
}

void draw_text(Context& ctx, TTF_Font* font, const std::string& text,
               float x, float y, SDL_Color color) {
    if (!font || text.empty()) {
        return;
    }
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surface) {
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(ctx.renderer, surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        const SDL_FRect dst{std::round(x), std::round(y), static_cast<float>(surface->w),
                            static_cast<float>(surface->h)};
        SDL_RenderTexture(ctx.renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

float text_width(TTF_Font* font, const std::string& text) {
    if (!font || text.empty()) {
        return 0.0f;
    }
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str(), text.size(), &w, &h);
    return static_cast<float>(w) + 1.0f;
}

float text_height(TTF_Font* font) {
    return font ? static_cast<float>(TTF_GetFontHeight(font)) : 0.0f;
}

bool pointer_over_popup(const Context& ctx) {
    const Context::Popup& p = ctx.popup;
    if (p.id.empty() || ctx.open_popup != p.id || p.items.empty()) return false;
    const Rect list{p.anchor.x, p.anchor.y + p.anchor.h + 4.0f, p.width,
                    p.row_h * static_cast<float>(p.items.size())};
    return contains(list, ctx.input.mouse_x, ctx.input.mouse_y);
}

namespace {
// Shared press/hover/release logic for button-like widgets.
bool button_behavior(Context& ctx, const char* id, Rect r, bool* out_hover, bool* out_pressed) {
    // An open list covers whatever is underneath it.
    const bool hover = contains(r, ctx.input.mouse_x, ctx.input.mouse_y)
                       && !pointer_over_popup(ctx);
    if (hover && ctx.input.pressed) {
        ctx.active_id = id;
    }

    bool clicked = false;
    if (ctx.active_id == id) {
        if (ctx.input.released) {
            clicked = hover;
            ctx.active_id.clear();
        } else if (!ctx.input.down) {
            ctx.active_id.clear();
        }
    }

    if (out_hover) *out_hover = hover;
    if (out_pressed) *out_pressed = ctx.active_id == id && ctx.input.down;
    return clicked;
}

void draw_button(Context& ctx, Rect r, const char* label,
                 bool hover, bool pressed, bool toggled, bool enabled = true,
                 TTF_Font* custom_font = nullptr) {
    SDL_Color bg = ctx.palette.button_bg;
    if (!enabled) {
        // Locked: no hover or press feedback, dimmed text.
    } else if (toggled) {
        bg = ctx.palette.button_active;
    } else if (pressed) {
        bg = ctx.palette.button_pressed;
    } else if (hover) {
        bg = ctx.palette.button_hover;
    }
    fill_rounded_rect(ctx, r, 6.0f, bg);

    TTF_Font* font = custom_font ? custom_font : ctx.fonts->regular;
    SDL_Color fg = toggled ? ctx.palette.accent : ctx.palette.text;
    if (!enabled) fg = ctx.palette.text_dim;
    draw_text(ctx, font, label,
              r.x + (r.w - text_width(font, label)) / 2.0f,
              r.y + (r.h - text_height(font)) / 2.0f, fg);
}
} // namespace

bool button(Context& ctx, const char* id, Rect r, const char* label, bool enabled) {
    bool hover = false, pressed = false;
    const bool clicked = enabled && button_behavior(ctx, id, r, &hover, &pressed);
    draw_button(ctx, r, label, hover, pressed, false, enabled);
    return clicked;
}

bool toggle_button(Context& ctx, const char* id, Rect r, const char* label, bool active,
                   bool enabled, TTF_Font* font) {
    bool hover = false, pressed = false;
    const bool clicked = enabled && button_behavior(ctx, id, r, &hover, &pressed);
    draw_button(ctx, r, label, hover, pressed, active, enabled, font);
    return clicked;
}

bool tab_button(Context& ctx, const char* id, Rect r, const char* label, bool active,
                bool enabled) {
    bool hover = false, pressed = false;
    const bool clicked = enabled && button_behavior(ctx, id, r, &hover, &pressed);
    if (enabled && hover && !active) {
        fill_rounded_rect(ctx, r, 6.0f, ctx.palette.button_hover);
    }

    TTF_Font* font = ctx.fonts->regular;
    const SDL_Color fg = enabled ? ctx.palette.text : ctx.palette.text_dim;
    draw_text(ctx, font, label,
              r.x + (r.w - text_width(font, label)) / 2.0f,
              r.y + (r.h - text_height(font)) / 2.0f, fg);
    if (active) {
        fill_rect(ctx, {r.x + 8.0f, r.y + r.h - 2.0f, r.w - 16.0f, 2.0f}, ctx.palette.accent);
    }
    return clicked;
}

void separator(Context& ctx, Rect r) {
    fill_rect(ctx, r, ctx.palette.panel_border);
}

namespace {

int popup_control(Context& ctx, const char* id, Rect r, const std::string& label,
                  const std::vector<std::string>& items, int current, float list_w) {
    int result = -1;

    bool hover = false, pressed = false;
    const bool clicked = button_behavior(ctx, id, r, &hover, &pressed);
    if (clicked) {
        ctx.open_popup = ctx.open_popup == id ? std::string() : std::string(id);
    }

    const bool open = ctx.open_popup == id;
    const float row_h = r.h;
    const Rect list = popup_list_rect(ctx, r, list_w, row_h, items.size());

    if (open) {
        if (ctx.input.pressed && !contains(r, ctx.input.mouse_x, ctx.input.mouse_y)
            && !contains(list, ctx.input.mouse_x, ctx.input.mouse_y)) {
            ctx.open_popup.clear();
        } else if (ctx.input.released) {
            for (size_t i = 0; i < items.size(); ++i) {
                const Rect row{list.x, list.y + row_h * static_cast<float>(i), list.w, row_h};
                if (contains(row, ctx.input.mouse_x, ctx.input.mouse_y)) {
                    result = static_cast<int>(i);
                    ctx.open_popup.clear();
                    break;
                }
            }
        }
    }

    draw_button(ctx, r, "", hover, pressed, open);
    TTF_Font* font = ctx.fonts->regular;
    draw_text(ctx, font, label, r.x + 12.0f,
              r.y + (r.h - text_height(font)) / 2.0f, ctx.palette.text);

    if (open && ctx.open_popup == id) {
        ctx.popup.id = id;
        ctx.popup.anchor = r;
        ctx.popup.items = items;
        ctx.popup.current = current;
        ctx.popup.row_h = row_h;
        ctx.popup.width = list_w;
    }
    return result;
}

} // namespace

int dropdown(Context& ctx, const char* id, Rect r,
             const std::vector<std::string>& items, int current) {
    const std::string label = items.empty() ? std::string() : items[current];
    return popup_control(ctx, id, r, label, items, current, r.w);
}

int menu(Context& ctx, const char* id, Rect r, const char* label,
         const std::vector<std::string>& items, float list_w) {
    return popup_control(ctx, id, r, label, items, -1, list_w);
}

} // namespace ui

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <vector>

namespace ui {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

bool contains(Rect r, float px, float py);

struct Palette {
    SDL_Color window_bg;
    SDL_Color panel_bg;
    SDL_Color panel_border;
    SDL_Color text;
    SDL_Color text_dim;
    SDL_Color button_bg;
    SDL_Color button_hover;
    SDL_Color button_pressed;
    SDL_Color button_active;
    SDL_Color accent;
    SDL_Color popup_bg;
    SDL_Color popup_border;
};

Palette dark_palette();
Palette light_palette();
bool is_light_palette(const Palette& p);

// Non-owning pointers into md::FontBook, at the fixed UI size.
struct Fonts {
    TTF_Font* regular = nullptr;
    TTF_Font* bold = nullptr;
    TTF_Font* italic = nullptr;
    TTF_Font* bold_italic = nullptr;
    TTF_Font* strike = nullptr;
    TTF_Font* mono = nullptr;
};

// Snapshot of the mouse for the current frame.
struct Input {
    float mouse_x = 0, mouse_y = 0;
    bool down = false;     // held right now
    bool pressed = false;  // went down during this frame
    bool released = false; // went up during this frame
};

struct Context {
    SDL_Renderer* renderer = nullptr;
    Fonts* fonts = nullptr;
    Palette palette;
    Input input;

    std::string active_id;  // widget the mouse was pressed on
    std::string open_popup; // dropdown id with an open popup

    // Deferred popup drawing, refilled every frame by dropdown().
    struct Popup {
        std::string id;
        Rect anchor;
        std::vector<std::string> items;
        int current = 0;
        float row_h = 0;
        float width = 0;
    } popup;
};

// True when the open list of a menu or dropdown is under the mouse pointer.
bool pointer_over_popup(const Context& ctx);

void begin_frame(Context& ctx);
void end_frame(Context& ctx); // draws the deferred popup on top of everything

// Primitives.
void fill_rect(Context& ctx, Rect r, SDL_Color color);
void fill_rounded_rect(Context& ctx, Rect r, float radius, SDL_Color color);
void stroke_rect(Context& ctx, Rect r, SDL_Color color);
void draw_text(Context& ctx, TTF_Font* font, const std::string& text,
               float x, float y, SDL_Color color);
float text_width(TTF_Font* font, const std::string& text);
float text_height(TTF_Font* font);

// Widgets. All return whether the user activated them this frame.
// A disabled widget is drawn dimmed and never reports a click.
bool button(Context& ctx, const char* id, Rect r, const char* label, bool enabled = true);
// `active` is drawn as pressed state; the caller owns it and flips it on click.
bool toggle_button(Context& ctx, const char* id, Rect r, const char* label, bool active,
                   bool enabled = true, TTF_Font* font = nullptr);
// A tab: `active` gets an accent underline. Returns true when clicked (if enabled).
bool tab_button(Context& ctx, const char* id, Rect r, const char* label, bool active,
                bool enabled);
void separator(Context& ctx, Rect r);
// Returns the newly selected item index, or -1 when the selection did not change.
int dropdown(Context& ctx, const char* id, Rect r,
             const std::vector<std::string>& items, int current);
// A button with a fixed label that opens a list of actions. Returns the chosen
// index, or -1. The list is `list_w` wide and starts under the button.
int menu(Context& ctx, const char* id, Rect r, const char* label,
         const std::vector<std::string>& items, float list_w);

} // namespace ui

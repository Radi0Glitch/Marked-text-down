#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "window_state.hpp"

namespace winstate {

namespace {

constexpr int kMinW = 560;
constexpr int kMinH = 400;

#ifdef _WIN32
HWND hwnd_of(SDL_Window* window) {
    return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                    SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}
#endif

} // namespace

bool read(SDL_Window* window, Geometry& out) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_MINIMIZED) return false;
#ifdef _WIN32
    HWND hwnd = hwnd_of(window);
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!hwnd || !GetWindowPlacement(hwnd, &wp)) return false;
    out.x = wp.rcNormalPosition.left;
    out.y = wp.rcNormalPosition.top;
    out.w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    out.h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    out.has_rect = true;
    out.maximized = wp.showCmd == SW_SHOWMAXIMIZED;
#else
    out.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    // A maximized window reports the maximized size, not the restored one: keep the old rectangle.
    out.has_rect = !out.maximized;
    if (out.has_rect) {
        SDL_GetWindowPosition(window, &out.x, &out.y);
        SDL_GetWindowSize(window, &out.w, &out.h);
    }
#endif
    return true;
}

void apply(SDL_Window* window, const Geometry& g) {
#ifdef _WIN32
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    wp.showCmd = g.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    wp.rcNormalPosition = RECT{g.x, g.y, g.x + g.w, g.y + g.h};
    if (HWND hwnd = hwnd_of(window)) SetWindowPlacement(hwnd, &wp);
#else
    SDL_SetWindowSize(window, g.w, g.h);
    SDL_SetWindowPosition(window, g.x, g.y);
    if (g.maximized) SDL_MaximizeWindow(window);
#endif
}

bool usable(const Geometry& g) {
    if (g.w < kMinW || g.h < kMinH) return false;
#ifdef _WIN32
    const RECT rc{g.x, g.y, g.x + g.w, g.y + g.h};
    return MonitorFromRect(&rc, MONITOR_DEFAULTTONULL) != nullptr;
#else
    const float cx = static_cast<float>(g.x) + static_cast<float>(g.w) / 2.0f;
    const float cy = static_cast<float>(g.y) + static_cast<float>(g.h) / 2.0f;
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    bool found = false;
    for (int i = 0; ids && i < count && !found; ++i) {
        SDL_Rect b{};
        if (SDL_GetDisplayBounds(ids[i], &b) && cx >= b.x && cx < b.x + b.w
            && cy >= b.y && cy < b.y + b.h) {
            found = true;
        }
    }
    SDL_free(ids);
    return found;
#endif
}

} // namespace winstate

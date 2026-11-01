#pragma once

#include <SDL3/SDL.h>

namespace winstate {

// The restored (normal) rectangle of a window, and whether the window is maximized.
// Coordinates are exactly what the platform reports; they are not scaled.
struct Geometry {
    int x = 0, y = 0, w = 0, h = 0;
    bool has_rect = false; // false when the platform does not expose the restored rectangle
    bool maximized = false;
};

// Reads the state of an open window. False for a minimized window: nothing to record.
bool read(SDL_Window* window, Geometry& out);

// Places a window from saved geometry.
void apply(SDL_Window* window, const Geometry& g);

// True when the rectangle is big enough and its centre lies on a connected display.
bool usable(const Geometry& g);

} // namespace winstate

#include "svg.hpp"

#include <cairo/cairo.h>
#include <librsvg/rsvg.h>

namespace md {

bool svg_to_texture(SDL_Renderer* ren, const std::string& svg_data, SvgTexture& out) {
    out = SvgTexture{};
    if (!ren || svg_data.empty()) return false;

#if LIBRSVG_MAJOR_VERSION >= 2 && LIBRSVG_MINOR_VERSION >= 52
    RsvgHandle* handle = rsvg_handle_new_from_data(
        reinterpret_cast<const guint8*>(svg_data.data()),
        svg_data.size(),
        nullptr);
#else
    GError* err = nullptr;
    RsvgHandle* handle = rsvg_handle_new_from_data(
        reinterpret_cast<const guint8*>(svg_data.data()),
        svg_data.size(),
        &err);
    if (err) { g_error_free(err); err = nullptr; }
#endif
    if (!handle) return false;

    RsvgRectangle viewport{0, 0, 1, 1};
    double w = 0, h = 0;
#if LIBRSVG_MAJOR_VERSION >= 2 && LIBRSVG_MINOR_VERSION >= 46
    if (!rsvg_handle_get_intrinsic_size_in_pixels(handle, &w, &h)) {
        RsvgDimensionData dim{};
        rsvg_handle_get_dimensions(handle, &dim);
        w = dim.width > 0 ? dim.width : 300;
        h = dim.height > 0 ? dim.height : 100;
    }
#else
    RsvgDimensionData dim{};
    rsvg_handle_get_dimensions(handle, &dim);
    w = dim.width > 0 ? dim.width : 300;
    h = dim.height > 0 ? dim.height : 100;
#endif
    if (w <= 0) w = 300;
    if (h <= 0) h = 100;

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)w, (int)h);
    cairo_t* cr = cairo_create(surf);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);

    viewport.x = 0; viewport.y = 0; viewport.width = w; viewport.height = h;
#if LIBRSVG_MAJOR_VERSION >= 2 && LIBRSVG_MINOR_VERSION >= 52
    rsvg_handle_render_document(handle, cr, &viewport, nullptr);
#else
    rsvg_handle_render_cairo(handle, cr);
#endif

    cairo_surface_flush(surf);
    unsigned char* data = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);

    SDL_Surface* sdl_surf = SDL_CreateSurface((int)w, (int)h, SDL_PIXELFORMAT_ARGB8888);
    if (sdl_surf) {
        // copy
        for (int y = 0; y < (int)h; ++y) {
            memcpy((unsigned char*)sdl_surf->pixels + y * sdl_surf->pitch,
                   data + y * stride,
                   (size_t)w * 4);
        }
        out.tex = SDL_CreateTextureFromSurface(ren, sdl_surf);
        SDL_DestroySurface(sdl_surf);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    g_object_unref(handle);

    if (!out.tex) return false;
    out.w = (int)w;
    out.h = (int)h;
    return true;
}

} // namespace md

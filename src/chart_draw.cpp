#include "chart_draw.hpp"

#include <algorithm>
#include <cmath>

namespace md {
namespace chartdraw {
namespace {

constexpr float kPad = 12.0f;
constexpr float kTitleH = 18.0f;
constexpr float kAxisGap = 26.0f;
constexpr float kBarGap = 10.0f;
constexpr float kFontSize = 13.0f;
constexpr float kPi = 3.14159265f;

const SDL_Color kBar1{96, 140, 214, 255};
const SDL_Color kBar2{110, 170, 118, 255};
const SDL_Color kBar3{214, 150, 82, 255};
const SDL_Color kBar4{186, 110, 202, 255};
const SDL_Color kBar5{90, 180, 196, 255};
const SDL_Color kLine{126, 166, 236, 255};
const SDL_Color kGrid{62, 70, 90, 255};
const SDL_Color kPieCols[] = {
    {96, 140, 214, 255},
    {110, 170, 118, 255},
    {214, 150, 82, 255},
    {186, 110, 202, 255},
    {90, 180, 196, 255},
    {214, 120, 130, 255},
    {170, 150, 96, 255},
    {130, 136, 190, 255},
};

TTF_Font* face(FontBook& fonts, float size) {
    return fonts.get(Family::Sans, false, false, size);
}

float font_h(TTF_Font* f) { return static_cast<float>(TTF_GetFontHeight(f)); }

bool is_selected(const Selection* sel, int index, bool value) {
    return sel != nullptr && sel->index == index && sel->value == value;
}

void set_color(ui::Context& ctx, SDL_Color c) {
    SDL_SetRenderDrawColor(ctx.renderer, c.r, c.g, c.b, c.a);
}

void fill_poly(ui::Context& ctx, const std::vector<SDL_FPoint>& p, SDL_Color c) {
    if (p.size() < 3) return;
    const SDL_FColor fc{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    std::vector<SDL_Vertex> verts((p.size() - 2) * 3);
    for (size_t i = 1; i + 1 < p.size(); ++i) {
        SDL_Vertex& a = verts[(i - 1) * 3];
        SDL_Vertex& b = verts[(i - 1) * 3 + 1];
        SDL_Vertex& d = verts[(i - 1) * 3 + 2];
        a = {{p[0].x, p[0].y}, fc, {0, 0}};
        b = {{p[i].x, p[i].y}, fc, {0, 0}};
        d = {{p[i + 1].x, p[i + 1].y}, fc, {0, 0}};
    }
    SDL_RenderGeometry(ctx.renderer, nullptr, verts.data(),
                       static_cast<int>(verts.size()), nullptr, 0);
}

void dotted_box(ui::Context& ctx, ui::Rect r, SDL_Color c) {
    const float step = 5.0f;
    for (float px = r.x; px < r.x + r.w - 1.0f; px += step) {
        ui::fill_rect(ctx, {px, r.y, 2.0f, 1.0f}, c);
        ui::fill_rect(ctx, {px, r.y + r.h - 1.0f, 2.0f, 1.0f}, c);
    }
    for (float py = r.y; py < r.y + r.h - 1.0f; py += step) {
        ui::fill_rect(ctx, {r.x, py, 1.0f, 2.0f}, c);
        ui::fill_rect(ctx, {r.x + r.w - 1.0f, py, 1.0f, 2.0f}, c);
    }
}

SDL_Color bar_color(size_t i) {
    static const SDL_Color cols[] = {kBar1, kBar2, kBar3, kBar4, kBar5};
    return cols[i % 5];
}

Size placeholder(ui::Context* ctx, FontBook& fonts, float x, float y,
                 float max_w, const char* text) {
    TTF_Font* f = face(fonts, 14.0f);
    std::string t = text;
    float tw = ui::text_width(f, t);
    if (max_w > 0.0f && tw + 40.0f > max_w) {
        t = "График";
        tw = ui::text_width(f, t);
    }
    float w = tw + 40.0f;
    const float h = font_h(f) + 24.0f;
    if (max_w > 0.0f && w > max_w) w = max_w;
    if (ctx) {
        const bool light = ui::is_light_palette(ctx->palette);
        const ui::Rect r{x, y, w, h};
        ui::fill_rounded_rect(*ctx, r, 6.0f,
                              light ? SDL_Color{232, 237, 246, 255} : SDL_Color{40, 46, 60, 255});
        ui::stroke_rect(*ctx, r,
                        light ? SDL_Color{185, 195, 214, 255} : SDL_Color{76, 86, 106, 255});
        ui::draw_text(*ctx, f, t, x + (w - tw) / 2.0f,
                      y + (h - font_h(f)) / 2.0f, ctx->palette.text_dim);
    }
    return {w, h};
}

// Find a nice tick count for the value range.
int nice_ticks(double vmax) {
    if (vmax <= 0.0) return 4;
    if (vmax <= 1.0) return 4;
    if (vmax <= 5.0) return 5;
    if (vmax <= 10.0) return 5;
    return 4;
}

// Truncate label with ellipsis so it fits inside `max_w` without splitting UTF-8.
std::string fit_chart_label(TTF_Font* font, std::string text, float max_w) {
    if (max_w <= 8.0f || ui::text_width(font, text) <= max_w) return text;
    while (!text.empty() && ui::text_width(font, text + "…") > max_w) {
        size_t start = text.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
            --start;
        }
        text.erase(start);
    }
    return text.empty() ? std::string("…") : text + "…";
}

Size render_xy(ui::Context* ctx, FontBook& fonts, const ChartModel& m,
               float x, float y, float max_w, const Selection* selected,
               std::vector<Slot>* slots, bool line) {
    const size_t n = m.labels.size();
    if (n == 0 || m.values.size() == 0) {
        return placeholder(ctx, fonts, x, y, max_w, "График пуст");
    }

    TTF_Font* f = face(fonts, kFontSize);
    TTF_Font* ft = face(fonts, 15.0f);
    const float lh = font_h(f);

    // The scale covers every value and always contains zero, so bars grow from the zero line.
    double vmin = 0.0;
    double vmax = 0.0;
    for (const std::string& s : m.values) {
        const double v = chart_value(s);
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    // Grid steps are round numbers and the scale ends on grid lines.
    const double raw_span = vmax > vmin ? vmax - vmin : 1.0;
    const double raw_step = raw_span / nice_ticks(raw_span);
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw_step)));
    double step = magnitude * 10.0;
    for (const double k : {1.0, 2.0, 2.5, 5.0, 10.0}) {
        if (k * magnitude >= raw_step) {
            step = k * magnitude;
            break;
        }
    }
    vmin = std::floor(vmin / step) * step;
    vmax = std::ceil(vmax / step) * step;
    if (vmax <= vmin) vmax = vmin + step;
    const int ticks = static_cast<int>(std::round((vmax - vmin) / step));
    const double span = vmax - vmin;

    // Size the chart to fit the full block width.
    float total_w = max_w;
    if (total_w < 220.0f) total_w = 220.0f;
    if (total_w > 1200.0f) total_w = 1200.0f;
    float plot_h = std::clamp(total_w * 0.30f, 150.0f, 210.0f);
    const float left = kPad + kAxisGap + 8.0f;
    const float bottom_pad = kPad + lh + 16.0f;
    const float top_pad = kPad + (m.title.empty() ? 0.0f : kTitleH) + lh + 8.0f;
    const float plot_w = std::max(120.0f, total_w - left - kPad - 10.0f);
    const float plot_x = x + left;
    const float plot_y = y + top_pad;
    const float plot_bottom = plot_y + plot_h;
    auto y_of = [&](double v) {
        return plot_bottom - plot_h * static_cast<float>((v - vmin) / span);
    };
    const float y0 = y_of(0.0);

    if (!ctx) return {total_w, top_pad + plot_h + bottom_pad};

    if (!m.title.empty()) {
        const float tw = ui::text_width(ft, m.title);
        ui::draw_text(*ctx, ft, m.title, x + (total_w - tw) / 2.0f, y + kPad,
                      ctx->palette.text);
    }

    // Axes and horizontal grid.
    const bool light = ui::is_light_palette(ctx->palette);
    const SDL_Color grid_col = light ? SDL_Color{208, 216, 228, 255} : kGrid;
    set_color(*ctx, grid_col);
    for (int i = 0; i <= ticks; ++i) {
        const double val = vmin + step * static_cast<double>(i);
        const float ty = y_of(val);
        SDL_RenderLine(ctx->renderer, plot_x, ty, plot_x + plot_w, ty);
        std::string lbl = std::to_string(static_cast<long long>(val));
        if (val != static_cast<double>(static_cast<long long>(val))) {
            lbl = std::to_string(val);
            if (lbl.find('.') != std::string::npos) {
                while (!lbl.empty() && lbl.back() == '0') lbl.pop_back();
                if (!lbl.empty() && lbl.back() == '.') lbl.pop_back();
            }
        }
        const float lw = ui::text_width(f, lbl);
        ui::draw_text(*ctx, f, lbl, plot_x - lw - 6.0f,
                      ty - lh / 2.0f, ctx->palette.text_dim);
    }
    set_color(*ctx, ctx->palette.panel_border);
    SDL_RenderLine(ctx->renderer, plot_x, plot_y, plot_x, plot_bottom);
    SDL_RenderLine(ctx->renderer, plot_x, plot_bottom, plot_x + plot_w, plot_bottom);
    if (vmin < 0.0) {
        set_color(*ctx, ctx->palette.text_dim);
        SDL_RenderLine(ctx->renderer, plot_x, y0, plot_x + plot_w, y0);
    }

    const float group_w = plot_w / static_cast<float>(n);
    std::vector<SDL_FPoint> pts;
    const bool edit_mode = (selected != nullptr || slots != nullptr);

    for (size_t i = 0; i < n; ++i) {
        const double v = chart_value(m.values[i]);
        const float ty = y_of(v);
        const float h = std::fabs(ty - y0);
        const float top = std::min(ty, y0);
        const float cx = plot_x + group_w * (static_cast<float>(i) + 0.5f);
        const bool sel_cat = is_selected(selected, static_cast<int>(i), false);
        const bool sel_val = is_selected(selected, static_cast<int>(i), true);

        if (line) {
            pts.push_back({cx, ty});
        } else {
            const float bw = std::max(6.0f, std::min(52.0f, group_w - kBarGap));
            const ui::Rect br{cx - bw / 2.0f, top, bw, h};
            const SDL_Color bc = bar_color(i);
            if (h > 0.0f) {
                ui::fill_rect(*ctx, br, bc);
            }
            if (sel_cat || sel_val) {
                ui::stroke_rect(*ctx, {br.x - 1.0f, std::min(br.y, plot_bottom - 4.0f),
                                       br.w + 2.0f, std::max(br.h, 4.0f)},
                                ctx->palette.accent);
            }
        }

        // Category label under the group (clickable slot).
        const float max_cell_w = std::max(24.0f, group_w - 4.0f);
        const std::string raw_cl = m.labels[i];
        const std::string cl = fit_chart_label(f, raw_cl, max_cell_w - 8.0f);
        const float slw = cl.empty() ? std::min(36.0f, max_cell_w - 4.0f)
                                     : std::max(24.0f, ui::text_width(f, cl) + 10.0f);
        const ui::Rect cat_rect{cx - slw / 2.0f, plot_bottom + 5.0f, slw, lh + 6.0f};
        if (slots) {
            slots->push_back(Slot{static_cast<int>(i), false, cat_rect});
        }
        if (sel_cat) {
            ui::fill_rounded_rect(*ctx, cat_rect, 4.0f, ctx->palette.button_active);
            ui::stroke_rect(*ctx, cat_rect, ctx->palette.accent);
        } else if (edit_mode) {
            dotted_box(*ctx, cat_rect, ctx->palette.panel_border);
        }
        if (cl.empty()) {
            if (!edit_mode) {
                dotted_box(*ctx, {cx - 14.0f, plot_bottom + 8.0f, 28.0f, 12.0f},
                           ctx->palette.text_dim);
            }
            if (sel_cat) {
                ui::fill_rect(*ctx, {cx, cat_rect.y + 3.0f, 1.5f, lh}, ctx->palette.accent);
            }
        } else {
            const float tw = ui::text_width(f, cl);
            ui::draw_text(*ctx, f, cl, cx - tw / 2.0f, cat_rect.y + 3.0f,
                          sel_cat ? ctx->palette.accent : ctx->palette.text_dim);
            if (sel_cat) {
                ui::fill_rect(*ctx, {cx + tw / 2.0f + 1.5f, cat_rect.y + 3.0f, 1.5f, lh},
                              ctx->palette.accent);
            }
        }

        // Value label above the bar/point (clickable).
        const std::string raw_vl = m.values[i].empty() ? (edit_mode ? "" : "0") : m.values[i];
        const std::string vl = fit_chart_label(f, raw_vl, max_cell_w - 6.0f);
        const float vlw = vl.empty() ? std::min(30.0f, max_cell_w - 4.0f)
                                     : std::max(24.0f, ui::text_width(f, vl) + 10.0f);
        const float anchor = line ? ty : top;
        const float val_y = std::max(y + (m.title.empty() ? 4.0f : kTitleH + 6.0f),
                                     anchor - lh - 9.0f);
        const ui::Rect val_rect{cx - vlw / 2.0f, val_y, vlw, lh + 6.0f};
        if (slots) {
            slots->push_back(Slot{static_cast<int>(i), true, val_rect});
        }
        if (sel_val) {
            ui::fill_rounded_rect(*ctx, val_rect, 4.0f, ctx->palette.button_active);
            ui::stroke_rect(*ctx, val_rect, ctx->palette.accent);
        } else if (edit_mode) {
            dotted_box(*ctx, val_rect, ctx->palette.panel_border);
        }
        if (!vl.empty() && (h > 0.0f || edit_mode)) {
            const float tw = ui::text_width(f, vl);
            ui::draw_text(*ctx, f, vl, cx - tw / 2.0f, val_rect.y + 3.0f,
                          sel_val ? ctx->palette.accent : ctx->palette.text);
            if (sel_val) {
                ui::fill_rect(*ctx, {cx + tw / 2.0f + 1.5f, val_rect.y + 3.0f, 1.5f, lh},
                              ctx->palette.accent);
            }
        } else {
            if (!edit_mode) {
                dotted_box(*ctx, {cx - 12.0f, val_rect.y + 4.0f, 24.0f, 12.0f},
                           ctx->palette.text_dim);
            }
            if (sel_val) {
                ui::fill_rect(*ctx, {cx, val_rect.y + 3.0f, 1.5f, lh}, ctx->palette.accent);
            }
        }
    }

    if (line && pts.size() >= 2) {
        set_color(*ctx, kLine);
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            SDL_RenderLine(ctx->renderer, pts[i].x, pts[i].y, pts[i + 1].x,
                           pts[i + 1].y);
        }
        for (const SDL_FPoint& p : pts) {
            ui::fill_rect(*ctx, {p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f}, kLine);
        }
    }

    return {total_w, top_pad + plot_h + bottom_pad};
}

Size render_pie(ui::Context* ctx, FontBook& fonts, const ChartModel& m,
                float x, float y, float max_w, const Selection* selected,
                std::vector<Slot>* slots) {
    const size_t n = m.labels.size();
    if (n == 0) return placeholder(ctx, fonts, x, y, max_w, "График пуст");

    TTF_Font* f = face(fonts, kFontSize);
    TTF_Font* ft = face(fonts, 15.0f);
    const float lh = font_h(f);

    double total = 0.0;
    for (const std::string& s : m.values) total += std::max(0.0, chart_value(s));
    if (total <= 0.0) {
        for (size_t i = 0; i < n; ++i) total += 1.0; // equal shares
    }

    float size = max_w * 0.48f;
    if (size < 120.0f) size = 120.0f;
    if (size > 170.0f) size = 170.0f;
    const float top_pad = kPad + (m.title.empty() ? 0.0f : kTitleH);
    const float legend_x = x + size + kPad * 2.5f;
    float legend_y = y + top_pad + 10.0f;
    const float legend_h = static_cast<float>(n) * (lh + 10.0f) + 20.0f;
    const float total_h = top_pad + std::max(size, legend_h) + kPad * 2.0f;
    const float total_w = max_w;

    if (!ctx) return {total_w, total_h};

    if (!m.title.empty()) {
        const float tw = ui::text_width(ft, m.title);
        ui::draw_text(*ctx, ft, m.title, x + (total_w - tw) / 2.0f,
                      y + kPad, ctx->palette.text);
    }

    const float cx = x + size / 2.0f + kPad;
    const float cy = y + top_pad + size / 2.0f + 10.0f;
    const float r = size / 2.0f - 6.0f;
    double angle = -kPi / 2.0f; // start at the top

    for (size_t i = 0; i < n; ++i) {
        const double v = total <= 0.0 ? 1.0 : std::max(0.0, chart_value(m.values[i]));
        const double frac = v / total;
        if (frac <= 0.0001) continue;
        const double a0 = angle;
        const double a1 = angle + frac * 2.0f * kPi;
        const int segs = std::max(10, std::min(40, static_cast<int>(frac * 60.0)));
        std::vector<SDL_FPoint> poly;
        poly.push_back({cx, cy});
        for (int k = 0; k <= segs; ++k) {
            const double t = a0 + (a1 - a0) * (static_cast<double>(k) / static_cast<double>(segs));
            poly.push_back({static_cast<float>(cx + std::cos(t) * r),
                            static_cast<float>(cy + std::sin(t) * r)});
        }
        const SDL_Color c = kPieCols[i % 8];
        fill_poly(*ctx, poly, c);
        if (is_selected(selected, static_cast<int>(i), true)
            || is_selected(selected, static_cast<int>(i), false)) {
            set_color(*ctx, ctx->palette.accent);
            for (size_t p = 0; p < poly.size(); ++p) {
                SDL_RenderLine(ctx->renderer, poly[p].x, poly[p].y,
                               poly[(p + 1) % poly.size()].x,
                               poly[(p + 1) % poly.size()].y);
            }
        }
        angle = a1;
    }

    // Legend: labels and values are both clickable.
    const bool edit_mode = (selected != nullptr || slots != nullptr);
    legend_y = y + top_pad + 16.0f;
    for (size_t i = 0; i < n; ++i) {
        const SDL_Color c = kPieCols[i % 8];
        const bool sel_cat = is_selected(selected, static_cast<int>(i), false);
        const bool sel_val = is_selected(selected, static_cast<int>(i), true);
        ui::fill_rect(*ctx, {legend_x, legend_y + 2.0f, 10.0f, 10.0f}, c);
        const std::string cl = m.labels[i].empty() ? (edit_mode ? "" : "Доля") : m.labels[i];
        const std::string vl = m.values[i].empty() ? (edit_mode ? "" : "0") : m.values[i];
        const float label_w = std::max(36.0f, ui::text_width(f, cl) + 12.0f);
        const float val_w = std::max(30.0f, ui::text_width(f, vl) + 12.0f);
        const float gap = 14.0f;
        const ui::Rect cat_rect{legend_x + 16.0f, legend_y - 2.0f, label_w, lh + 6.0f};
        const ui::Rect val_rect{legend_x + 16.0f + label_w + gap, legend_y - 2.0f, val_w, lh + 6.0f};
        if (slots) {
            slots->push_back(Slot{static_cast<int>(i), false, cat_rect});
            slots->push_back(Slot{static_cast<int>(i), true, val_rect});
        }
        if (sel_cat) {
            ui::fill_rounded_rect(*ctx, cat_rect, 4.0f, ctx->palette.button_active);
            ui::stroke_rect(*ctx, cat_rect, ctx->palette.accent);
        } else if (edit_mode) {
            dotted_box(*ctx, cat_rect, ctx->palette.panel_border);
        }
        if (sel_val) {
            ui::fill_rounded_rect(*ctx, val_rect, 4.0f, ctx->palette.button_active);
            ui::stroke_rect(*ctx, val_rect, ctx->palette.accent);
        } else if (edit_mode) {
            dotted_box(*ctx, val_rect, ctx->palette.panel_border);
        }
        if (!cl.empty()) {
            ui::draw_text(*ctx, f, cl, cat_rect.x + 6.0f, legend_y + 1.0f,
                          sel_cat ? ctx->palette.accent : ctx->palette.text);
        }
        if (sel_cat) {
            const float cw = cl.empty() ? 0.0f : ui::text_width(f, cl);
            ui::fill_rect(*ctx, {cat_rect.x + 6.0f + cw + 1.5f, legend_y + 1.0f, 1.5f, lh},
                          ctx->palette.accent);
        }
        if (!vl.empty()) {
            ui::draw_text(*ctx, f, vl, val_rect.x + 6.0f, legend_y + 1.0f,
                          sel_val ? ctx->palette.accent : ctx->palette.text);
        }
        if (sel_val) {
            const float vw = vl.empty() ? 0.0f : ui::text_width(f, vl);
            ui::fill_rect(*ctx, {val_rect.x + 6.0f + vw + 1.5f, legend_y + 1.0f, 1.5f, lh},
                          ctx->palette.accent);
        }
        legend_y += lh + 10.0f;
    }

    return {total_w, total_h};
}

} // namespace

Size render(ui::Context* ctx, FontBook& fonts, const ChartModel& model,
            float x, float y, float max_w,
            const Selection* selected,
            std::vector<Slot>* slots) {
    if (slots) slots->clear();
    if (max_w <= 0.0f) max_w = 100000.0f;
    if ((model.labels.empty() && model.values.empty()) || model.kind == ChartKind::Pie) {
        // A pie with no data is degenerate; fall through to pie drawing which
        // guards itself.
    }
    if (model.kind == ChartKind::Pie) {
        return render_pie(ctx, fonts, model, x, y, max_w, selected, slots);
    }
    return render_xy(ctx, fonts, model, x, y, max_w, selected, slots,
                     model.kind == ChartKind::Line);
}

Size render_block(ui::Context* ctx, FontBook& fonts, const std::string& code,
                  float x, float y, float max_w) {
    bool ok = true;
    const ChartModel model = chart_from_text(code, &ok);
    if (!ok) {
        return placeholder(ctx, fonts, x, y, max_w, "График не поддерживается");
    }
    return render(ctx, fonts, model, x, y, max_w, nullptr, nullptr);
}

} // namespace chartdraw
} // namespace md

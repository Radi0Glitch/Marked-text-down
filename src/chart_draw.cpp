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

    double vmax = 0.0;
    for (const std::string& s : m.values) vmax = std::max(vmax, std::fabs(chart_value(s)));

    // Size the chart to fit the column.
    float total_w = max_w;
    if (total_w < 220.0f) total_w = 220.0f;
    if (total_w > 420.0f) total_w = 420.0f; // keep it readable, not huge
    float plot_h = std::min(190.0f, total_w * 0.42f);
    const float left = kPad + kAxisGap;
    const float bottom_pad = kPad + lh + 10.0f;
    const float top_pad = kPad + (m.title.empty() ? 0.0f : kTitleH);
    const float plot_w = total_w - left - kPad - 10.0f;
    const float plot_x = x + left;
    const float plot_y = y + top_pad;
    const float plot_bottom = plot_y + plot_h;

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
    const int ticks = nice_ticks(vmax);
    const double step = vmax <= 0.0 ? 1.0 : vmax / static_cast<double>(ticks);
    for (int i = 0; i <= ticks; ++i) {
        const float ty = plot_bottom - plot_h * (static_cast<float>(i) / static_cast<float>(ticks));
        SDL_RenderLine(ctx->renderer, plot_x, ty, plot_x + plot_w, ty);
        const double val = step * static_cast<double>(i);
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

    const float group_w = plot_w / static_cast<float>(n);
    std::vector<SDL_FPoint> pts;

    for (size_t i = 0; i < n; ++i) {
        const double v = chart_value(m.values[i]);
        const float h = vmax <= 0.0 ? 0.0f
                                     : plot_h * static_cast<float>(v / vmax);
        const float cx = plot_x + group_w * (static_cast<float>(i) + 0.5f);
        const float ty = plot_bottom - h;

        if (line) {
            pts.push_back({cx, ty});
        } else {
            const float bw = std::min(44.0f, group_w - kBarGap);
            const ui::Rect br{cx - bw / 2.0f, ty, bw, h};
            const SDL_Color bc = bar_color(i);
            ui::fill_rect(*ctx, br, bc);
            if (is_selected(selected, static_cast<int>(i), false)
                || is_selected(selected, static_cast<int>(i), true)) {
                ui::stroke_rect(*ctx, br, ctx->palette.accent);
            }
        }

        // Category label under the group (clickable slot).
        const std::string& cl = m.labels[i];
        if (slots) {
            const float slw = ui::text_width(f, cl.empty() ? " " : cl);
            slots->push_back(Slot{
                static_cast<int>(i), false,
                {cx - slw / 2.0f - 6.0f, plot_bottom + 6.0f, slw + 12.0f, lh + 6.0f}});
        }
        if (cl.empty()) {
            dotted_box(*ctx, {cx - 18.0f, plot_bottom + 8.0f, 36.0f, 12.0f},
                       ctx->palette.text_dim);
        } else {
            const float slw = ui::text_width(f, cl);
            ui::draw_text(*ctx, f, cl, cx - slw / 2.0f, plot_bottom + 8.0f,
                          is_selected(selected, static_cast<int>(i), false)
                              ? ctx->palette.accent
                              : ctx->palette.text_dim);
        }

        // Value label above the bar/point (clickable).
        if (slots) {
            const std::string vl = m.values[i].empty() ? "0" : m.values[i];
            const float vlw = ui::text_width(f, vl);
            slots->push_back(Slot{
                static_cast<int>(i), true,
                {cx - vlw / 2.0f - 6.0f, ty - lh - 8.0f, vlw + 12.0f, lh + 6.0f}});
        }
        if (h > 0.0f) {
            const std::string vl = m.values[i].empty() ? "0" : m.values[i];
            const float vlw = ui::text_width(f, vl);
            ui::draw_text(*ctx, f, vl, cx - vlw / 2.0f, ty - lh - 6.0f,
                          is_selected(selected, static_cast<int>(i), true)
                              ? ctx->palette.accent
                              : ctx->palette.text);
        } else {
            dotted_box(*ctx, {cx - 12.0f, ty - lh - 6.0f, 24.0f, 12.0f},
                       ctx->palette.text_dim);
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
    if (size < 110.0f) size = 110.0f;
    if (size > 150.0f) size = 150.0f;
    const float top_pad = kPad + (m.title.empty() ? 0.0f : kTitleH);
    const float legend_x = x + size + kPad * 2.0f;
    float legend_y = y + top_pad + 10.0f;
    const float total_h = top_pad + size + kPad * 2.0f;
    const float total_w = legend_x + 140.0f;

    if (!ctx) return {std::min(total_w, max_w), total_h};

    if (!m.title.empty()) {
        const float tw = ui::text_width(ft, m.title);
        ui::draw_text(*ctx, ft, m.title, x + (std::min(total_w, max_w) - tw) / 2.0f,
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
        if (is_selected(selected, static_cast<int>(i), true)) {
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
    legend_y = y + top_pad + 16.0f;
    for (size_t i = 0; i < n; ++i) {
        const SDL_Color c = kPieCols[i % 8];
        ui::fill_rect(*ctx, {legend_x, legend_y, 10.0f, 10.0f}, c);
        const std::string cl = m.labels[i].empty() ? "Доля" : m.labels[i];
        const std::string vl = m.values[i].empty() ? "0" : m.values[i];
        const float label_w = ui::text_width(f, cl);
        const float val_w = ui::text_width(f, vl);
        const float gap = 16.0f;
        if (slots) {
            slots->push_back(Slot{
                static_cast<int>(i), false,
                {legend_x + 12.0f, legend_y - 2.0f, label_w + 12.0f, lh + 8.0f}});
            slots->push_back(Slot{
                static_cast<int>(i), true,
                {legend_x + 16.0f + label_w + gap - 2.0f,
                 legend_y - 2.0f, val_w + 12.0f, lh + 8.0f}});
        }
        ui::draw_text(*ctx, f, cl,
                      legend_x + 16.0f, legend_y - 1.0f,
                      is_selected(selected, static_cast<int>(i), false)
                          ? ctx->palette.accent
                          : ctx->palette.text);
        ui::draw_text(*ctx, f, vl,
                      legend_x + 16.0f + label_w + gap,
                      legend_y - 1.0f,
                      is_selected(selected, static_cast<int>(i), true)
                          ? ctx->palette.accent
                          : ctx->palette.text);
        legend_y += lh + 10.0f;
    }

    return {std::min(total_w, max_w), total_h};
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

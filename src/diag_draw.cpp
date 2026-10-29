#include "diag_draw.hpp"

#include <algorithm>
#include <cmath>

namespace md {
namespace diagdraw {
namespace {

// --- style -----------------------------------------------------------------

constexpr float kPad = 12.0f;    // outer margin around the whole picture
constexpr float kGapX = 46.0f;   // nodes side by side (top-down layout)
constexpr float kGapY = 44.0f;   // gap between ranks (top-down layout)
constexpr float kLRGapX = 56.0f; // gap between columns (left-to-right layout)
constexpr float kLRGapY = 30.0f; // stacked nodes (left-to-right layout)
constexpr float kLabelMaxW = 138.0f;
constexpr float kFontSize = 15.0f;
constexpr float kSeqGap = 60.0f;  // horizontal space between actors
constexpr float kSeqStep = 42.0f; // vertical distance between messages
constexpr float kPi = 3.14159265f;

const SDL_Color kFill{54, 62, 86, 255};
const SDL_Color kStroke{104, 118, 146, 255};
const SDL_Color kEdge{150, 162, 186, 255};
const SDL_Color kChipBg{33, 37, 46, 255};
const SDL_Color kLife{82, 92, 112, 255};

TTF_Font* face(FontBook& fonts, float size) {
    return fonts.get(Family::Sans, false, false, size);
}

float font_h(TTF_Font* f) { return static_cast<float>(TTF_GetFontHeight(f)); }

bool is_selected(const Selection* sel, int index, bool edge) {
    return sel != nullptr && sel->index == index && sel->edge == edge;
}

// --- text ------------------------------------------------------------------

std::vector<std::string> wrap_text(TTF_Font* f, const std::string& text,
                                   float max_w) {
    std::vector<std::string> out;
    if (text.empty()) {
        out.push_back("");
        return out;
    }
    std::vector<std::string> words;
    std::string cur;
    for (char c : text) {
        if (c == ' ') {
            if (!cur.empty()) words.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) words.push_back(cur);

    std::string line;
    for (const std::string& w : words) {
        const std::string cand = line.empty() ? w : line + " " + w;
        if (ui::text_width(f, cand) <= max_w) {
            line = cand;
            continue;
        }
        if (!line.empty()) {
            out.push_back(line);
            line.clear();
        }
        if (ui::text_width(f, w) <= max_w) {
            line = w;
            continue;
        }
        // A single word wider than the box: split it by UTF-8 characters.
        size_t pos = 0;
        std::string piece;
        while (pos < w.size()) {
            size_t n = 1;
            while (pos + n < w.size()
                   && (static_cast<unsigned char>(w[pos + n]) & 0xC0) == 0x80) {
                ++n;
            }
            const std::string step = piece + w.substr(pos, n);
            if (!piece.empty() && ui::text_width(f, step) > max_w) {
                out.push_back(piece);
                piece = w.substr(pos, n);
            } else {
                piece = step;
            }
            pos += n;
        }
        line = piece;
    }
    if (!line.empty()) out.push_back(line);
    if (out.empty()) out.push_back("");
    return out;
}

// --- shapes ----------------------------------------------------------------

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

void stroke_poly(ui::Context& ctx, const std::vector<SDL_FPoint>& p, SDL_Color c) {
    if (p.size() < 2) return;
    set_color(ctx, c);
    for (size_t i = 0; i < p.size(); ++i) {
        const SDL_FPoint& a = p[i];
        const SDL_FPoint& b = p[(i + 1) % p.size()];
        SDL_RenderLine(ctx.renderer, a.x, a.y, b.x, b.y);
    }
}

void add_arc(std::vector<SDL_FPoint>& out, float cx, float cy, float r,
             float a0, float a1, int segs) {
    for (int k = 0; k <= segs; ++k) {
        const float t = a0 + (a1 - a0) * (static_cast<float>(k) / static_cast<float>(segs));
        out.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r});
    }
}

std::vector<SDL_FPoint> shape_poly(DiagShape s, ui::Rect r) {
    std::vector<SDL_FPoint> p;
    switch (s) {
        case DiagShape::Box:
            return {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};
        case DiagShape::Diamond:
            return {{r.x + r.w / 2, r.y}, {r.x + r.w, r.y + r.h / 2},
                    {r.x + r.w / 2, r.y + r.h}, {r.x, r.y + r.h / 2}};
        case DiagShape::Ellipse: {
            const float cx = r.x + r.w / 2;
            const float cy = r.y + r.h / 2;
            for (int k = 0; k < 28; ++k) {
                const float t = 2.0f * kPi * static_cast<float>(k) / 28.0f;
                p.push_back({cx + std::cos(t) * r.w / 2, cy + std::sin(t) * r.h / 2});
            }
            return p;
        }
        case DiagShape::Rounded:
        case DiagShape::Stadium:
        default: {
            const float rr = s == DiagShape::Stadium
                                 ? std::min(r.w, r.h) / 2
                                 : std::min(10.0f, std::min(r.w, r.h) / 2);
            const int segs = rr > 24.0f ? 6 : (rr > 8.0f ? 4 : 2);
            add_arc(p, r.x + rr, r.y + rr, rr, kPi, 1.5f * kPi, segs);       // top-left
            add_arc(p, r.x + r.w - rr, r.y + rr, rr, 1.5f * kPi, 2.0f * kPi, segs); // top-right
            add_arc(p, r.x + r.w - rr, r.y + r.h - rr, rr, 0.0f, 0.5f * kPi, segs); // bottom-right
            add_arc(p, r.x + rr, r.y + r.h - rr, rr, 0.5f * kPi, kPi, segs); // bottom-left
            return p;
        }
    }
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

// --- arrows ----------------------------------------------------------------

void line(ui::Context& ctx, SDL_FPoint a, SDL_FPoint b, SDL_Color c) {
    set_color(ctx, c);
    SDL_RenderLine(ctx.renderer, a.x, a.y, b.x, b.y);
}

void dashed_line(ui::Context& ctx, SDL_FPoint a, SDL_FPoint b, SDL_Color c) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    const float ux = dx / len;
    const float uy = dy / len;
    set_color(ctx, c);
    for (float s = 0; s < len; s += 9.0f) {
        const float e = std::min(s + 6.0f, len);
        SDL_RenderLine(ctx.renderer, a.x + ux * s, a.y + uy * s,
                       a.x + ux * e, a.y + uy * e);
    }
}

void arrow_head(ui::Context& ctx, SDL_FPoint tip, SDL_FPoint dir, float size,
                SDL_Color c) {
    const float bx = tip.x - dir.x * size;
    const float by = tip.y - dir.y * size;
    const float px = -dir.y * (size * 0.42f);
    const float py = dir.x * (size * 0.42f);
    fill_poly(ctx, {{tip.x, tip.y}, {bx + px, by + py}, {bx - px, by - py}}, c);
}

// The box an arrow attaches to: tapered shapes are hit slightly inside.
ui::Rect anchor_rect(DiagShape s, ui::Rect r) {
    if (s == DiagShape::Diamond) {
        return {r.x + r.w * 0.24f, r.y + r.h * 0.06f, r.w * 0.52f, r.h * 0.88f};
    }
    if (s == DiagShape::Ellipse) {
        return {r.x + r.w * 0.12f, r.y + r.h * 0.08f, r.w * 0.76f, r.h * 0.84f};
    }
    return r;
}

// Where a ray from the box centre leaves the box on its way to `d`.
bool ray_exit(ui::Rect r, SDL_FPoint c, SDL_FPoint d, SDL_FPoint& out) {
    float best = 1.0e30f;
    if (d.x > 1.0e-6f) best = std::min(best, (r.x + r.w - c.x) / d.x);
    if (d.x < -1.0e-6f) best = std::min(best, (r.x - c.x) / d.x);
    if (d.y > 1.0e-6f) best = std::min(best, (r.y + r.h - c.y) / d.y);
    if (d.y < -1.0e-6f) best = std::min(best, (r.y - c.y) / d.y);
    if (best > 1.0e29f) return false;
    out = {c.x + d.x * best, c.y + d.y * best};
    return true;
}

// A label chip sitting on an edge; also used as its click target.
ui::Rect draw_chip(ui::Context& ctx, TTF_Font* f, const std::string& text,
                   SDL_FPoint mid, bool selected, float lh) {
    const bool light = ui::is_light_palette(ctx.palette);
    const SDL_Color chip_bg = light ? SDL_Color{245, 247, 252, 255} : kChipBg;
    const float tw = ui::text_width(f, text);
    const float cw = tw + 16.0f;
    const float ch = lh + 7.0f;
    const ui::Rect r{mid.x - cw / 2, mid.y - ch / 2, cw, ch};
    ui::fill_rounded_rect(ctx, r, 4.0f, chip_bg);
    ui::stroke_rect(ctx, r, selected ? ctx.palette.accent : ctx.palette.panel_border);
    ui::draw_text(ctx, f, text, r.x + 8.0f, r.y + (ch - lh) / 2.0f,
                  selected ? ctx.palette.accent : ctx.palette.text);
    return r;
}

// --- placeholder -----------------------------------------------------------

Size placeholder(ui::Context* ctx, FontBook& fonts, float x, float y,
                 float max_w, const char* text) {
    TTF_Font* f = face(fonts, 14.0f);
    std::string t = text;
    float tw = ui::text_width(f, t);
    if (max_w > 0.0f && tw + 40.0f > max_w) {
        t = "Диаграмма";
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

// --- flowchart -------------------------------------------------------------

struct NodeBox {
    std::vector<std::string> lines;
    float tw = 0;
    float th = 0;
    float w = 0;
    float h = 0;
};

std::vector<NodeBox> measure_nodes(const DiagModel& m, FontBook& fonts,
                                   float fs) {
    std::vector<NodeBox> out(m.nodes.size());
    TTF_Font* f = face(fonts, fs);
    const float lh = font_h(f);
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        NodeBox& b = out[i];
        b.lines = wrap_text(f, m.nodes[i].label, kLabelMaxW);
        for (const std::string& l : b.lines) {
            b.tw = std::max(b.tw, ui::text_width(f, l));
        }
        b.tw = std::max(b.tw, 26.0f);
        b.th = static_cast<float>(b.lines.size()) * lh;

        float padx = 14.0f;
        float pady = 9.0f;
        if (m.nodes[i].shape == DiagShape::Rounded) padx = 18.0f;
        if (m.nodes[i].shape == DiagShape::Stadium) padx = 26.0f;
        b.w = b.tw + 2.0f * padx;
        b.h = b.th + 2.0f * pady;

        switch (m.nodes[i].shape) {
            case DiagShape::Diamond:
                // The usable area inside a rhombus is much smaller than its box.
                b.w = std::max(std::max(b.w, b.tw * 1.45f + 12.0f), 72.0f);
                b.h = std::max(std::max(b.h, b.th * 1.62f + 10.0f), 48.0f);
                break;
            case DiagShape::Ellipse:
                b.w = std::max(std::max(b.w, b.tw * 1.36f + 16.0f), 64.0f);
                b.h = std::max(std::max(b.h, b.th * 1.40f + 12.0f), 44.0f);
                break;
            default:
                b.w = std::max(b.w, 62.0f);
                b.h = std::max(b.h, 34.0f);
                break;
        }
    }
    return out;
}

struct FlowGeom {
    std::vector<ui::Rect> rects;
    float w = 0;
    float h = 0;
};

// Longest-path layering (Kahn), then pack each rank centred. Nodes inside a
// cycle have no topological order and drop to the row below everything.
FlowGeom layout_flow(const DiagModel& m, const std::vector<NodeBox>& boxes) {
    FlowGeom g;
    const int n = static_cast<int>(m.nodes.size());
    g.rects.assign(static_cast<size_t>(n), ui::Rect{});
    if (n == 0) return g;

    std::vector<std::pair<int, int>> pairs;
    std::vector<int> deg(static_cast<size_t>(n), 0);
    for (const DiagEdge& e : m.edges) {
        const int u = diag_find_node(m, e.from);
        const int v = diag_find_node(m, e.to);
        if (u < 0 || v < 0 || u == v) continue;
        pairs.push_back({u, v});
        deg[static_cast<size_t>(v)]++;
    }

    std::vector<int> order;
    order.reserve(static_cast<size_t>(n));
    std::vector<bool> done(static_cast<size_t>(n), false);
    {
        std::vector<int> work;
        for (int i = 0; i < n; ++i) {
            if (deg[static_cast<size_t>(i)] == 0) work.push_back(i);
        }
        while (!work.empty()) {
            const int v = work.back();
            work.pop_back();
            if (done[static_cast<size_t>(v)]) continue;
            done[static_cast<size_t>(v)] = true;
            order.push_back(v);
            for (const auto& pr : pairs) {
                if (pr.first != v) continue;
                if (--deg[static_cast<size_t>(pr.second)] == 0) work.push_back(pr.second);
            }
        }
    }

    std::vector<int> rank(static_cast<size_t>(n), 0);
    for (int v : order) {
        for (const auto& pr : pairs) {
            if (pr.first != v) continue;
            rank[static_cast<size_t>(pr.second)] =
                std::max(rank[static_cast<size_t>(pr.second)],
                         rank[static_cast<size_t>(v)] + 1);
        }
    }
    int max_rank = -1;
    for (int v : order) max_rank = std::max(max_rank, rank[static_cast<size_t>(v)]);
    for (int i = 0; i < n; ++i) {
        if (!done[static_cast<size_t>(i)]) rank[static_cast<size_t>(i)] = max_rank + 1;
    }

    int maxr = 0;
    for (int r : rank) maxr = std::max(maxr, r);
    std::vector<std::vector<int>> rows(static_cast<size_t>(maxr) + 1);
    for (int i = 0; i < n; ++i) rows[static_cast<size_t>(rank[static_cast<size_t>(i)])].push_back(i);

    if (m.lr) {
        std::vector<float> colw(rows.size(), 0.0f);
        std::vector<float> colh(rows.size(), 0.0f);
        for (size_t c = 0; c < rows.size(); ++c) {
            for (size_t k = 0; k < rows[c].size(); ++k) {
                const int i = rows[c][k];
                colw[c] = std::max(colw[c], boxes[static_cast<size_t>(i)].w);
                colh[c] += boxes[static_cast<size_t>(i)].h
                    + (k > 0 ? kLRGapY : 0.0f);
            }
        }
        float total_w = 2.0f * kPad;
        float max_h = 0.0f;
        for (size_t c = 0; c < rows.size(); ++c) {
            total_w += colw[c] + (c > 0 ? kLRGapX : 0.0f);
            max_h = std::max(max_h, colh[c]);
        }
        g.w = std::max(total_w, 2.0f * kPad);
        g.h = max_h + 2.0f * kPad;
        float x = kPad;
        for (size_t c = 0; c < rows.size(); ++c) {
            float y = kPad + (max_h - colh[c]) * 0.5f;
            for (int i : rows[c]) {
                const NodeBox& b = boxes[static_cast<size_t>(i)];
                g.rects[static_cast<size_t>(i)] =
                    {x + (colw[c] - b.w) / 2.0f, y, b.w, b.h};
                y += b.h + kLRGapY;
            }
            x += colw[c] + kLRGapX;
        }
        return g;
    }

    std::vector<float> roww(rows.size(), 0.0f);
    std::vector<float> rowh(rows.size(), 0.0f);
    for (size_t r = 0; r < rows.size(); ++r) {
        for (size_t k = 0; k < rows[r].size(); ++k) {
            const int i = rows[r][k];
            roww[r] += boxes[static_cast<size_t>(i)].w
                + (k > 0 ? kGapX : 0.0f);
            rowh[r] = std::max(rowh[r], boxes[static_cast<size_t>(i)].h);
        }
    }
    float max_w = 0.0f;
    for (float w : roww) max_w = std::max(max_w, w);
    g.w = max_w + 2.0f * kPad;
    float y = kPad;
    for (size_t r = 0; r < rows.size(); ++r) {
        if (rows[r].empty()) continue;
        float x = kPad + (max_w - roww[r]) * 0.5f;
        for (int i : rows[r]) {
            const NodeBox& b = boxes[static_cast<size_t>(i)];
            g.rects[static_cast<size_t>(i)] = {x, y, b.w, b.h};
            x += b.w + kGapX;
        }
        y += rowh[r] + kGapY;
    }
    g.h = y - kGapY + kPad;
    return g;
}

Size render_flow(ui::Context* ctx, FontBook& fonts, const DiagModel& m,
                 float x, float y, float max_w, const Selection* selected,
                 std::vector<Slot>* slots) {
    float fs = kFontSize;
    std::vector<NodeBox> boxes = measure_nodes(m, fonts, fs);
    FlowGeom geom = layout_flow(m, boxes);
    if (geom.w > max_w) {
        // Too wide for the column: smaller text makes narrower rows.
        fs = std::max(9.0f, std::floor(fs * (max_w / geom.w)));
        boxes = measure_nodes(m, fonts, fs);
        geom = layout_flow(m, boxes);
    }
    const float scale = geom.w > max_w ? max_w / geom.w : 1.0f;
    if (!ctx) return {geom.w * scale, geom.h * scale};

    const bool light = ui::is_light_palette(ctx->palette);
    const SDL_Color fill_col = light ? SDL_Color{224, 232, 248, 255} : kFill;
    const SDL_Color stroke_col = light ? SDL_Color{136, 154, 186, 255} : kStroke;
    const SDL_Color edge_col = light ? SDL_Color{96, 112, 140, 255} : kEdge;

    auto X = [&](float v) { return x + v * scale; };
    auto Y = [&](float v) { return y + v * scale; };
    TTF_Font* f = face(fonts, fs);
    const float lh = font_h(f);
    std::vector<Slot> edge_slots;

    // Edges first so the nodes sit on top of them.
    for (size_t ei = 0; ei < m.edges.size(); ++ei) {
        const DiagEdge& e = m.edges[ei];
        const int u = diag_find_node(m, e.from);
        const int v = diag_find_node(m, e.to);
        if (u < 0 || v < 0) continue;
        const bool on = is_selected(selected, static_cast<int>(ei), true);

        if (u == v) {
            // Self-loops keep a click target above the node.
            const ui::Rect r = geom.rects[static_cast<size_t>(u)];
            edge_slots.push_back(Slot{static_cast<int>(ei), true,
                                      {X(r.x + r.w - 30), Y(r.y) - 24.0f, 34.0f, 22.0f}});
            continue;
        }

        const ui::Rect ru = geom.rects[static_cast<size_t>(u)];
        const ui::Rect rv = geom.rects[static_cast<size_t>(v)];
        const ui::Rect su{X(ru.x), Y(ru.y), ru.w * scale, ru.h * scale};
        const ui::Rect sv{X(rv.x), Y(rv.y), rv.w * scale, rv.h * scale};
        const SDL_FPoint cu{su.x + su.w / 2.0f, su.y + su.h / 2.0f};
        const SDL_FPoint cv{sv.x + sv.w / 2.0f, sv.y + sv.h / 2.0f};
        const float dx = cv.x - cu.x;
        const float dy = cv.y - cu.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) continue;
        const SDL_FPoint udir{dx / len, dy / len};
        SDL_FPoint a, b;
        if (!ray_exit(anchor_rect(m.nodes[static_cast<size_t>(u)].shape, su),
                      cu, udir, a)) {
            continue;
        }
        if (!ray_exit(anchor_rect(m.nodes[static_cast<size_t>(v)].shape, sv),
                      cv, SDL_FPoint{-udir.x, -udir.y}, b)) {
            continue;
        }
        const SDL_Color col = on ? ctx->palette.accent : edge_col;
        if (e.dashed) {
            dashed_line(*ctx, a, b, col);
        } else {
            line(*ctx, a, b, col);
        }
        arrow_head(*ctx, b, udir, on ? 11.0f : 9.0f, col);

        const SDL_FPoint mid{(a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f};
        if (!e.label.empty()) {
            edge_slots.push_back(
                Slot{static_cast<int>(ei), true, draw_chip(*ctx, f, e.label, mid, on, lh)});
        } else {
            if (on) {
                ui::fill_rect(*ctx, {mid.x - 4.0f, mid.y - 4.0f, 8.0f, 8.0f},
                              ctx->palette.accent);
            }
            edge_slots.push_back(Slot{static_cast<int>(ei), true,
                                      {mid.x - 16.0f, mid.y - 11.0f, 32.0f, 22.0f}});
        }
    }

    // Nodes on top.
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const ui::Rect r0 = geom.rects[i];
        const ui::Rect r{X(r0.x), Y(r0.y), r0.w * scale, r0.h * scale};
        const bool on = is_selected(selected, static_cast<int>(i), false);
        const std::vector<SDL_FPoint> poly =
            shape_poly(m.nodes[i].shape, r);
        fill_poly(*ctx, poly, fill_col);
        stroke_poly(*ctx, poly, on ? ctx->palette.accent : stroke_col);

        const NodeBox& b = boxes[i];
        const float cx = r.x + r.w / 2.0f;
        const float cy = r.y + r.h / 2.0f;
        if (m.nodes[i].label.empty()) {
            dotted_box(*ctx, {cx - 22.0f, cy - 7.0f, 44.0f, 14.0f},
                       ctx->palette.text_dim);
        } else {
            float ty = cy - b.th / 2.0f;
            for (size_t li = 0; li < b.lines.size(); ++li) {
                const float lw = ui::text_width(f, b.lines[li]);
                ui::draw_text(*ctx, f, b.lines[li], cx - lw / 2.0f, ty,
                              ctx->palette.text);
                ty += lh;
            }
            if (on) {
                // Caret at the end of the label.
                const float lw = ui::text_width(f, b.lines.back());
                ui::fill_rect(*ctx,
                              {cx + lw / 2.0f + 2.0f, cy - b.th / 2.0f
                                   + static_cast<float>(b.lines.size() - 1) * lh,
                               1.5f, lh},
                              ctx->palette.accent);
            }
        }
        if (slots) slots->push_back(Slot{static_cast<int>(i), false, r});
    }
    for (const Slot& s : edge_slots) {
        if (slots) slots->push_back(s);
    }
    return {geom.w * scale, geom.h * scale};
}

// --- sequence ----------------------------------------------------------------

struct SeqGeom {
    std::vector<ui::Rect> boxes;
    std::vector<float> life_x;
    std::vector<float> msg_y;
    float life_bottom = 0;
    float w = 0;
    float h = 0;
};

SeqGeom layout_seq(const DiagModel& m, FontBook& fonts, float fs) {
    SeqGeom g;
    TTF_Font* f = face(fonts, fs);
    const float bh = font_h(f) + 16.0f;
    g.boxes.assign(m.nodes.size(), ui::Rect{});
    g.life_x.assign(m.nodes.size(), 0.0f);
    float x = kPad;
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const float tw = ui::text_width(f, m.nodes[i].label);
        const float bw = std::max(tw + 28.0f, 58.0f);
        g.boxes[i] = {x, kPad, bw, bh};
        g.life_x[i] = x + bw / 2.0f;
        x += bw + kSeqGap;
    }
    g.w = m.nodes.empty() ? 2.0f * kPad : x - kSeqGap + kPad;
    g.msg_y.assign(m.edges.size(), -1.0f);
    const float first = kPad + bh + 34.0f;
    for (size_t j = 0; j < m.edges.size(); ++j) {
        g.msg_y[j] = first + kSeqStep * static_cast<float>(j);
    }
    const float bottom = m.edges.empty()
        ? kPad + bh + 46.0f
        : first + kSeqStep * static_cast<float>(m.edges.size() - 1) + 22.0f;
    g.life_bottom = bottom;
    g.h = bottom + kPad;
    return g;
}

Size render_sequence(ui::Context* ctx, FontBook& fonts, const DiagModel& m,
                     float x, float y, float max_w, const Selection* selected,
                     std::vector<Slot>* slots) {
    float fs = kFontSize;
    SeqGeom geom = layout_seq(m, fonts, fs);
    if (geom.w > max_w) {
        fs = std::max(9.0f, std::floor(fs * (max_w / geom.w)));
        geom = layout_seq(m, fonts, fs);
    }
    const float scale = geom.w > max_w ? max_w / geom.w : 1.0f;
    if (!ctx) return {geom.w * scale, geom.h * scale};

    const bool light = ui::is_light_palette(ctx->palette);
    const SDL_Color fill_col = light ? SDL_Color{224, 232, 248, 255} : kFill;
    const SDL_Color stroke_col = light ? SDL_Color{136, 154, 186, 255} : kStroke;
    const SDL_Color edge_col = light ? SDL_Color{96, 112, 140, 255} : kEdge;
    const SDL_Color life_col = light ? SDL_Color{156, 168, 190, 255} : kLife;

    auto X = [&](float v) { return x + v * scale; };
    auto Y = [&](float v) { return y + v * scale; };
    TTF_Font* f = face(fonts, fs);
    const float lh = font_h(f);
    std::vector<Slot> edge_slots;

    // Lifelines under everything.
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const float lx = X(geom.life_x[i]);
        dashed_line(*ctx, {lx, Y(kPad) + geom.boxes[i].h * scale},
                    {lx, Y(geom.life_bottom)}, life_col);
    }

    // Messages.
    for (size_t ei = 0; ei < m.edges.size(); ++ei) {
        const DiagEdge& e = m.edges[ei];
        const int u = diag_find_node(m, e.from);
        const int v = diag_find_node(m, e.to);
        if (u < 0 || v < 0) continue;
        const bool on = is_selected(selected, static_cast<int>(ei), true);
        const SDL_Color col = on ? ctx->palette.accent : edge_col;
        const float my = Y(geom.msg_y[ei]);
        float x1 = X(geom.life_x[static_cast<size_t>(u)]);
        float x2 = X(geom.life_x[static_cast<size_t>(v)]);
        SDL_FPoint label_at{(x1 + x2) / 2.0f, my - 15.0f};
        ui::Rect click{x1, my - 10.0f, x2 - x1, 20.0f};

        if (u == v) {
            // Self message: a small U-turn next to the lifeline.
            const float right = x1 + 34.0f;
            line(*ctx, {x1, my}, {right, my}, col);
            line(*ctx, {right, my}, {right, my + 16.0f}, col);
            line(*ctx, {right, my + 16.0f}, {x1, my + 16.0f}, col);
            arrow_head(*ctx, {x1, my + 16.0f}, {-1.0f, 0.0f}, 9.0f, col);
            label_at = {right + 8.0f, my + 8.0f};
            click = {x1, my - 6.0f, right - x1 + 6.0f, 28.0f};
        } else {
            const float dir = x2 > x1 ? 1.0f : -1.0f;
            if (e.dashed) {
                dashed_line(*ctx, {x1, my}, {x2, my}, col);
            } else {
                line(*ctx, {x1, my}, {x2, my}, col);
            }
            arrow_head(*ctx, {x2, my}, {dir, 0.0f}, on ? 11.0f : 9.0f, col);
        }

        if (!e.label.empty()) {
            const ui::Rect chip = draw_chip(*ctx, f, e.label, label_at, on, lh);
            edge_slots.push_back(Slot{static_cast<int>(ei), true, chip});
        } else {
            if (on) {
                ui::fill_rect(*ctx, {label_at.x - 4.0f, label_at.y - 4.0f, 8.0f, 8.0f},
                              ctx->palette.accent);
            }
            edge_slots.push_back(
                Slot{static_cast<int>(ei), true,
                     {label_at.x - 16.0f, label_at.y - 11.0f, 32.0f, 22.0f}});
        }
    }

    // Actor boxes on top of the lifelines.
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        const ui::Rect r0 = geom.boxes[i];
        const ui::Rect r{X(r0.x), Y(r0.y), r0.w * scale, r0.h * scale};
        const bool on = is_selected(selected, static_cast<int>(i), false);
        const std::vector<SDL_FPoint> poly = shape_poly(DiagShape::Box, r);
        fill_poly(*ctx, poly, fill_col);
        stroke_poly(*ctx, poly, on ? ctx->palette.accent : stroke_col);
        const std::string& label = m.nodes[i].label;
        if (label.empty()) {
            dotted_box(*ctx, {r.x + r.w / 2.0f - 22.0f, r.y + r.h / 2.0f - 7.0f,
                              44.0f, 14.0f},
                       ctx->palette.text_dim);
        } else {
            const float lw = ui::text_width(f, label);
            ui::draw_text(*ctx, f, label, r.x + (r.w - lw) / 2.0f,
                          r.y + (r.h - lh) / 2.0f, ctx->palette.text);
            if (on) {
                ui::fill_rect(*ctx,
                              {r.x + (r.w + lw) / 2.0f + 2.0f, r.y + (r.h - lh) / 2.0f,
                               1.5f, lh},
                              ctx->palette.accent);
            }
        }
        if (slots) slots->push_back(Slot{static_cast<int>(i), false, r});
    }
    for (const Slot& s : edge_slots) {
        if (slots) slots->push_back(s);
    }
    return {geom.w * scale, geom.h * scale};
}

} // namespace

// --- public -----------------------------------------------------------------

Size render(ui::Context* ctx, FontBook& fonts, const DiagModel& model,
            float x, float y, float max_w, const Selection* selected,
            std::vector<Slot>* slots) {
    if (slots) slots->clear();
    if (max_w <= 0.0f) max_w = 100000.0f;
    if (model.nodes.empty() && model.edges.empty()) {
        return placeholder(ctx, fonts, x, y, max_w, "Диаграмма пуста");
    }
    if (model.kind == DiagKind::Sequence) {
        return render_sequence(ctx, fonts, model, x, y, max_w, selected, slots);
    }
    return render_flow(ctx, fonts, model, x, y, max_w, selected, slots);
}

Size render_block(ui::Context* ctx, FontBook& fonts, const std::string& code,
                  float x, float y, float max_w) {
    bool ok = true;
    const DiagModel model = diag_from_text(code, &ok);
    if (!ok) {
        // Never draw what we cannot visualise: the code stays in the file.
        return placeholder(ctx, fonts, x, y, max_w, "Диаграмма не поддерживается");
    }
    return render(ctx, fonts, model, x, y, max_w, nullptr, nullptr);
}

} // namespace diagdraw
} // namespace md

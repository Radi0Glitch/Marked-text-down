#include "math_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace md {
namespace mathdraw {

namespace {

TTF_Font* face(FontBook& fonts, float size) {
    return fonts.get(Family::Sans, false, false, size);
}

float font_h(TTF_Font* f) { return static_cast<float>(TTF_GetFontHeight(f)); }

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

bool same_path(const std::vector<int>& a, const std::vector<int>* b) {
    return b && a.size() == b->size() && std::equal(a.begin(), a.end(), b->begin());
}

void mark_cursor(ui::Context& ctx, const std::vector<int>& path,
                 const std::vector<int>* hl, ui::Rect r) {
    if (!same_path(path, hl)) return;
    ui::stroke_rect(ctx, {r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, r.h + 4.0f}, ctx.palette.accent);
    ui::fill_rect(ctx, {r.x + r.w + 1.0f, r.y + 1.0f, 1.5f, r.h - 2.0f}, ctx.palette.accent);
}

Size go(ui::Context* ctx, FontBook& fonts, const MathNode& n, float x, float y,
        float size, const std::vector<int>* hl, std::vector<Slot>* slots,
        const std::vector<int>& path) {
    const bool draw = ctx != nullptr;
    const float s2 = size * 0.72f; // super/subscript and limit scale

    switch (n.type) {
        case MathNode::Text: {
            TTF_Font* f = face(fonts, size);
            const float h = font_h(f);
            const float w = n.text.empty() ? size * 0.55f : ui::text_width(f, n.text);
            const ui::Rect rect{x, y, w, h};
            if (slots) slots->push_back(Slot{path, rect});
            if (draw) {
                if (n.text.empty()) {
                    dotted_box(*ctx, {x, y + h * 0.15f, w, h * 0.7f}, ctx->palette.text_dim);
                } else {
                    ui::draw_text(*ctx, f, n.text, x, y, ctx->palette.text);
                }
                mark_cursor(*ctx, path, hl, rect);
            }
            return {w, h};
        }

        case MathNode::Seq: {
            if (n.slots.empty()) {
                // An empty row is still a place to type.
                const float w = size * 0.55f;
                const float h = font_h(face(fonts, size)) * 0.85f;
                const ui::Rect rect{x, y + h * 0.1f, w, h};
                if (slots) slots->push_back(Slot{path, rect});
                if (draw) {
                    dotted_box(*ctx, rect, ctx->palette.text_dim);
                    mark_cursor(*ctx, path, hl, rect);
                }
                return {w, h};
            }
            std::vector<Size> ss;
            ss.reserve(n.slots.size());
            float total = 0, mh = 0;
            for (const MathNode& c : n.slots) {
                Size s = go(nullptr, fonts, c, 0, 0, size, hl, nullptr, path);
                ss.push_back(s);
                total += s.w;
                mh = std::max(mh, s.h);
            }
            if (draw) {
                float cx = x;
                for (size_t i = 0; i < n.slots.size(); ++i) {
                    std::vector<int> cp = path;
                    cp.push_back(static_cast<int>(i));
                    go(ctx, fonts, n.slots[i], cx, y + (mh - ss[i].h) / 2.0f,
                       size, hl, slots, cp);
                    cx += ss[i].w;
                }
            }
            return {total, mh};
        }

        case MathNode::Frac: {
            if (n.slots.size() < 2) return go(ctx, fonts, MathNode{MathNode::Seq, {}, {}},
                                              x, y, size, hl, slots, path);
            const Size num = go(nullptr, fonts, n.slots[0], 0, 0, size, hl, nullptr, path);
            const Size den = go(nullptr, fonts, n.slots[1], 0, 0, size, hl, nullptr, path);
            const float gap = size * 0.12f;
            const float line_h = std::max(1.3f, size * 0.07f);
            const float w = std::max(num.w, den.w) + size * 0.35f;
            const float h = num.h + den.h + line_h + 2.0f * gap;
            if (draw) {
                const float mid_x = x + (w - std::max(num.w, den.w)) / 2.0f;
                std::vector<int> p0 = path; p0.push_back(0);
                std::vector<int> p1 = path; p1.push_back(1);
                go(ctx, fonts, n.slots[0], mid_x, y, size, hl, slots, p0);
                ui::fill_rect(*ctx, {x + size * 0.1f, y + num.h + gap,
                                     w - size * 0.2f, line_h}, ctx->palette.text);
                go(ctx, fonts, n.slots[1], mid_x + (std::max(num.w, den.w) - den.w) / 2.0f,
                   y + num.h + gap + line_h + gap, size, hl, slots, p1);
            }
            return {w, h};
        }

        case MathNode::Sqrt:
        case MathNode::Root: {
            const bool has_index = n.type == MathNode::Root && n.slots.size() >= 2;
            TTF_Font* f = face(fonts, size);
            const std::string rad = "\xE2\x88\x9A"; // √
            const float rw = ui::text_width(f, rad) + size * 0.05f;
            Size is{0, font_h(f)};
            if (!n.slots.empty()) {
                is = go(nullptr, fonts, n.slots.back(), 0, 0, size, hl, nullptr, path);
            }
            Size idx{0, 0};
            if (has_index) {
                idx = go(nullptr, fonts, n.slots[0], 0, 0, s2, hl, nullptr, path);
            }
            const float w = idx.w + rw + is.w + size * 0.1f;
            const float h = std::max(is.h + size * 0.12f, font_h(f));
            if (draw) {
                float bx = x + idx.w;
                if (has_index) {
                    std::vector<int> p0 = path; p0.push_back(0);
                    go(ctx, fonts, n.slots[0], x, y, s2, hl, slots, p0);
                }
                ui::draw_text(*ctx, f, rad, bx, y + h - font_h(f) * 0.92f, ctx->palette.text);
                // overline
                ui::fill_rect(*ctx, {bx + rw - size * 0.05f, y + size * 0.04f,
                                     is.w + size * 0.15f, std::max(1.2f, size * 0.06f)},
                              ctx->palette.text);
                if (!n.slots.empty()) {
                    std::vector<int> pl = path;
                    pl.push_back(static_cast<int>(n.slots.size()) - 1);
                    go(ctx, fonts, n.slots.back(), bx + rw, y + size * 0.1f,
                       size, hl, slots, pl);
                }
            }
            return {w, h};
        }

        case MathNode::Sup:
        case MathNode::Sub: {
            if (n.slots.size() < 2) return go(ctx, fonts, MathNode{MathNode::Seq, {}, {}},
                                              x, y, size, hl, slots, path);
            const Size base = go(nullptr, fonts, n.slots[0], 0, 0, size, hl, nullptr, path);
            const Size acc = go(nullptr, fonts, n.slots[1], 0, 0, s2, hl, nullptr, path);
            const float off = size * 0.38f;
            const float w = base.w + acc.w;
            const float h = base.h + off;
            if (draw) {
                std::vector<int> p0 = path; p0.push_back(0);
                std::vector<int> p1 = path; p1.push_back(1);
                if (n.type == MathNode::Sup) {
                    go(ctx, fonts, n.slots[0], x, y + off, size, hl, slots, p0);
                    go(ctx, fonts, n.slots[1], x + base.w, y, s2, hl, slots, p1);
                } else {
                    go(ctx, fonts, n.slots[0], x, y, size, hl, slots, p0);
                    go(ctx, fonts, n.slots[1], x + base.w, y + off, s2, hl, slots, p1);
                }
            }
            return {w, h};
        }

        case MathNode::SupSub: {
            if (n.slots.size() < 3) return go(ctx, fonts, MathNode{MathNode::Seq, {}, {}},
                                              x, y, size, hl, slots, path);
            const Size base = go(nullptr, fonts, n.slots[0], 0, 0, size, hl, nullptr, path);
            const Size sub = go(nullptr, fonts, n.slots[1], 0, 0, s2, hl, nullptr, path);
            const Size sup = go(nullptr, fonts, n.slots[2], 0, 0, s2, hl, nullptr, path);
            const float off = size * 0.38f;
            const float acc_w = std::max(sub.w, sup.w);
            const float w = base.w + acc_w;
            const float h = base.h + off * 2.0f;
            if (draw) {
                std::vector<int> p0 = path; p0.push_back(0);
                std::vector<int> p1 = path; p1.push_back(1);
                std::vector<int> p2 = path; p2.push_back(2);
                go(ctx, fonts, n.slots[0], x, y + off, size, hl, slots, p0);
                go(ctx, fonts, n.slots[1], x + base.w, y + h - sub.h, s2, hl, slots, p1);
                go(ctx, fonts, n.slots[2], x + base.w, y, s2, hl, slots, p2);
            }
            return {w, h};
        }

        case MathNode::Sum:
        case MathNode::Prod:
        case MathNode::Int: {
            const char* sym = n.type == MathNode::Sum ? "\xE2\x88\x91"      // ∑
                             : n.type == MathNode::Prod ? "\xE2\x88\x8F"    // ∏
                                                        : "\xE2\x88\xAB";   // ∫
            TTF_Font* fsym = face(fonts, size * 1.7f);
            const float sym_w = ui::text_width(fsym, sym);
            const float sym_h = font_h(fsym);
            Size lo{0, 0}, up{0, 0}, body{0, 0};
            if (n.slots.size() >= 3) {
                lo = go(nullptr, fonts, n.slots[0], 0, 0, s2, hl, nullptr, path);
                up = go(nullptr, fonts, n.slots[1], 0, 0, s2, hl, nullptr, path);
                body = go(nullptr, fonts, n.slots[2], 0, 0, size, hl, nullptr, path);
            }
            const float gap = size * 0.15f;
            const float limits_w = std::max(lo.w, up.w);
            const float w = sym_w + limits_w + body.w + gap * 2.0f;
            const float h = std::max(sym_h, up.h + lo.h + gap);
            if (draw) {
                ui::draw_text(*ctx, fsym, sym, x, y + (h - sym_h) / 2.0f, ctx->palette.text);
                if (n.slots.size() >= 3) {
                    std::vector<int> p0 = path; p0.push_back(0);
                    std::vector<int> p1 = path; p1.push_back(1);
                    std::vector<int> p2 = path; p2.push_back(2);
                    go(ctx, fonts, n.slots[1], x + sym_w, y, s2, hl, slots, p1);
                    go(ctx, fonts, n.slots[0], x + sym_w, y + h - lo.h, s2, hl, slots, p0);
                    go(ctx, fonts, n.slots[2], x + sym_w + limits_w + gap,
                       y + (h - body.h) / 2.0f, size, hl, slots, p2);
                }
            }
            return {w, h};
        }

        case MathNode::Parens:
        case MathNode::Brackets:
        case MathNode::Braces:
        case MathNode::Abs: {
            const char* open = n.type == MathNode::Parens ? "("
                             : n.type == MathNode::Brackets ? "["
                             : n.type == MathNode::Braces ? "{" : "|";
            const char* close = n.type == MathNode::Parens ? ")"
                             : n.type == MathNode::Brackets ? "]"
                             : n.type == MathNode::Braces ? "}" : "|";
            TTF_Font* fg = face(fonts, size * 1.35f);
            const float ow = ui::text_width(fg, open);
            const float gh = font_h(fg);
            Size is{0, font_h(face(fonts, size))};
            if (!n.slots.empty()) {
                is = go(nullptr, fonts, n.slots[0], 0, 0, size, hl, nullptr, path);
            }
            const float w = ow * 2.0f + is.w + size * 0.15f;
            const float h = std::max(is.h + size * 0.15f, gh);
            if (draw) {
                ui::draw_text(*ctx, fg, open, x, y + (h - gh) / 2.0f, ctx->palette.text);
                if (!n.slots.empty()) {
                    std::vector<int> p0 = path; p0.push_back(0);
                    go(ctx, fonts, n.slots[0], x + ow, y + (h - is.h) / 2.0f,
                       size, hl, slots, p0);
                }
                ui::draw_text(*ctx, fg, close, x + ow + is.w + size * 0.05f,
                              y + (h - gh) / 2.0f, ctx->palette.text);
            }
            return {w, h};
        }

        case MathNode::Matrix: {
            int rows = 1, cols = 1;
            const size_t xp = n.text.find('x');
            if (xp != std::string::npos) {
                rows = std::max(1, std::atoi(n.text.substr(0, xp).c_str()));
                cols = std::max(1, std::atoi(n.text.substr(xp + 1).c_str()));
            }
            const int cells = rows * cols;
            std::vector<Size> cs(static_cast<size_t>(cells));
            std::vector<float> cw(static_cast<size_t>(cols), 0.0f);
            std::vector<float> rh(static_cast<size_t>(rows), 0.0f);
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    const int i = r * cols + c;
                    if (i < static_cast<int>(n.slots.size())) {
                        cs[static_cast<size_t>(i)] =
                            go(nullptr, fonts, n.slots[static_cast<size_t>(i)],
                               0, 0, size, hl, nullptr, path);
                    } else {
                        cs[static_cast<size_t>(i)] = {size * 0.6f,
                                                      font_h(face(fonts, size)) * 0.8f};
                    }
                    cw[static_cast<size_t>(c)] =
                        std::max(cw[static_cast<size_t>(c)], cs[static_cast<size_t>(i)].w);
                    rh[static_cast<size_t>(r)] =
                        std::max(rh[static_cast<size_t>(r)], cs[static_cast<size_t>(i)].h);
                }
            }
            const float cgap = size * 0.55f;
            const float rgap = size * 0.28f;
            float inner_w = 0, inner_h = 0;
            for (int c = 0; c < cols; ++c) inner_w += cw[static_cast<size_t>(c)];
            inner_w += cgap * static_cast<float>(cols - 1);
            for (int r = 0; r < rows; ++r) inner_h += rh[static_cast<size_t>(r)];
            inner_h += rgap * static_cast<float>(rows - 1);
            TTF_Font* fg = face(fonts, size * 1.4f);
            const float bw = ui::text_width(fg, "[");
            const float gh = font_h(fg);
            const float w = inner_w + bw * 2.0f + size * 0.4f;
            const float h = inner_h + size * 0.25f;
            if (draw) {
                float cy = y + size * 0.12f;
                for (int r = 0; r < rows; ++r) {
                    float cx = x + bw + size * 0.2f;
                    for (int c = 0; c < cols; ++c) {
                        const int i = r * cols + c;
                        if (i < static_cast<int>(n.slots.size())) {
                            std::vector<int> cp = path;
                            cp.push_back(i);
                            go(ctx, fonts, n.slots[static_cast<size_t>(i)],
                               cx, cy + (rh[static_cast<size_t>(r)] - cs[static_cast<size_t>(i)].h) / 2.0f,
                               size, hl, slots, cp);
                        }
                        cx += cw[static_cast<size_t>(c)] + cgap;
                    }
                    cy += rh[static_cast<size_t>(r)] + rgap;
                }
                ui::draw_text(*ctx, fg, "[", x, y + (h - gh) / 2.0f, ctx->palette.text);
                ui::draw_text(*ctx, fg, "]", x + w - bw, y + (h - gh) / 2.0f, ctx->palette.text);
            }
            return {w, h};
        }

        case MathNode::Piecewise: {
            TTF_Font* fg = face(fonts, size * 1.4f);
            const float bw = ui::text_width(fg, "{");
            const float gh = font_h(fg);
            Size is{0, font_h(face(fonts, size))};
            if (!n.slots.empty()) {
                is = go(nullptr, fonts, n.slots[0], 0, 0, size, hl, nullptr, path);
            }
            const float w = bw + is.w + size * 0.2f;
            const float h = std::max(is.h + size * 0.15f, gh);
            if (draw) {
                ui::draw_text(*ctx, fg, "{", x, y + (h - gh) / 2.0f, ctx->palette.text);
                if (!n.slots.empty()) {
                    std::vector<int> p0 = path; p0.push_back(0);
                    go(ctx, fonts, n.slots[0], x + bw, y + (h - is.h) / 2.0f,
                       size, hl, slots, p0);
                }
            }
            return {w, h};
        }
    }
    return {0, 0};
}

} // namespace

Size render(ui::Context* ctx, FontBook& fonts, const MathNode& node,
            float x, float y, float size,
            const std::vector<int>* highlight,
            std::vector<Slot>* slots,
            const std::vector<int>& path) {
    return go(ctx, fonts, node, x, y, size, highlight, slots, path);
}

} // namespace mathdraw
} // namespace md

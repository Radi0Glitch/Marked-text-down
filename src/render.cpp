#include "render.hpp"

#include <algorithm>
#include <cmath>

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"

namespace md {

namespace {

const SDL_Color kCodeBg{35, 39, 48, 255};
const SDL_Color kCodeText{226, 192, 141, 255};
const SDL_Color kQuoteBar{90, 98, 116, 255};
const SDL_Color kSelection{64, 110, 180, 110};
const SDL_Color kTableHead{40, 45, 56, 255};

float span_prefix_width(TTF_Font* font, const std::string& text, int begin, int end) {
    // NB: TTF_GetStringSize treats length 0 as "the whole C string".
    if (end <= begin) return 0.0f;
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str() + begin, static_cast<size_t>(end - begin), &w, &h);
    return static_cast<float>(w) + 2.0f;
}

void draw_span_text(ui::Context& ctx, TTF_Font* font, const std::string& text,
                    int begin, int end, float x, float y, SDL_Color color) {
    if (end <= begin) return;
    const std::string piece = text.substr(begin, end - begin);
    SDL_Surface* surface = TTF_RenderText_Blended(font, piece.c_str(), piece.size(), color);
    if (!surface) return;
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

} // namespace

void render_document(ui::Context& ctx, const Document& doc, const Layout& layout,
                     FontBook& fonts, ui::Rect viewport, float scroll_y,
                     const EditView* edit) {
    const bool light = ui::is_light_palette(ctx.palette);
    const SDL_Color code_bg = light ? SDL_Color{238, 241, 246, 255} : kCodeBg;
    const SDL_Color code_text = light ? SDL_Color{154, 52, 18, 255} : kCodeText;
    const SDL_Color quote_bar = light ? SDL_Color{180, 190, 206, 255} : kQuoteBar;
    const SDL_Color selection_col = light ? SDL_Color{59, 130, 246, 75} : kSelection;
    const SDL_Color table_head = light ? SDL_Color{232, 236, 244, 255} : kTableHead;
    const SDL_Color formula_bg = light ? SDL_Color{243, 246, 251, 255} : SDL_Color{28, 30, 38, 255};
    const SDL_Color diag_bg = light ? SDL_Color{238, 243, 252, 255} : SDL_Color{42, 48, 66, 255};
    const SDL_Color inline_math_bg = light ? SDL_Color{219, 234, 254, 180} : SDL_Color{48, 56, 78, 150};
    const SDL_Color inline_math_fg = light ? SDL_Color{30, 64, 175, 255} : SDL_Color{210, 225, 255, 255};

    const SDL_Rect clip{static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                        static_cast<int>(viewport.w), static_cast<int>(viewport.h)};
    SDL_SetRenderClipRect(ctx.renderer, &clip);

    const float origin_x = viewport.x;
    const float origin_y = viewport.y - scroll_y;

    // Block backgrounds first, then text on top.
    for (size_t bi = 0; bi < doc.blocks.size() && bi < layout.boxes.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        const BlockBox& box = layout.boxes[bi];
        const float by = origin_y + box.y;
        if (by + box.h < viewport.y || by > viewport.y + viewport.h) continue;

        if (block.type == BlockType::CodeBlock) {
            const auto is_special = [&]() {
                const std::string& l = block.lang;
                return l == "math" || l == "latex" || l == "tex" || l == "mermaid"
                    || l == "plantuml" || l == "puml" || l == "dot" || l == "graphviz";
            };
            const auto is_formula = [&]() {
                const std::string& l = block.lang;
                return l == "math" || l == "latex" || l == "tex";
            };
            const bool is_diagram = is_diagram_lang(block.lang) && !is_formula();
            std::string draw_code;
            if (is_diagram) {
                for (const Inline& inl : block.inlines) draw_code += inl.text;
            }
            const bool is_chart = is_diagram && md::is_chart_text(draw_code);
            SDL_Color bg = code_bg;
            if (is_special()) {
                bg = is_formula() ? formula_bg : diag_bg;
                if (!is_formula()) {
                    ui::fill_rect(ctx, {origin_x + box.x + 2.0f, by + 6.0f, 3.0f, box.h - 12.0f},
                                  ctx.palette.accent);
                }
            }
            ui::fill_rounded_rect(ctx, {origin_x + box.x, by, box.w, box.h}, 8.0f, bg);
            if (is_formula()) {
                // The formula is drawn as a native vector tree; the LaTeX
                // source stays invisible until the user opens the editor.
                std::string tex;
                for (const Inline& inl : block.inlines) tex += inl.text;
                const MathNode node = math_from_latex(tex);
                mathdraw::render(&ctx, fonts, node,
                                 origin_x + box.x + 14.0f, by + 10.0f, 18.0f,
                                 nullptr, nullptr);
            } else if (is_diagram && is_chart) {
                chartdraw::render_block(&ctx, fonts, draw_code,
                                        origin_x + box.x + 14.0f, by + 10.0f,
                                        box.w - 28.0f);
            } else if (is_diagram) {
                // The diagram is drawn as a native picture; its code stays
                // invisible until the user opens the editor.
                diagdraw::render_block(&ctx, fonts, draw_code,
                                       origin_x + box.x + 14.0f, by + 10.0f,
                                       box.w - 28.0f);
            }
            if (!block.lang.empty() && !is_formula() && !is_diagram) {
                std::string label = "[" + block.lang + "]";
                TTF_Font* f = ctx.fonts->regular;
                ui::draw_text(ctx, f, label,
                              origin_x + box.x + box.w - ui::text_width(f, label) - 10.0f,
                              by + 4.0f, ctx.palette.accent);
            }
        } else if (block.type == BlockType::Quote) {
            ui::fill_rounded_rect(ctx, {origin_x + 2.0f, by, 4.0f, box.h}, 2.0f, quote_bar);
        } else if (block.type == BlockType::Rule) {
            ui::fill_rect(ctx, {origin_x, by + box.h / 2.0f - 0.5f, box.w, 1.0f},
                          ctx.palette.panel_border);
        }
    }

    // Table grids: shaded header row, 1px lines between cells.
    for (const TableGrid& g : layout.tables) {
        if (g.block < 0 || static_cast<size_t>(g.block) >= layout.boxes.size()
            || static_cast<size_t>(g.block) >= doc.blocks.size()) continue;
        const BlockBox& box = layout.boxes[g.block];
        const float by = origin_y + box.y;
        if (by + box.h < viewport.y || by > viewport.y + viewport.h) continue;

        const float left = origin_x + g.xs.front();
        const float right = origin_x + g.xs.back();
        const float top = origin_y + g.ys.front();
        const float bottom = origin_y + g.ys.back();
        ui::fill_rect(ctx, {left, top, right - left, g.ys[1] - g.ys[0]}, table_head);
        for (float x : g.xs) {
            ui::fill_rect(ctx, {origin_x + x, top, 1.0f, bottom - top}, ctx.palette.panel_border);
        }
        for (float y : g.ys) {
            ui::fill_rect(ctx, {left, origin_y + y, right - left, 1.0f}, ctx.palette.panel_border);
        }
    }

    if (edit && edit->selecting) {
        for (const Line& line : layout.lines) {
            const float ly = origin_y + line.y;
            if (ly + line.height < viewport.y || ly > viewport.y + viewport.h) continue;
            const int b = line.block;
            if (b < 0 || static_cast<size_t>(b) >= doc.blocks.size()) continue;
            if (b < edit->sel_start.block || b > edit->sel_end.block) continue;
            // A selection never spans two cells, so only its own cell is highlighted.
            if (b == edit->sel_start.block && line.cell != edit->sel_start.cell) continue;
            if (b == edit->sel_end.block && line.cell != edit->sel_end.cell) continue;

            if (line.spans.empty()) {
                ui::fill_rect(ctx, {origin_x + line.x0, ly, 6.0f, line.height}, selection_col);
                continue;
            }
            const Block& block = doc.blocks[b];
            if (line.cell >= 0 && static_cast<size_t>(line.cell) >= block.cells.size()) continue;
            const std::vector<Inline>& inls = inlines_of(block, line.cell);
            for (const Span& span : line.spans) {
                if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
                int sb = span.byte_begin, se = span.byte_end;
                if (b == edit->sel_start.block) {
                    if (span.inl < edit->sel_start.inl) continue;
                    if (span.inl == edit->sel_start.inl) sb = std::max(sb, edit->sel_start.offset);
                }
                if (b == edit->sel_end.block) {
                    if (span.inl > edit->sel_end.inl) continue;
                    if (span.inl == edit->sel_end.inl) se = std::min(se, edit->sel_end.offset);
                }
                if (se <= sb) continue;

                const Inline& inl = inls[span.inl];
                TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
                const float w0 = span_prefix_width(font, inl.text, span.byte_begin, sb);
                const float w1 = span_prefix_width(font, inl.text, span.byte_begin, se);
                ui::fill_rect(ctx, {origin_x + span.x + w0, ly, w1 - w0, line.height},
                              selection_col);
            }
        }
    }

    int marked_block = -1;
    for (const Line& line : layout.lines) {
        const float ly = origin_y + line.y;
        if (ly + line.height < viewport.y || ly > viewport.y + viewport.h) continue;
        if (line.block < 0 || static_cast<size_t>(line.block) >= doc.blocks.size()) continue;

        const Block& block = doc.blocks[line.block];
        if (line.cell >= 0 && static_cast<size_t>(line.cell) >= block.cells.size()) continue;
        if (line.block != marked_block) {
            marked_block = line.block;
            if (static_cast<size_t>(line.block) < layout.markers.size()) {
                const std::string& marker = layout.markers[line.block];
                if (!marker.empty()) {
                    const std::string& bl = block.lang;
                    if (block.type == BlockType::CodeBlock && (bl == "math" || bl == "latex" || bl == "tex")) {
                        // skip marker for visual formulas
                    } else {
                        TTF_Font* mf = font_for(fonts, block, 0);
                        const int mlen = static_cast<int>(marker.size());
                        const float mw = span_prefix_width(mf, marker, 0, mlen);
                        const float base = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(mf));
                        draw_span_text(ctx, mf, marker, 0, mlen,
                                       origin_x + line.x0 - 10.0f - mw, base, ctx.palette.text_dim);
                    }
                }
            }
        }

        const std::vector<Inline>& inls = inlines_of(block, line.cell);
        if (block.type == BlockType::CodeBlock) {
            const std::string& bl = block.lang;
            if (bl == "math" || bl == "latex" || bl == "tex" || is_diagram_lang(bl)) {
                continue;
            }
        }
        for (const Span& span : line.spans) {
            if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
            const Inline& inl = inls[span.inl];
            TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
            const float sx = origin_x + span.x;

            const bool inline_code = (inl.style & StyleCode)
                && block.type != BlockType::CodeBlock;
            if (inline_code) {
                ui::fill_rounded_rect(ctx,
                    {sx - 3.0f, ly + 2.0f, span.width + 6.0f, line.height - 4.0f},
                    4.0f, code_bg);
            }
            // Highlight inline math $...$
            if (!inline_code && block.type != BlockType::CodeBlock) {
                const std::string& t = inl.text;
                if (t.find('$') != std::string::npos) {
                    ui::fill_rounded_rect(ctx,
                        {sx - 3.0f, ly + 2.0f, span.width + 6.0f, line.height - 4.0f},
                        4.0f, inline_math_bg);
                }
            }

            SDL_Color color = ctx.palette.text;
            if (block.type == BlockType::Quote) color = ctx.palette.text_dim;
            if (inl.style & StyleLink) color = ctx.palette.accent;
            if (inl.style & StyleCode) color = code_text;
            if (!(inl.style & StyleCode) && block.type != BlockType::CodeBlock
                && inl.text.find('$') != std::string::npos) {
                color = inline_math_fg;
            }

            const float ty = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(font));
            draw_span_text(ctx, font, inl.text, span.byte_begin, span.byte_end,
                           sx, ty, color);
        }
    }

    SDL_SetRenderClipRect(ctx.renderer, nullptr);

    if (edit && edit->caret_visible) {
        float cx = 0;
        const int li = locate_caret(doc, layout, fonts, edit->caret, &cx);
        if (li >= 0) {
            const Line& line = layout.lines[li];
            const float ly = origin_y + line.y;
            if (ly + line.height >= viewport.y && ly <= viewport.y + viewport.h) {
                ui::fill_rect(ctx, {origin_x + cx, ly + 2.0f, 1.5f, line.height - 4.0f},
                              ctx.palette.accent);
            }
        }
    }
}

} // namespace md

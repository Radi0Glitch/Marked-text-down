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

const SDL_Color kCodeBg{44, 39, 33, 255};
const SDL_Color kCodeText{226, 192, 141, 255};
const SDL_Color kQuoteBar{106, 96, 84, 255};
const SDL_Color kSelection{140, 70, 50, 110};
const SDL_Color kTableHead{50, 44, 38, 255};

float span_prefix_width(TTF_Font* font, const std::string& text, int begin, int end) {
    // NB: TTF_GetStringSize treats length 0 as "the whole C string".
    if (end <= begin) return 0.0f;
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str() + begin, static_cast<size_t>(end - begin), &w, &h);
    return static_cast<float>(w) + 2.0f;
}

// A small page with a folded corner and the file type, drawn in front of a file link.
void draw_file_icon(ui::Context& ctx, FontBook& fonts, const std::string& href, float x, float y) {
    const float zoom = fonts.zoom();
    const float w = 16.0f * zoom;
    const float h = 20.0f * zoom;
    const ui::Rect page{x, y, w, h};
    ui::fill_rounded_rect(ctx, page, 2.0f * zoom, ctx.palette.panel_bg);
    ui::stroke_rect(ctx, page, ctx.palette.panel_border);
    ui::fill_rect(ctx, {x + w - 5.0f * zoom, y, 5.0f * zoom, 5.0f * zoom}, ctx.palette.panel_border);

    std::string ext = "FILE";
    const size_t dot = href.find_last_of('.');
    const size_t slash = href.find_last_of("/\\");
    const bool has_ext = dot != std::string::npos && dot + 1 < href.size() && href.size() - dot - 1 <= 4
        && (slash == std::string::npos || dot > slash);
    if (has_ext) {
        ext = href.substr(dot + 1);
        for (char& c : ext) {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        }
    }
    TTF_Font* font = fonts.get(md::Family::Sans, true, false, 6.0f);
    const float tw = ui::text_width(font, ext);
    ui::draw_text(ctx, font, ext, x + (w - tw) / 2.0f,
                  y + h - ui::text_height(font) - 2.0f * zoom, ctx.palette.accent);
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

std::string margin_marker(const Block& block) {
    switch (block.type) {
        case BlockType::Heading: return std::string(static_cast<size_t>(std::clamp(block.level, 1, 6)), '#');
        case BlockType::CodeBlock: return block.lang.empty() ? "```" : "```" + block.lang;
        case BlockType::Table: return "| - |";
        case BlockType::Quote: return ">";
        case BlockType::ListItem: return block.ordered ? "1." : "-";
        case BlockType::Rule: return "---";
        default: return "";
    }
}

// Soft drop shadow: translucent rounded rectangles growing out of the card and
// shifted a little downwards, so the card reads as raised above the page.
void card_shadow(ui::Context& ctx, ui::Rect r, float radius, Uint8 strength) {
    // Symmetric soft shadow: nested rounded rectangles that grow out of the card on
    // every side. Layers overlap, so the shade is strongest next to the edge and
    // fades outwards; the whole shape is nudged down by a couple of pixels.
    constexpr int kLayers = 14;
    const Uint8 alpha = static_cast<Uint8>(std::max(1, strength / 2));
    for (int i = 0; i < kLayers; ++i) {
        const float grow = static_cast<float>(i) * 1.2f;
        const ui::Rect s{r.x - grow, r.y + 2.0f - grow, r.w + grow * 2.0f, r.h + grow * 2.0f};
        ui::fill_rounded_rect(ctx, s, radius + grow, SDL_Color{0, 0, 0, alpha});
    }
}

} // namespace

void render_document(ui::Context& ctx, const Document& doc, const Layout& layout,
                     FontBook& fonts, ui::Rect viewport, float scroll_y,
                     const EditView* edit) {
    const bool light = ui::is_light_palette(ctx.palette);
    const SDL_Color code_bg = light ? SDL_Color{236, 230, 216, 255} : kCodeBg;
    const SDL_Color code_text = light ? SDL_Color{160, 60, 34, 255} : kCodeText;
    const SDL_Color quote_bar = light ? SDL_Color{200, 186, 166, 255} : kQuoteBar;
    const SDL_Color selection_col = light ? SDL_Color{192, 67, 38, 60} : kSelection;
    const SDL_Color table_head = light ? SDL_Color{236, 230, 216, 255} : kTableHead;
    const SDL_Color formula_bg = light ? SDL_Color{255, 255, 255, 255} : SDL_Color{36, 32, 28, 255};
    const SDL_Color diag_bg = light ? SDL_Color{255, 255, 255, 255} : SDL_Color{48, 42, 36, 255};
    const SDL_Color code_card = light ? SDL_Color{38, 36, 31, 255} : SDL_Color{44, 39, 33, 255};
    const SDL_Color card_border = light ? SDL_Color{150, 46, 24, 255} : SDL_Color{120, 44, 30, 255};
    const SDL_Color table_body = light ? SDL_Color{255, 255, 255, 255} : SDL_Color{42, 38, 33, 255};
    const SDL_Color inline_math_bg = light ? SDL_Color{236, 226, 210, 200} : SDL_Color{64, 50, 40, 150};

    // Shadows spread a little beyond the column, so the clip is widened sideways.
    const SDL_Rect clip{static_cast<int>(viewport.x) - 24, static_cast<int>(viewport.y),
                        static_cast<int>(viewport.w) + 48, static_cast<int>(viewport.h)};
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
            SDL_Color bg = is_special() ? code_bg : code_card;
            if (is_special()) {
                bg = is_formula() ? formula_bg : diag_bg;
                if (!is_formula()) {
                    ui::fill_rect(ctx, {origin_x + box.x + 2.0f, by + 6.0f, 3.0f, box.h - 12.0f},
                                  ctx.palette.accent);
                }
            }
            const ui::Rect card{origin_x + box.x, by, box.w, box.h};
            if (is_special()) {
                card_shadow(ctx, card, 12.0f, 4);
                ui::fill_rounded_rect(ctx, card, 12.0f, card_border);
                ui::fill_rounded_rect(ctx, {card.x + 1.0f, card.y + 1.0f, card.w - 2.0f, card.h - 2.0f},
                                      11.0f, bg);
            } else {
                card_shadow(ctx, card, 12.0f, 8);
                ui::fill_rounded_rect(ctx, card, 12.0f, bg);
            }
            if (is_formula()) {
                // The formula is drawn as a native vector tree; the LaTeX
                // source stays invisible until the user opens the editor.
                std::string tex;
                for (const Inline& inl : block.inlines) tex += inl.text;
                const MathNode node = math_from_latex(tex);
                mathdraw::render(&ctx, fonts, node,
                                 origin_x + box.x + 14.0f, by + 10.0f,
                                 18.0f * fonts.zoom(),
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
            if (!is_formula() && !is_diagram) {
                const float header_h = 40.0f;
                for (int i = 0; i < 3; ++i) {
                    ui::fill_rounded_rect(ctx, {origin_x + box.x + box.w - 22.0f - i * 14.0f, by + 15.0f, 9.0f, 9.0f},
                                          4.5f, SDL_Color{74, 71, 61, 255});
                }
                ui::fill_rect(ctx, {origin_x + box.x, by + header_h, box.w, 1.0f}, SDL_Color{58, 56, 47, 255});
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
        const ui::Rect card{left, top, right - left, bottom - top};
        card_shadow(ctx, card, 10.0f, 4);
        ui::fill_rounded_rect(ctx, card, 10.0f, table_body);
        const float head_h = g.ys[1] - g.ys[0];
        ui::fill_rounded_rect(ctx, {left, top, right - left, head_h}, 10.0f, table_head);
        ui::fill_rect(ctx, {left, top + head_h - 10.0f, right - left, 10.0f}, table_head);
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
            if (span.icon) {
                const float ih = 20.0f * fonts.zoom();
                draw_file_icon(ctx, fonts, inl.href, sx, ly + (line.height - ih) / 2.0f);
                continue;
            }
            if (span.math) {
                // An inline formula is drawn as a formula; its source shows only while revealed.
                const float fsize = math_size(block, fonts.zoom());
                const std::string tex = inl.text.substr(static_cast<size_t>(span.byte_begin) + 1,
                    static_cast<size_t>(span.byte_end - span.byte_begin - 2));
                const auto msz = mathdraw::render(nullptr, fonts, math_from_latex(tex), 0, 0, fsize,
                                                  nullptr, nullptr);
                ui::fill_rounded_rect(ctx, {sx, ly + 2.0f, span.width, line.height - 4.0f}, 4.0f,
                                      inline_math_bg);
                mathdraw::render(&ctx, fonts, math_from_latex(tex), sx + 2.0f,
                                 ly + line.baseline - msz.h * 0.8f, fsize, nullptr, nullptr);
                continue;
            }

            const bool inline_code = (inl.style & StyleCode)
                && block.type != BlockType::CodeBlock;
            if (inline_code) {
                ui::fill_rounded_rect(ctx,
                    {sx - 3.0f, ly + 2.0f, span.width + 6.0f, line.height - 4.0f},
                    4.0f, code_bg);
            }

            SDL_Color color = ctx.palette.text;
            if (block.type == BlockType::CodeBlock) color = SDL_Color{216, 212, 200, 255};
            if (block.type == BlockType::Quote) color = ctx.palette.text_dim;
            if (inl.style & StyleLink) color = ctx.palette.link;
            if (inl.style & StyleCode) color = code_text;

            const float ty = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(font));
            draw_span_text(ctx, font, inl.text, span.byte_begin, span.byte_end,
                           sx, ty, color);
            if (inl.style & StyleLink) {
                ui::fill_rect(ctx, {sx, ly + line.baseline + 3.0f, span.width - 2.0f, 2.0f}, color);
            }
        }
    }

    // Second-level headings get a hairline that continues to the column edge.
    for (size_t li = 0; li < layout.lines.size(); ++li) {
        const Line& line = layout.lines[li];
        if (line.block < 0 || static_cast<size_t>(line.block) >= doc.blocks.size() || line.spans.empty()) continue;
        const Block& block = doc.blocks[static_cast<size_t>(line.block)];
        if (block.type != BlockType::Heading || block.level != 2) continue;
        const bool last = li + 1 >= layout.lines.size() || layout.lines[li + 1].block != line.block;
        if (!last) continue;
        const Span& end = line.spans.back();
        const float ry = origin_y + line.y + line.height / 2.0f;
        const float rx = origin_x + end.x + end.width + 14.0f;
        const float rx1 = viewport.x + viewport.w;
        if (rx1 > rx) ui::fill_rect(ctx, {rx, ry, rx1 - rx, 1.0f}, ctx.palette.panel_border);
    }

    SDL_SetRenderClipRect(ctx.renderer, nullptr);

    // Margin markers sit left of the column, so they are drawn outside the clip.
    for (size_t bi = 0; bi < doc.blocks.size() && bi < layout.boxes.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        const std::string marker = margin_marker(block);
        const BlockBox& box = layout.boxes[bi];
        const float by = origin_y + box.y;
        if (marker.empty() || by + box.h < viewport.y || by > viewport.y + viewport.h) continue;
        TTF_Font* mf = fonts.get(Family::Mono, false, false, 13.0f);
        const int mlen = static_cast<int>(marker.size());
        const float mw = span_prefix_width(mf, marker, 0, mlen);
        // A block scrolled under the toolbar keeps its marker on the visible edge.
        const float my = std::max(by + 2.0f, viewport.y + 4.0f);
        draw_span_text(ctx, mf, marker, 0, mlen, origin_x - mw - 14.0f, my, ctx.palette.text_dim);
    }

    if (edit && edit->caret_visible) {
        float cx = 0;
        const int li = locate_caret(doc, layout, fonts, edit->caret, &cx);
        if (li >= 0) {
            const Line& line = layout.lines[li];
            const float ly = origin_y + line.y;
            if (ly + line.height >= viewport.y && ly <= viewport.y + viewport.h) {
                ui::fill_rect(ctx, {origin_x + cx, ly + 2.0f, 2.0f, line.height - 4.0f},
                              ctx.palette.accent);
            }
        }
    }
}

} // namespace md

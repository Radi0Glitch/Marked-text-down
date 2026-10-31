#include "layout.hpp"

#include <algorithm>
#include <cmath>

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "diag_model.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"
#include "text.hpp"

namespace md {

namespace {

constexpr float kCellPadX = 8.0f;
constexpr float kCellPadY = 5.0f;
constexpr float kMinColumn = 56.0f;

float size_for(const Block& block) {
    switch (block.type) {
        case BlockType::Heading: {
            static constexpr float sizes[6] = {30, 25, 21, 18, 17, 17};
            const int level = std::clamp(block.level, 1, 6);
            return sizes[level - 1];
        }
        case BlockType::CodeBlock:
            return 15.0f;
        default:
            return 17.0f;
    }
}

float measure(TTF_Font* font, const std::string& text, int begin, int end) {
    // NB: TTF_GetStringSize treats length 0 as "the whole C string".
    if (end <= begin) return 0.0f;
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str() + begin, static_cast<size_t>(end - begin), &w, &h);
    return static_cast<float>(w) + 2.0f;
}

struct Atom {
    int inl = 0;
    int begin = 0;
    int end = 0;
    bool space = false;
    float width = 0;
    TTF_Font* font = nullptr;
};

struct BlockMetrics {
    float indent = 0;      // text indent inside the content column
    float pad_before = 0;
    float pad_after = 0;
    float leading = 6.0f;  // extra space between lines
    bool preformatted = false;
};

BlockMetrics metrics_for(const Block& block, bool first) {
    BlockMetrics m;
    switch (block.type) {
        case BlockType::Heading:
            m.pad_before = first ? 4.0f : 22.0f;
            m.pad_after = 10.0f;
            break;
        case BlockType::Quote:
            m.indent = 18.0f;
            m.pad_before = 6.0f;
            m.pad_after = 14.0f;
            break;
        case BlockType::CodeBlock:
            m.indent = 12.0f;
            m.pad_before = 6.0f;
            m.pad_after = 16.0f;
            m.leading = 2.0f;
            m.preformatted = true;
            break;
        case BlockType::ListItem:
            m.indent = 26.0f * static_cast<float>(block.depth + 1);
            m.pad_after = 6.0f;
            break;
        case BlockType::Rule:
            m.pad_before = 12.0f;
            m.pad_after = 14.0f;
            break;
        case BlockType::Table:
            m.pad_before = 6.0f;
            m.pad_after = 14.0f;
            break;
        default:
            m.pad_after = 14.0f;
            break;
    }
    return m;
}

// Accumulates lines for one text container.
class LineBuilder {
public:
    LineBuilder(int block_index, int cell, std::uint8_t extra_style,
                const BlockMetrics& metrics, float width)
        : block_index_(block_index), cell_(cell), extra_(extra_style),
          metrics_(metrics), width_(width) {
        new_line();
    }

    void add(const Atom& atom) {
        update_metrics(atom.font);
        line_.spans.push_back(Span{atom.inl, atom.begin, atom.end, cursor_, atom.width});
        cursor_ += atom.width;
    }

    bool fits(const Atom& atom) const {
        return cursor_ + atom.width <= width_;
    }

    bool empty() const { return line_.spans.empty(); }

    void set_default_font(TTF_Font* font) {
        default_height_ = static_cast<float>(TTF_GetFontHeight(font));
        default_ascent_ = static_cast<float>(TTF_GetFontAscent(font));
    }

    void finish_line(Layout& out, float& y) {
        line_.height = max_height_ > 0 ? max_height_ : default_height_;
        line_.baseline = max_ascent_ > 0 ? max_ascent_ : default_ascent_;
        line_.y = y;
        out.lines.push_back(line_);
        y += line_.height + metrics_.leading;
        max_height_ = 0;
        max_ascent_ = 0;
        new_line();
    }

private:
    void new_line() {
        line_ = Line{};
        line_.block = block_index_;
        line_.cell = cell_;
        line_.extra_style = extra_;
        line_.x0 = metrics_.indent;
        cursor_ = metrics_.indent;
    }

    void update_metrics(TTF_Font* font) {
        if (!font) return;
        max_height_ = std::max(max_height_, static_cast<float>(TTF_GetFontHeight(font)));
        max_ascent_ = std::max(max_ascent_, static_cast<float>(TTF_GetFontAscent(font)));
    }

    int block_index_;
    int cell_;
    std::uint8_t extra_;
    const BlockMetrics& metrics_;
    float width_;
    Line line_;
    float cursor_ = 0;
    float max_height_ = 0;
    float max_ascent_ = 0;
    float default_height_ = 0;
    float default_ascent_ = 0;
};

// Lays out the inlines of one text container (a block or a table cell) from `y`
// downwards. `width` is the right edge of the usable column.
void layout_text(const Block& block, const std::vector<Inline>& inlines, int block_index,
                 int cell, std::uint8_t extra_style, FontBook& fonts,
                 const BlockMetrics& metrics, float width, float& y, Layout& out) {
    LineBuilder builder(block_index, cell, extra_style, metrics, width);
    builder.set_default_font(font_for(fonts, block, extra_style));
    const float avail = width - metrics.indent;

    // Tokenize into words and space runs.
    std::vector<Atom> atoms;
    for (size_t ii = 0; ii < inlines.size(); ++ii) {
        const Inline& inl = inlines[ii];
        TTF_Font* font = font_for(fonts, block, inl.style | extra_style);
        const std::string& text = inl.text;
        int pos = 0;
        while (pos < static_cast<int>(text.size())) {
            const bool space = text[pos] == ' ';
            int end = pos;
            while (end < static_cast<int>(text.size())
                   && (text[end] == ' ') == space) {
                end = next_char(text, end);
            }
            atoms.push_back(Atom{static_cast<int>(ii), pos, end, space,
                                 measure(font, text, pos, end), font});
            pos = end;
        }
    }

    if (atoms.empty()) {
        builder.finish_line(out, y); // keep empty blocks and cells visible
    }

    for (const Atom& atom : atoms) {
        if (builder.empty() && atom.space) {
            continue; // no leading spaces after a wrap
        }
        if (!builder.fits(atom) && !builder.empty()) {
            builder.finish_line(out, y);
            if (atom.space) continue;
        }
        if (!builder.fits(atom) && builder.empty() && !atom.space) {
            // Single word wider than the column: split by UTF-8 chars.
            const std::string& text = inlines[atom.inl].text;
            int start = atom.begin;
            while (start < atom.end) {
                int best = -1;
                int pos = start;
                while (pos < atom.end) {
                    const int next = next_char(text, pos);
                    const float w = measure(atom.font, text, start, next);
                    if (w > avail) break;
                    best = next;
                    pos = next;
                }
                if (best <= start) best = next_char(text, start); // always progress
                Atom piece{atom.inl, start, best, false,
                           measure(atom.font, text, start, best), atom.font};
                builder.add(piece);
                builder.finish_line(out, y);
                start = best;
            }
            continue;
        }
        builder.add(atom);
    }
    if (!builder.empty()) {
        builder.finish_line(out, y);
    }
}

float cell_text_width(FontBook& fonts, const Block& block, const std::vector<Inline>& cell,
                      std::uint8_t extra_style) {
    float w = 0;
    for (const Inline& inl : cell) {
        TTF_Font* font = font_for(fonts, block, inl.style | extra_style);
        w += measure(font, inl.text, 0, static_cast<int>(inl.text.size()));
    }
    return w;
}

// Shifts the lines laid out since `from` inside [left, right] so the text is
// centred or right-aligned. Left alignment needs no shift.
void align_lines(Layout& out, size_t from, TableAlign align, float left, float right) {
    if (align == TableAlign::None || align == TableAlign::Left) return;
    for (size_t i = from; i < out.lines.size(); ++i) {
        Line& line = out.lines[i];
        float used = 0;
        if (!line.spans.empty()) {
            const Span& last = line.spans.back();
            used = last.x + last.width - line.x0;
        }
        const float free = (right - left) - used;
        if (free <= 0) continue;
        const float shift = align == TableAlign::Center ? free / 2.0f : free;
        for (Span& span : line.spans) span.x += shift;
        line.x0 += shift;
    }
}

// Columns share the content width in proportion to their natural width. Rows
// grow to fit wrapped text. The header row is bold.
void layout_table(const Block& block, int bi, FontBook& fonts, float width, float& y,
                  Layout& out) {
    const int cols = table_cols(block);
    const int rows = table_rows(block);
    if (cols == 0 || rows == 0) {
        out.boxes.push_back(BlockBox{0, y, width, 0});
        return;
    }

    const float top = y;
    const float z = fonts.zoom();
    std::vector<float> natural(static_cast<size_t>(cols), kMinColumn);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const std::uint8_t extra = r == 0 ? StyleBold : 0;
            const float w = cell_text_width(fonts, block, block.cells[r * cols + c], extra)
                + 2.0f * kCellPadX * z;
            natural[c] = std::max(natural[c], w);
        }
    }
    float total = 0;
    for (float w : natural) total += w;
    const float scale = total > 0 ? width / total : 1.0f;

    TableGrid grid;
    grid.block = bi;
    grid.cols = cols;
    grid.xs.push_back(0);
    for (int c = 0; c < cols; ++c) {
        grid.xs.push_back(grid.xs.back() + std::max(kMinColumn, natural[c] * scale));
    }

    grid.ys.push_back(y);
    for (int r = 0; r < rows; ++r) {
        float row_bottom = y;
        for (int c = 0; c < cols; ++c) {
            const int index = r * cols + c;
            BlockMetrics cell_metrics;
            cell_metrics.indent = grid.xs[c] + kCellPadX * z;
            float cy = y + kCellPadY * z;
            const size_t first_line = out.lines.size();
            layout_text(block, block.cells[index], bi, index, r == 0 ? StyleBold : 0, fonts,
                        cell_metrics, grid.xs[c + 1] - kCellPadX * z, cy, out);
            align_lines(out, first_line, block.aligns[c], cell_metrics.indent,
                        grid.xs[c + 1] - kCellPadX * z);
            row_bottom = std::max(row_bottom, cy - cell_metrics.leading);
        }
        y = row_bottom + kCellPadY * z;
        grid.ys.push_back(y);
    }

    out.tables.push_back(grid);
    out.boxes.push_back(BlockBox{0, top, width, y - top});
}

// Index of the grid cell (column or row) that contains `v`. `edges` has n + 1 values.
int edge_cell(const std::vector<float>& edges, float v) {
    const int n = static_cast<int>(edges.size()) - 1;
    for (int i = 0; i < n; ++i) {
        if (v < edges[static_cast<size_t>(i) + 1]) return i;
    }
    return std::max(0, n - 1);
}

const TableGrid* grid_for(const Layout& layout, int block) {
    for (const TableGrid& g : layout.tables) {
        if (g.block == block) return &g;
    }
    return nullptr;
}

} // namespace

TTF_Font* font_for(FontBook& fonts, const Block& block, std::uint8_t style) {
    // Code blocks are plain monospace: emphasis inside them is not shown.
    const bool code_block = block.type == BlockType::CodeBlock;
    const bool bold = (!code_block && (style & StyleBold))
        || block.type == BlockType::Heading;
    const bool italic = !code_block && (style & StyleItalic);
    const bool strike = !code_block && (style & StyleStrike);
    const Family family = (style & StyleCode) || block.type == BlockType::CodeBlock
        ? Family::Mono : Family::Sans;
    return fonts.get(family, bold, italic, size_for(block), strike);
}

void layout_document(const Document& doc, FontBook& fonts, float width, Layout& out) {
    out = Layout{};
    float y = 0;
    std::vector<int> numbers(doc.blocks.size(), 0); // ordered list numbers

    for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        BlockMetrics metrics = metrics_for(block, bi == 0);
        const float z = fonts.zoom();
        metrics.pad_before *= z;
        metrics.pad_after *= z;
        metrics.indent *= z;
        metrics.leading *= z;

        // A list ends with a little more air before the next block.
        if (bi > 0 && doc.blocks[bi - 1].type == BlockType::ListItem
            && block.type != BlockType::ListItem) {
            y += 8.0f * z;
        }

        // Markers: "•" for bullets, running numbers for ordered items. A number
        // continues across deeper nested items but resets after a shallower one.
        std::string marker;
        if (block.type == BlockType::ListItem && block.ordered) {
            int n = 1;
            for (size_t j = bi; j-- > 0;) {
                const Block& prev = doc.blocks[j];
                if (prev.type != BlockType::ListItem || prev.depth < block.depth) break;
                if (prev.depth == block.depth) {
                    if (prev.ordered) n = numbers[j] + 1;
                    break;
                }
            }
            numbers[bi] = n;
            marker = std::to_string(n) + ".";
        } else if (block.type == BlockType::ListItem) {
            marker = "\u2022";
        }
        out.markers.push_back(marker);

        // The marker is painted left of the text column (x0 - 10 - marker_width),
        // so two- and three-digit numbers need a wider indent than the default
        // bullet layout, otherwise the leading digits get cut off.
        if (block.type == BlockType::ListItem && !marker.empty()) {
            TTF_Font* mf = font_for(fonts, block, 0);
            const float mw = measure(mf, marker, 0, static_cast<int>(marker.size()));
            const float needed = 10.0f * z + mw + 6.0f * z;
            if (metrics.indent < needed) metrics.indent = needed;
        }

        y += metrics.pad_before;

        if (block.type == BlockType::Table) {
            layout_table(block, static_cast<int>(bi), fonts, width, y, out);
            y += metrics.pad_after;
            continue;
        }

        const float box_top = y;

        if (metrics.preformatted) {
            const std::string& bl = block.lang;
            if (bl == "math" || bl == "latex" || bl == "tex") {
                // A formula block reserves the height of its rendered tree.
                std::string tex;
                for (const Inline& inl : block.inlines) tex += inl.text;
                const MathNode node = math_from_latex(tex);
                const auto sz = mathdraw::render(nullptr, fonts, node, 0, 0, 18.0f * z,
                                                 nullptr, nullptr);
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                Atom a{0, 0, 0, false, 0.0f, font_for(fonts, block, 0)};
                builder.add(a);
                builder.finish_line(out, y);
                y += sz.h;
            } else if (is_diagram_lang(bl)) {
                // A diagram block reserves the height of its drawn picture;
                // the code itself never reaches the screen.
                std::string code;
                for (const Inline& inl : block.inlines) code += inl.text;
                bool is_chart = md::is_chart_text(code);
                md::diagdraw::Size sz{};
                if (is_chart) {
                    const auto csz = chartdraw::render_block(nullptr, fonts, code, 0, 0, width - 28.0f);
                    sz.w = csz.w; sz.h = csz.h;
                } else {
                    sz = diagdraw::render_block(nullptr, fonts, code, 0, 0, width - 28.0f);
                }
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                Atom a{0, 0, 0, false, 0.0f, font_for(fonts, block, 0)};
                builder.add(a);
                builder.finish_line(out, y);
                y += sz.h;
            } else {
                // Code block: one layout line per source line, no wrapping.
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                for (size_t ii = 0; ii < block.inlines.size(); ++ii) {
                    const Inline& inl = block.inlines[ii];
                    TTF_Font* font = font_for(fonts, block, inl.style);
                    const std::string& text = inl.text;
                    int start = 0;
                    for (int pos = 0; pos <= static_cast<int>(text.size()); ++pos) {
                        if (pos == static_cast<int>(text.size()) || text[pos] == '\n') {
                            Atom a{static_cast<int>(ii), start, pos, false,
                                   measure(font, text, start, pos), font};
                            builder.add(a);
                            builder.finish_line(out, y);
                            start = pos + 1;
                        }
                    }
                }
                if (block.inlines.empty()) {
                    builder.finish_line(out, y);
                }
            }
        } else {
            layout_text(block, block.inlines, static_cast<int>(bi), -1, 0, fonts, metrics,
                        width, y, out);
        }

        const float box_bottom = y - metrics.leading;
        y = box_bottom + metrics.pad_after;

        // Background geometry for render: code blocks and quotes get padding.
        const std::string& bl2 = block.lang;
        const bool is_formula_b = block.type == BlockType::CodeBlock && (bl2 == "math" || bl2 == "latex" || bl2 == "tex");
        const bool is_diagram_b = block.type == BlockType::CodeBlock && is_diagram_lang(bl2);
        const float pad = (block.type == BlockType::CodeBlock && !is_formula_b && !is_diagram_b) ? 10.0f
                        : block.type == BlockType::Quote ? 8.0f : 0.0f;
        out.boxes.push_back(BlockBox{0, box_top - pad, width,
                                     box_bottom - box_top + pad * 2.0f});
    }

    out.content_height = y;
}

bool operator==(const Pos& a, const Pos& b) {
    return a.block == b.block && a.cell == b.cell && a.inl == b.inl && a.offset == b.offset;
}

bool operator<(const Pos& a, const Pos& b) {
    if (a.block != b.block) return a.block < b.block;
    if (a.cell != b.cell) return a.cell < b.cell;
    if (a.inl != b.inl) return a.inl < b.inl;
    return a.offset < b.offset;
}

Pos hit_test(const Document& doc, const Layout& layout, FontBook& fonts, float x, float y) {
    if (layout.lines.empty()) return {};

    const Line* line = &layout.lines.back();
    for (const Line& l : layout.lines) {
        if (y <= l.y + l.height) {
            line = &l;
            break;
        }
    }

    // Inside a table, first find the cell under the pointer, then its line.
    if (const TableGrid* grid = grid_for(layout, line->block)) {
        const int cell = edge_cell(grid->ys, y) * grid->cols + edge_cell(grid->xs, x);
        const Line* in_cell = nullptr;
        for (const Line& l : layout.lines) {
            if (l.block != line->block || l.cell != cell) continue;
            in_cell = &l;
            if (y <= l.y + l.height) break;
        }
        if (in_cell) line = in_cell;
    }

    if (line->spans.empty()) return Pos{line->block, 0, 0, line->cell};

    const Block& block = doc.blocks[line->block];
    const std::vector<Inline>& inls = inlines_of(block, line->cell);

    const Span& first = line->spans.front();
    if (x <= first.x) return Pos{line->block, first.inl, first.byte_begin, line->cell};
    const Span& last = line->spans.back();
    if (x >= last.x + last.width) return Pos{line->block, last.inl, last.byte_end, line->cell};

    for (const Span& span : line->spans) {
        if (x >= span.x + span.width) continue;
        const Inline& inl = inls[span.inl];
        TTF_Font* font = font_for(fonts, block, inl.style | line->extra_style);

        // Walk UTF-8 boundaries and pick the closest one by midpoint.
        int prev = span.byte_begin;
        float prev_w = 0.0f;
        while (prev < span.byte_end) {
            const int next = next_char(inl.text, prev);
            const float w = measure(font, inl.text, span.byte_begin, next);
            if (x < span.x + (prev_w + w) / 2.0f) {
                return Pos{line->block, span.inl, prev, line->cell};
            }
            prev = next;
            prev_w = w;
        }
        return Pos{line->block, span.inl, span.byte_end, line->cell};
    }
    return Pos{line->block, last.inl, last.byte_end, line->cell};
}

int locate_caret(const Document& doc, const Layout& layout, FontBook& fonts,
                 Pos pos, float* x_out) {
    if (pos.block < 0 || pos.block >= static_cast<int>(doc.blocks.size())) return -1;
    const Block& block = doc.blocks[pos.block];
    const std::vector<Inline>& inls = inlines_of(block, pos.cell);
    auto mine = [&](const Line& l) {
        return l.block == pos.block && l.cell == pos.cell;
    };

    int first_line = -1, last_line = -1;
    for (size_t i = 0; i < layout.lines.size(); ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        if (first_line < 0) first_line = static_cast<int>(i);
        last_line = static_cast<int>(i);

        for (const Span& span : line.spans) {
            if (span.inl != pos.inl) continue;
            if (pos.offset >= span.byte_begin && pos.offset < span.byte_end) {
                const Inline& inl = inls[span.inl];
                TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
                *x_out = span.x + measure(font, inl.text, span.byte_begin, pos.offset);
                return static_cast<int>(i);
            }
        }
    }
    if (first_line < 0) return -1;

    // Caret exactly at a span end (e.g. end of an inline or of the block).
    for (int i = first_line; i <= last_line; ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        for (const Span& span : line.spans) {
            if (span.inl == pos.inl && pos.offset == span.byte_end) {
                *x_out = span.x + span.width;
                return i;
            }
        }
    }

    // Caret in an empty inline: place it at the start of the next content.
    for (int i = first_line; i <= last_line; ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        for (const Span& span : line.spans) {
            if (span.inl >= pos.inl) {
                *x_out = span.x;
                return i;
            }
        }
    }

    const Line& line = layout.lines[last_line];
    *x_out = line.spans.empty() ? line.x0
        : line.spans.back().x + line.spans.back().width;
    return last_line;
}

} // namespace md

#pragma once

#include <string>
#include <vector>

#include "document.hpp"
#include "fontbook.hpp"

namespace md {

// A piece of one Inline that fits on a single line. Byte offsets refer to
// Inline::text of the block the line belongs to.
struct Span {
    int inl = 0;
    int byte_begin = 0;
    int byte_end = 0;
    float x = 0;
    float width = 0;
};

struct Line {
    int block = 0;
    int cell = -1;               // table cell index, -1 outside tables
    std::uint8_t extra_style = 0; // added to every span's style, e.g. bold headers
    float y = 0;        // top, document coordinates
    float height = 0;
    float baseline = 0; // distance from y to the baseline
    float x0 = 0;       // text indent, also the caret x for empty lines
    std::vector<Span> spans;
};

struct BlockBox {
    float x = 0, y = 0, w = 0, h = 0; // background/border geometry
};

// Grid of one table: column edges (x, relative to the content column) and row
// edges (y, document coordinates). Edges are n + 1 values for n columns or rows.
struct TableGrid {
    int block = 0;
    int cols = 0;
    std::vector<float> xs;
    std::vector<float> ys;
};

struct Layout {
    std::vector<Line> lines;
    std::vector<BlockBox> boxes;      // one per block
    std::vector<std::string> markers; // one per block, list bullet/number or empty
    std::vector<TableGrid> tables;
    float content_height = 0;
};

// Shared by layout and render so both always agree on metrics.
TTF_Font* font_for(FontBook& fonts, const Block& block, std::uint8_t style);

void layout_document(const Document& doc, FontBook& fonts, float width, Layout& out);

// A caret position inside the document model. Offsets are UTF-8 byte offsets.
// `cell` is set only when the block is a table: it selects the cell.
struct Pos {
    int block = 0;
    int inl = 0;
    int offset = 0;
    int cell = -1;
};

bool operator==(const Pos& a, const Pos& b);
bool operator<(const Pos& a, const Pos& b);

// Maps a point in document coordinates to a model position.
Pos hit_test(const Document& doc, const Layout& layout, FontBook& fonts, float x, float y);

// Finds the line and x coordinate of a caret. Returns the line index or -1.
int locate_caret(const Document& doc, const Layout& layout, FontBook& fonts,
                 Pos pos, float* x_out);

} // namespace md

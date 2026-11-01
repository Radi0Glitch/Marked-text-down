#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <vector>

#include "math_model.hpp"

namespace md {

enum Style : std::uint8_t {
    StyleBold = 1 << 0,
    StyleItalic = 1 << 1,
    StyleCode = 1 << 2,
    StyleLink = 1 << 3,
    StyleStrike = 1 << 4,
    StyleImage = 1 << 5, // with StyleLink: an image, its href is the file or address
};

enum class BlockType {
    Paragraph,
    Heading,
    Quote,
    CodeBlock,
    ListItem,
    Rule,  // horizontal rule, no text
    Table, // GFM table, text lives in Block::cells
};

struct Inline {
    std::string text; // UTF-8, no markdown markers
    std::uint8_t style = 0;
    std::string href; // link target, set when style has StyleLink

    Inline() = default;
    Inline(std::string t, std::uint8_t s = 0, std::string h = {})
        : text(std::move(t)), style(s), href(std::move(h)) {}
};

// A link to the web or by mail, as opposed to a file on disk.
inline bool is_web_link(const std::string& href) {
    return href.find("://") != std::string::npos || href.rfind("mailto:", 0) == 0;
}

// Links to files and every image are drawn as file thumbnails instead of text.
inline bool is_file_link(const Inline& inl) {
    if (inl.style & StyleImage) return true;
    return (inl.style & StyleLink) && !inl.href.empty() && !is_web_link(inl.href);
}

enum class TableAlign : std::uint8_t { None, Left, Center, Right };

struct Block {
    Block() = default;
    Block(BlockType t, int lvl, std::vector<Inline> in)
        : type(t), level(lvl), inlines(std::move(in)) {}

    BlockType type = BlockType::Paragraph;
    int level = 0; // heading 1..6, unused otherwise
    std::vector<Inline> inlines;
    int depth = 0;        // list nesting, ListItem only
    bool ordered = false; // numbered list item, ListItem only
    std::string lang;     // fenced code block language (e.g. "mermaid", "latex", "math")

    // Table only. `inlines` holds one empty placeholder so that every block has
    // text. Cells are row-major; row 0 is the header.
    std::vector<std::vector<Inline>> cells;
    std::vector<TableAlign> aligns; // one per column
};

inline int table_cols(const Block& b) { return static_cast<int>(b.aligns.size()); }

inline int table_rows(const Block& b) {
    const int cols = table_cols(b);
    return cols > 0 ? static_cast<int>(b.cells.size()) / cols : 0;
}

// The text container a position points into: the block's inlines, or one table cell.
inline std::vector<Inline>& inlines_of(Block& b, int cell) {
    return cell >= 0 ? b.cells[static_cast<size_t>(cell)] : b.inlines;
}

inline const std::vector<Inline>& inlines_of(const Block& b, int cell) {
    return cell >= 0 ? b.cells[static_cast<size_t>(cell)] : b.inlines;
}

struct Document {
    std::vector<Block> blocks;
};

inline bool operator==(const Inline& a, const Inline& b) {
    return a.text == b.text && a.style == b.style && a.href == b.href;
}

inline bool operator==(const Block& a, const Block& b) {
    return a.type == b.type && a.level == b.level && a.depth == b.depth
        && a.ordered == b.ordered && a.lang == b.lang && a.inlines == b.inlines
        && a.cells == b.cells && a.aligns == b.aligns;
}

inline bool operator==(const Document& a, const Document& b) {
    return a.blocks == b.blocks;
}

// Read-only showcase document for stage 1.
Document make_demo_document();

} // namespace md

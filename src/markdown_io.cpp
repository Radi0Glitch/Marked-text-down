#include "markdown_io.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "md4c.h"

namespace md {

namespace {

// ---------------------------------------------------------------- import

void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// `entity` is the raw text including '&' and ';'. Unknown entities pass through.
std::string decode_entity(const std::string& entity) {
    static const std::pair<const char*, const char*> named[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&apos;", "'"}, {"&nbsp;", "\xC2\xA0"},
    };
    for (const auto& [name, value] : named) {
        if (entity == name) return value;
    }
    if (entity.size() > 3 && entity[1] == '#') {
        const bool hex = entity[2] == 'x' || entity[2] == 'X';
        const char* digits = entity.c_str() + (hex ? 3 : 2);
        char* end = nullptr;
        const unsigned long cp = std::strtoul(digits, &end, hex ? 16 : 10);
        if (end && *end == ';' && cp > 0 && cp <= 0x10FFFF) {
            std::string out;
            append_utf8(out, static_cast<unsigned>(cp));
            return out;
        }
    }
    return entity;
}

bool is_blank(const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\n' && s[i] != '\r') return false;
    }
    return true;
}

std::string attr_string(const MD_ATTRIBUTE& a) {
    return a.text ? std::string(a.text, a.size) : std::string();
}

// The file name at the end of a link target, without its folders.
std::string base_name(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? path : path.substr(cut + 1);
}

struct Importer {
    Document& doc;
    std::vector<bool> lists;  // ordered flag of every open list
    int quote_depth = 0;
    int table = -1;                                     // block index of the open table
    std::vector<std::vector<std::vector<Inline>>> rows; // table rows read so far
    std::vector<TableAlign> aligns;                     // column alignment, from the header
    std::vector<Inline>* sink = nullptr;                // cell receiving text, if any
    int cur = -1;             // block that receives inline text, -1 for none
    bool item_open = false;   // list item still waiting for its first paragraph
    int bold = 0, italic = 0, code = 0, strike = 0, link = 0, image = 0;
    int image_text = 0; // bytes of text seen inside the open image
    std::string href;

    explicit Importer(Document& d) : doc(d) {}

    std::uint8_t style() const {
        std::uint8_t s = 0;
        if (bold) s |= StyleBold;
        if (italic) s |= StyleItalic;
        if (code) s |= StyleCode;
        if (strike) s |= StyleStrike;
        if (link || image) s |= StyleLink;
        if (image) s |= StyleImage;
        return s;
    }

    void open(BlockType type, int level = 0) {
        Block b;
        b.type = (type == BlockType::Paragraph && quote_depth > 0) ? BlockType::Quote : type;
        b.level = level;
        b.inlines.push_back(Inline{});
        doc.blocks.push_back(std::move(b));
        cur = static_cast<int>(doc.blocks.size()) - 1;
    }

    void append(const char* s, size_t n) {
        if (n == 0) return;
        if (image > 0) image_text += static_cast<int>(n);
        std::vector<Inline>* target = sink;
        if (!target) {
            if (cur < 0) {
                if (is_blank(s, n)) return;
                open(BlockType::Paragraph);
            }
            Block& b = doc.blocks[cur];
            if (b.type == BlockType::Rule) return;
            target = &b.inlines;
        }

        const std::uint8_t st = style();
        const std::string h = (link > 0 || image > 0) ? href : std::string();
        Inline& last = target->back();
        if (last.style == st && last.href == h) {
            last.text.append(s, n);
        } else {
            target->push_back(Inline{std::string(s, n), st, h});
        }
    }
};

TableAlign to_table_align(MD_ALIGN align) {
    switch (align) {
        case MD_ALIGN_LEFT: return TableAlign::Left;
        case MD_ALIGN_CENTER: return TableAlign::Center;
        case MD_ALIGN_RIGHT: return TableAlign::Right;
        default: return TableAlign::None;
    }
}

// Moves the rows read so far into the table block. Short rows are padded with
// empty cells, and cells past the header's column count are dropped.
void finish_table(Importer& im) {
    Block& t = im.doc.blocks[im.table];
    const size_t cols = im.aligns.size();
    if (cols == 0) {
        t.type = BlockType::Paragraph; // no header row: nothing to show as a table
        t.cells.clear();
        t.aligns.clear();
    } else {
        t.aligns = im.aligns;
        t.cells.clear();
        for (const auto& row : im.rows) {
            for (size_t c = 0; c < cols; ++c) {
                std::vector<Inline> cell = c < row.size() ? row[c] : std::vector<Inline>{};
                if (cell.empty()) cell.push_back(Inline{});
                t.cells.push_back(std::move(cell));
            }
        }
    }
    im.rows.clear();
    im.aligns.clear();
    im.table = -1;
}

int on_enter_block(MD_BLOCKTYPE type, void* detail, void* ud) {
    Importer& im = *static_cast<Importer*>(ud);
    switch (type) {
        case MD_BLOCK_QUOTE:
            ++im.quote_depth;
            break;
        case MD_BLOCK_UL:
            im.lists.push_back(false);
            break;
        case MD_BLOCK_OL:
            im.lists.push_back(true);
            break;
        case MD_BLOCK_LI: {
            Block b;
            b.type = BlockType::ListItem;
            b.depth = std::max(0, static_cast<int>(im.lists.size()) - 1);
            b.ordered = !im.lists.empty() && im.lists.back();
            b.inlines.push_back(Inline{});
            im.doc.blocks.push_back(std::move(b));
            im.cur = static_cast<int>(im.doc.blocks.size()) - 1;
            im.item_open = true;

            const auto* li = static_cast<const MD_BLOCK_LI_DETAIL*>(detail);
            if (li && li->is_task) {
                const bool checked = li->task_mark == 'x' || li->task_mark == 'X';
                const char* box = checked ? "[x] " : "[ ] ";
                im.append(box, std::strlen(box));
            }
            break;
        }
        case MD_BLOCK_P:
            // The first paragraph of a list item is the item itself.
            if (im.item_open && im.cur >= 0
                && im.doc.blocks[im.cur].type == BlockType::ListItem) {
                im.item_open = false;
            } else {
                im.open(BlockType::Paragraph);
            }
            break;
        case MD_BLOCK_H: {
            const auto* h = static_cast<const MD_BLOCK_H_DETAIL*>(detail);
            im.open(BlockType::Heading, h ? static_cast<int>(h->level) : 1);
            break;
        }
        case MD_BLOCK_CODE: {
            im.open(BlockType::CodeBlock);
            const auto* cd = static_cast<const MD_BLOCK_CODE_DETAIL*>(detail);
            if (cd && cd->lang.text) {
                im.doc.blocks.back().lang = std::string(cd->lang.text, cd->lang.size);
            }
            break;
        }
        case MD_BLOCK_HR:
            im.open(BlockType::Rule);
            im.cur = -1;
            break;
        case MD_BLOCK_TABLE:
            im.open(BlockType::Table);
            im.table = im.cur;
            im.rows.clear();
            im.aligns.clear();
            break;
        case MD_BLOCK_TR:
            im.rows.emplace_back();
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD: {
            const auto* td = static_cast<const MD_BLOCK_TD_DETAIL*>(detail);
            // Only the header row defines the columns and their alignment.
            if (im.rows.size() == 1) {
                im.aligns.push_back(to_table_align(td ? td->align : MD_ALIGN_DEFAULT));
            }
            im.rows.back().emplace_back(std::vector<Inline>{Inline{}});
            im.sink = &im.rows.back().back();
            break;
        }
        default:
            // HTML blocks and other unsupported blocks contribute no text.
            break;
    }
    return 0;
}

int on_leave_block(MD_BLOCKTYPE type, void* /*detail*/, void* ud) {
    Importer& im = *static_cast<Importer*>(ud);
    if (type == MD_BLOCK_TH || type == MD_BLOCK_TD) {
        im.sink = nullptr;
        return 0;
    }
    if (type == MD_BLOCK_TR) {
        return 0;
    }
    if (type == MD_BLOCK_TABLE) {
        finish_table(im);
    }
    switch (type) {
        case MD_BLOCK_QUOTE:
            --im.quote_depth;
            break;
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            if (!im.lists.empty()) im.lists.pop_back();
            break;
        case MD_BLOCK_LI:
            im.item_open = false;
            break;
        case MD_BLOCK_CODE:
            if (im.cur >= 0) {
                std::string& text = im.doc.blocks[im.cur].inlines.back().text;
                if (!text.empty() && text.back() == '\n') text.pop_back();
            }
            break;
        default:
            break;
    }
    im.cur = -1;
    return 0;
}

int on_enter_span(MD_SPANTYPE type, void* detail, void* ud) {
    Importer& im = *static_cast<Importer*>(ud);
    switch (type) {
        case MD_SPAN_EM: ++im.italic; break;
        case MD_SPAN_STRONG: ++im.bold; break;
        case MD_SPAN_CODE: ++im.code; break;
        case MD_SPAN_DEL: ++im.strike; break;
        case MD_SPAN_A: {
            const auto* a = static_cast<const MD_SPAN_A_DETAIL*>(detail);
            if (im.link == 0 && a) im.href = attr_string(a->href);
            ++im.link;
            break;
        }
        case MD_SPAN_IMG: {
            const auto* img = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
            if (im.link == 0 && im.image == 0 && img) im.href = attr_string(img->src);
            ++im.image;
            im.image_text = 0;
            break;
        }
        default:
            break;
    }
    return 0;
}

int on_leave_span(MD_SPANTYPE type, void* /*detail*/, void* ud) {
    Importer& im = *static_cast<Importer*>(ud);
    switch (type) {
        case MD_SPAN_EM: --im.italic; break;
        case MD_SPAN_STRONG: --im.bold; break;
        case MD_SPAN_CODE: --im.code; break;
        case MD_SPAN_DEL: --im.strike; break;
        case MD_SPAN_A:
            if (--im.link == 0 && im.image == 0) im.href.clear();
            break;
        case MD_SPAN_IMG:
            if (im.image_text == 0) {
                // An image without alt text is named after its file.
                const std::string name = base_name(im.href);
                im.append(name.data(), name.size());
            }
            if (--im.image == 0 && im.link == 0) im.href.clear();
            break;
        default:
            break;
    }
    return 0;
}

int on_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* ud) {
    Importer& im = *static_cast<Importer*>(ud);
    switch (type) {
        case MD_TEXT_NORMAL:
        case MD_TEXT_CODE:
        case MD_TEXT_LATEXMATH:
            im.append(text, size);
            break;
        case MD_TEXT_ENTITY: {
            const std::string out = decode_entity(std::string(text, size));
            im.append(out.data(), out.size());
            break;
        }
        case MD_TEXT_BR:
        case MD_TEXT_SOFTBR:
            im.append(" ", 1);
            break;
        default:
            // NULLCHAR and raw HTML carry nothing the model can show.
            break;
    }
    return 0;
}

// ---------------------------------------------------------------- export

// Characters that would start Markdown syntax when they appear in plain text.
std::string escape_text(const std::string& s) {
    static const std::string special = "\\`*_[]<>&~";
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (special.find(c) != std::string::npos) out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string code_span(const std::string& text) {
    size_t longest = 0, run = 0;
    for (char c : text) {
        run = c == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string fence(longest + 1, '`');
    const bool pad = !text.empty() && (text.front() == '`' || text.back() == '`');
    return fence + (pad ? " " : "") + text + (pad ? " " : "") + fence;
}

// The emphasis and link state that is open while writing inline text.
struct Marks {
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool image = false;
    std::string href;
};

bool same(const Marks& a, const Marks& b) {
    return a.bold == b.bold && a.italic == b.italic && a.strike == b.strike
        && a.image == b.image && a.href == b.href;
}

Marks marks_of(const Inline& inl, bool gfm) {
    Marks m;
    m.bold = inl.style & StyleBold;
    m.italic = inl.style & StyleItalic;
    m.strike = gfm && (inl.style & StyleStrike);
    if (inl.style & StyleLink) {
        m.href = inl.href;
        m.image = inl.style & StyleImage;
    }
    return m;
}

std::string opening(const Marks& m) {
    std::string s;
    if (!m.href.empty()) s += m.image ? "![" : "[";
    if (m.bold) s += "**";
    if (m.italic) s += "*";
    if (m.strike) s += "~~";
    return s;
}

std::string closing(const Marks& m) {
    std::string s;
    if (m.strike) s += "~~";
    if (m.italic) s += "*";
    if (m.bold) s += "**";
    if (!m.href.empty()) {
        // Destinations with spaces or parentheses go into angle brackets.
        const bool bracketed = m.href.find_first_of(" ()") != std::string::npos;
        s += bracketed ? "](<" + m.href + ">)" : "](" + m.href + ")";
    }
    return s;
}

std::string render_inlines(const std::vector<Inline>& inlines, bool gfm) {
    std::string out;
    std::string pending; // whitespace held back so markers never touch it
    Marks active;

    for (const Inline& inl : inlines) {
        if (inl.text.empty()) continue;
        const Marks target = marks_of(inl, gfm);

        std::string lead, core, trail, rendered;
        if (inl.style & StyleCode) {
            rendered = code_span(inl.text);
        } else {
            const size_t first = inl.text.find_first_not_of(' ');
            if (first == std::string::npos) {
                pending += inl.text;
                continue;
            }
            const size_t last = inl.text.find_last_not_of(' ');
            lead = inl.text.substr(0, first);
            core = inl.text.substr(first, last - first + 1);
            trail = inl.text.substr(last + 1);
            rendered = escape_text(core);
        }

        if (!same(target, active)) {
            out += closing(active);
            out += pending;
            pending.clear();
            out += lead;
            out += opening(target);
            active = target;
        } else {
            out += pending;
            pending.clear();
            out += lead;
        }
        out += rendered;
        pending = trail;
    }
    out += closing(active);
    return out;
}

// Escapes what would otherwise turn a paragraph into block syntax.
std::string paragraph_text(const Block& b, bool gfm) {
    std::string s = render_inlines(b.inlines, gfm);
    if (s.empty()) return s;
    const char c = s[0];
    if (c == '#' || c == '>' || c == '-' || c == '+' || c == '=') {
        s.insert(0, "\\");
    }
    size_t digits = 0;
    while (digits < s.size() && s[digits] >= '0' && s[digits] <= '9') ++digits;
    if (digits > 0 && digits < s.size() && (s[digits] == '.' || s[digits] == ')')) {
        s.insert(digits, "\\");
    }
    return s;
}

// Pipe table. A pipe inside a cell is escaped so it stays inside its cell.
std::string table_text(const Block& b, bool gfm) {
    const int cols = table_cols(b);
    const int rows = table_rows(b);
    if (cols == 0 || rows == 0) return std::string();

    auto row_text = [&](int r) {
        std::string line = "|";
        for (int c = 0; c < cols; ++c) {
            std::string cell;
            for (char ch : render_inlines(b.cells[static_cast<size_t>(r * cols + c)], gfm)) {
                if (ch == '|') cell += "\\|";
                else cell.push_back(ch);
            }
            line += " " + cell + " |";
        }
        return line;
    };

    std::string delimiter = "|";
    for (TableAlign a : b.aligns) {
        const char* marks = a == TableAlign::Left ? ":---"
            : a == TableAlign::Center ? ":---:"
            : a == TableAlign::Right ? "---:"
            : "---";
        delimiter += std::string(" ") + marks + " |";
    }

    std::string out = row_text(0) + "\n" + delimiter;
    for (int r = 1; r < rows; ++r) {
        out += "\n" + row_text(r);
    }
    return out;
}

std::string code_block_text(const Block& b) {
    std::string content;
    for (const Inline& inl : b.inlines) content += inl.text;

    size_t longest = 0, run = 0;
    for (char c : content) {
        run = c == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string fence(std::max<size_t>(3, longest + 1), '`');

    std::string out = fence + "\n" + content;
    if (!content.empty() && content.back() != '\n') out += "\n";
    out += fence;
    return out;
}

std::string prefix_lines(const std::string& text, const std::string& prefix) {
    std::string out;
    size_t start = 0;
    bool first = true;
    while (true) {
        const size_t nl = text.find('\n', start);
        const std::string line = text.substr(start, nl == std::string::npos
            ? std::string::npos : nl - start);
        if (!first) out += "\n";
        first = false;
        out += line.empty() ? std::string(">") : prefix + line;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

} // namespace

bool import_markdown(const std::string& source, bool gfm, Document& out) {
    out.blocks.clear();
    Importer im(out);

    MD_PARSER parser{};
    parser.abi_version = 0;
    // In CommonMark do not enable table parsing: pipe tables become plain paragraphs,
    // so the user can see them as raw markdown (and we can re-parse later in GFM).
    parser.flags = gfm ? (MD_DIALECT_GITHUB | MD_FLAG_TABLES)
                       : (MD_DIALECT_COMMONMARK & ~MD_FLAG_TABLES);
    parser.enter_block = on_enter_block;
    parser.leave_block = on_leave_block;
    parser.enter_span = on_enter_span;
    parser.leave_span = on_leave_span;
    parser.text = on_text;

    const int rc = md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &parser, &im);

    // Post-pass: display math written as $$ ... $$ paragraphs becomes formula
    // blocks; fenced ```math blocks keep only the inner LaTeX. The user sees
    // the rendered formula, never the source markers.
    auto trim = [](std::string s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\t'))
            s.erase(s.begin());
        while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t'))
            s.pop_back();
        return s;
    };
    auto unwrap_dollars = [&](std::string tex) {
        tex = trim(tex);
        if (tex.size() >= 4 && tex.compare(0, 2, "$$") == 0
            && tex.compare(tex.size() - 2, 2, "$$") == 0) {
            tex = trim(tex.substr(2, tex.size() - 4));
        }
        return tex;
    };
    for (Block& b : out.blocks) {
        if (b.type != BlockType::CodeBlock) continue;
        if (!(b.lang == "math" || b.lang == "latex" || b.lang == "tex")) continue;
        std::string content;
        for (const Inline& inl : b.inlines) content += inl.text;
        const std::string tex = unwrap_dollars(content);
        b.inlines.clear();
        b.inlines.push_back(Inline{tex, 0, ""});
    }
    // Paragraphs like "$$\n\\frac{a}{b}\n$$" turn into formula blocks.
    std::vector<Block> blocks;
    blocks.reserve(out.blocks.size());
    for (size_t i = 0; i < out.blocks.size(); ++i) {
        Block& b = out.blocks[i];
        if (b.type == BlockType::Paragraph) {
            std::string text;
            for (const Inline& inl : b.inlines) text += inl.text;
            const std::string t = trim(text);
            if (t.size() >= 4 && t.compare(0, 2, "$$") == 0
                && t.compare(t.size() - 2, 2, "$$") == 0) {
                Block m;
                m.type = BlockType::CodeBlock;
                m.lang = "math";
                m.inlines.push_back(Inline{unwrap_dollars(t), 0, ""});
                blocks.push_back(std::move(m));
                continue;
            }
        }
        blocks.push_back(std::move(b));
    }
    out.blocks = std::move(blocks);

    if (out.blocks.empty()) {
        Block empty;
        empty.inlines.push_back(Inline{});
        out.blocks.push_back(std::move(empty));
    }
    return rc == 0;
}

namespace {

// Helper: check if a single paragraph block looks like a table row or delimiter
bool is_table_like_row(const std::string& line) {
    // must contain at least one pipe (outside code? but inlines are flattened)
    if (line.find('|') == std::string::npos) return false;
    // delimiter row: |---|:---:|---
    bool has_dash = false;
    for (char c : line) {
        if (c == '-') has_dash = true;
        else if (c != '|' && c != ':' && c != ' ' && c != '\t' && c != '\\') {
            // could be escaped, but check dash presence
        }
    }
    if (!has_dash) return false;
    bool delim = true;
    for (char c : line) {
        if (c == '-') { has_dash = true; continue; }
        if (c == '|' || c == ':' || c == ' ' || c == '\t' || c == '\\') continue;
        delim = false; break;
    }
    return delim && has_dash;
}

bool is_table_data_row(const std::string& line) {
    if (line.find('|') == std::string::npos) return false;
    // not just delimiter
    if (is_table_like_row(line)) return false;
    // has pipes
    return true;
}

std::string block_to_raw_text(const Block& b, bool gfm) {
    if (b.type == BlockType::Table) {
        return table_text(b, gfm);
    }
    // for other blocks, approximate
    if (b.type == BlockType::CodeBlock) return code_block_text(b);
    if (b.type == BlockType::Heading) {
        const int level = std::clamp(b.level, 1, 6);
        return std::string(static_cast<size_t>(level), '#') + " " + render_inlines(b.inlines, gfm);
    }
    if (b.type == BlockType::Rule) return "---";
    if (b.type == BlockType::ListItem) {
        // simple
        return "- " + render_inlines(b.inlines, gfm);
    }
    if (b.type == BlockType::Quote) {
        return prefix_lines(render_inlines(b.inlines, gfm), "> ");
    }
    return paragraph_text(b, gfm);
}

std::string inlines_plain(const std::vector<Inline>& inlines) {
    // produce plain text without markers for splitting cells roughly? easier: use render_inlines then strip
    return render_inlines(inlines, true);
}

std::vector<std::string> split_table_row_cells(const std::string& line) {
    std::vector<std::string> cells;
    std::string cur;
    bool esc = false;
    for (char c : line) {
        if (esc) {
            if (c == '|') cur += '|'; // escaped pipe becomes pipe char in cell
            else { cur += '\\'; cur += c; } // keep other escaped
            esc = false;
            continue;
        }
        if (c == '\\') { esc = true; continue; }
        if (c == '|') {
            // trim spaces
            while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t')) cur.pop_back();
            size_t s = 0;
            while (s < cur.size() && (cur[s] == ' ' || cur[s] == '\t')) ++s;
            cells.push_back(cur.substr(s));
            cur.clear();
            continue;
        }
        cur += c;
    }
    // if line didn't start/end with pipe, might be partial; try to be lenient
    if (!cur.empty() || cells.empty()) {
        // don't push trailing if it was empty due to leading/trailing pipe? usually both
        while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t')) cur.pop_back();
        size_t s = 0;
        while (s < cur.size() && (cur[s] == ' ' || cur[s] == '\t')) ++s;
        if (!(cells.empty() && cur.empty() && line.find('|') == std::string::npos)) {
            // only push if not just trailing empty from final pipe
        }
        // easier: if ends with pipe we already pushed; if not, push
        if (line.empty() || line.back() != '|') {
            cells.push_back(cur.substr(s));
        } else if (!cells.empty() && cells.back().empty() && cur.empty()) {
            // skip
        }
    }
    // remove empty leading if first char was pipe
    if (!cells.empty() && cells[0].empty() && !line.empty() && line[0] == '|') {
        cells.erase(cells.begin());
    }
    if (!cells.empty() && cells.back().empty() && !line.empty() && line.back() == '|') {
        cells.pop_back();
    }
    return cells;
}

TableAlign parse_align_token(const std::string& t) {
    bool left = !t.empty() && t.front() == ':';
    bool right = !t.empty() && t.back() == ':';
    if (left && right) return TableAlign::Center;
    if (left) return TableAlign::Left;
    if (right) return TableAlign::Right;
    return TableAlign::None;
}

std::vector<TableAlign> parse_delim_row(const std::string& line) {
    std::vector<TableAlign> aligns;
    auto cells = split_table_row_cells(line);
    for (auto& c : cells) {
        // count dashes
        bool has_dash = false;
        for (char ch : c) if (ch == '-') has_dash = true;
        if (!has_dash) { aligns.push_back(TableAlign::None); continue; }
        aligns.push_back(parse_align_token(c));
    }
    return aligns;
}

} // namespace

void tables_to_paragraphs(Document& doc, bool gfm) {
    Document out;
    for (const Block& b : doc.blocks) {
        if (b.type == BlockType::Table) {
            Block p;
            p.type = BlockType::Paragraph;
            std::string raw = table_text(b, gfm);
            // store as a single inline
            p.inlines.clear();
            p.inlines.push_back(Inline{raw, 0, ""});
            out.blocks.push_back(std::move(p));
            continue;
        }
        out.blocks.push_back(b);
    }
    if (out.blocks.empty()) {
        Block p;
        p.inlines.push_back(Inline{});
        out.blocks.push_back(std::move(p));
    }
    doc = std::move(out);
}

bool paragraphs_to_tables(Document& doc) {
    std::vector<Block> result;
    size_t i = 0;
    const size_t n = doc.blocks.size();
    bool changed = false;
    while (i < n) {
        const Block& b = doc.blocks[i];
        if (b.type == BlockType::Paragraph && i + 1 < n && doc.blocks[i + 1].type == BlockType::Paragraph) {
            // get raw text of first paragraph
            std::string line1 = render_inlines(b.inlines, true);
            std::string line2 = render_inlines(doc.blocks[i + 1].inlines, true);
            // try to detect table: line2 is delimiter
            if (is_table_like_row(line2) && line1.find('|') != std::string::npos) {
                auto aligns = parse_delim_row(line2);
                if (!aligns.empty()) {
                    // read header + data rows
                    std::vector<std::vector<std::string>> rows;
                    rows.push_back(split_table_row_cells(line1));
                    size_t j = i + 2;
                    while (j < n && doc.blocks[j].type == BlockType::Paragraph) {
                        std::string l = render_inlines(doc.blocks[j].inlines, true);
                        if (is_table_like_row(l)) break; // should not happen
                        if (l.find('|') == std::string::npos) break; // end of table
                        rows.push_back(split_table_row_cells(l));
                        ++j;
                    }
                    // build table block
                    Block t;
                    t.type = BlockType::Table;
                    t.inlines.push_back(Inline{});
                    const size_t cols = aligns.size();
                    t.aligns = aligns;
                    // normalize rows to cols
                    for (auto& r : rows) {
                        while (r.size() < cols) r.push_back("");
                        if (r.size() > cols) r.resize(cols);
                    }
                    // convert each cell string back to inlines by re-parsing? easier: put as plain text inline
                    for (auto& r : rows) {
                        for (auto& c : r) {
                            std::vector<Inline> cell;
                            if (c.empty()) cell.push_back(Inline{});
                            else cell.push_back(Inline{c, 0, ""});
                            t.cells.push_back(std::move(cell));
                        }
                    }
                    result.push_back(std::move(t));
                    i = j;
                    changed = true;
                    continue;
                }
            }
        }
        result.push_back(b);
        ++i;
    }
    if (changed) {
        if (result.empty()) {
            Block p; p.inlines.push_back(Inline{}); result.push_back(std::move(p));
        }
        doc.blocks = std::move(result);
    }
    return changed;
}

std::string export_markdown(const Document& doc, bool gfm) {
    std::vector<std::string> units;
    const size_t n = doc.blocks.size();
    size_t i = 0;

    while (i < n) {
        const Block& b = doc.blocks[i];

        if (b.type == BlockType::Quote) {
            // Consecutive quote blocks form one quote; blocks are separated by ">".
            std::string inner;
            size_t j = i;
            for (; j < n && doc.blocks[j].type == BlockType::Quote; ++j) {
                if (j > i) inner += "\n\n";
                inner += paragraph_text(doc.blocks[j], gfm);
            }
            units.push_back(prefix_lines(inner, "> "));
            i = j;
        } else if (b.type == BlockType::ListItem) {
            // Consecutive items form one list. Numbers continue across deeper
            // items and restart after a shallower one.
            // A change between bullets and numbers at the top level starts a new list.
            std::vector<int> counters(16, 0);
            std::string list;
            size_t j = i;
            for (; j < n && doc.blocks[j].type == BlockType::ListItem; ++j) {
                const Block& item = doc.blocks[j];
                if (j > i && item.depth == 0 && item.ordered != doc.blocks[i].ordered) break;

                const int d = std::clamp(item.depth, 0, 15);
                for (int k = d + 1; k < 16; ++k) counters[k] = 0;

                std::string marker;
                if (item.ordered) {
                    counters[d] = counters[d] > 0 ? counters[d] + 1 : 1;
                    marker = std::to_string(counters[d]) + ". ";
                } else {
                    counters[d] = 0;
                    marker = "- ";
                }

                // GFM task checkboxes are written as they are, not escaped.
                std::string body;
                const bool task = gfm && !item.inlines.empty() && item.inlines[0].style == 0
                    && (item.inlines[0].text.rfind("[x] ", 0) == 0
                        || item.inlines[0].text.rfind("[ ] ", 0) == 0);
                if (task) {
                    Block rest = item;
                    body = rest.inlines[0].text.substr(0, 4);
                    rest.inlines[0].text.erase(0, 4);
                    body += paragraph_text(rest, gfm);
                } else {
                    body = paragraph_text(item, gfm);
                }

                if (j > i) list += "\n";
                list += std::string(static_cast<size_t>(d) * 4, ' ') + marker + body;
            }
            units.push_back(list);
            i = j;
        } else {
            switch (b.type) {
                case BlockType::Heading: {
                    const int level = std::clamp(b.level, 1, 6);
                    units.push_back(std::string(static_cast<size_t>(level), '#') + " "
                                    + render_inlines(b.inlines, gfm));
                    break;
                }
                case BlockType::CodeBlock: {
                    std::string content;
                    for (const Inline& inl : b.inlines) content += inl.text;

                    // Formulas live in the file as $$ ... $$ display math and
                    // never as fenced code with visible LaTeX tags.
                    if (b.lang == "math" || b.lang == "latex" || b.lang == "tex") {
                        std::string tex = content;
                        while (!tex.empty() && (tex.front() == ' ' || tex.front() == '\n'))
                            tex.erase(tex.begin());
                        while (!tex.empty() && (tex.back() == ' ' || tex.back() == '\n'))
                            tex.pop_back();
                        if (tex.size() >= 4 && tex.compare(0, 2, "$$") == 0
                            && tex.compare(tex.size() - 2, 2, "$$") == 0) {
                            tex = tex.substr(2, tex.size() - 4); // already wrapped
                        }
                        if (tex.empty()) tex = "\\,";
                        units.push_back("$$\n" + tex + "\n$$");
                        break;
                    }

                    size_t longest = 0, run = 0;
                    for (char c : content) {
                        run = c == '`' ? run + 1 : 0;
                        longest = std::max(longest, run);
                    }
                    const std::string fence(std::max<size_t>(3, longest + 1), '`');
                    std::string out_block = fence;
                    if (!b.lang.empty()) out_block += b.lang;
                    out_block += "\n" + content;
                    if (!content.empty() && content.back() != '\n') out_block += "\n";
                    out_block += fence;
                    units.push_back(out_block);
                    break;
                }
                case BlockType::Rule:
                    units.push_back("---");
                    break;
                case BlockType::Table:
                    units.push_back(table_text(b, gfm));
                    break;
                default:
                    units.push_back(paragraph_text(b, gfm));
                    break;
            }
            ++i;
        }
    }

    std::string out;
    for (size_t k = 0; k < units.size(); ++k) {
        if (k > 0) out += "\n\n";
        out += units[k];
    }
    if (!out.empty()) out += "\n";
    return out;
}

bool looks_like_gfm(const std::string& source) {
    static const std::string strike = "~~";
    std::string fence;  // marker of the open fenced code block, empty outside one
    bool prev_has_pipe = false;
    size_t start = 0;

    while (start <= source.size()) {
        size_t nl = source.find('\n', start);
        if (nl == std::string::npos) nl = source.size();
        std::string line = source.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const size_t first = line.find_first_not_of(" \t");
        const std::string trimmed = first == std::string::npos ? "" : line.substr(first);

        if (!fence.empty()) {
            if (trimmed.rfind(fence, 0) == 0
                && trimmed.find_first_not_of('`', 0) == std::string::npos) {
                fence.clear();
            }
            prev_has_pipe = false;
            continue;
        }
        if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0) {
            fence = trimmed.substr(0, trimmed.find_first_not_of(trimmed[0]));
            prev_has_pipe = false;
            continue;
        }

        if (line.find(strike) != std::string::npos) return true;

        // Task list item: "- [ ] text", "* [x] text", "1. [ ] text".
        size_t p = 0;
        while (p < trimmed.size() && trimmed[p] >= '0' && trimmed[p] <= '9') ++p;
        if (p > 0 && p < trimmed.size() && (trimmed[p] == '.' || trimmed[p] == ')')) {
            ++p;
        } else if (p == 0 && p < trimmed.size() && (trimmed[p] == '-' || trimmed[p] == '*' || trimmed[p] == '+')) {
            ++p;
        } else {
            p = std::string::npos;
        }
        if (p != std::string::npos && p + 4 <= trimmed.size() && trimmed[p] == ' '
            && trimmed[p + 1] == '[' && trimmed[p + 3] == ']'
            && (trimmed[p + 2] == ' ' || trimmed[p + 2] == 'x' || trimmed[p + 2] == 'X')) {
            return true;
        }

        // Table delimiter row ("|---|:--:|") directly under a row with pipes.
        if (prev_has_pipe && line.find('|') != std::string::npos) {
            bool delimiter = true;
            bool has_dash = false;
            for (char c : line) {
                if (c == '-') has_dash = true;
                else if (c != '|' && c != ':' && c != ' ' && c != '\t') { delimiter = false; break; }
            }
            if (delimiter && has_dash) return true;
        }
        prev_has_pipe = line.find('|') != std::string::npos;

        if (nl == source.size()) break;
    }
    return false;
}

bool read_text_file(const std::string& path, std::string& out) {
    std::error_code ec;
    const std::filesystem::path fspath = std::filesystem::u8path(path);
    if (!std::filesystem::exists(fspath, ec) || std::filesystem::is_directory(fspath, ec)) {
        return false;
    }
    std::ifstream in(fspath, std::ios::binary);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    out = buf.str();
    return true;
}

bool write_text_file(const std::string& path, const std::string& text) {
    const std::filesystem::path fspath = std::filesystem::u8path(path);
    std::ofstream file(fspath, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

} // namespace md

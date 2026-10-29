#include "editor.hpp"

#include <algorithm>

#include "text.hpp"

namespace md {

namespace {

constexpr int kMaxListDepth = 4;

bool is_table(const Document& doc, int block) {
    return doc.blocks[block].type == BlockType::Table;
}

// A block's start, or a table's first cell.
Pos unit_begin(const Document& doc, int block) {
    return Pos{block, 0, 0, is_table(doc, block) ? 0 : -1};
}

// A block's end, or a table's last cell.
Pos unit_end(const Document& doc, int block) {
    const Block& b = doc.blocks[block];
    const int cell = b.type == BlockType::Table ? static_cast<int>(b.cells.size()) - 1 : -1;
    const std::vector<Inline>& inls = inlines_of(b, cell);
    const int last = static_cast<int>(inls.size()) - 1;
    return Pos{block, last, static_cast<int>(inls[last].text.size()), cell};
}

Pos doc_end(const Document& doc) {
    return unit_end(doc, static_cast<int>(doc.blocks.size()) - 1);
}

bool block_has(const Block& b, BlockType type, int level, bool ordered) {
    return b.type == type
        && (type != BlockType::Heading || b.level == level)
        && (type != BlockType::ListItem || b.ordered == ordered);
}

bool is_blank(const std::vector<Inline>& inlines) {
    for (const Inline& inl : inlines) {
        if (!inl.text.empty()) return false;
    }
    return true;
}

// Offsets inside a container ignore inline boundaries: text is the same before and
// after restyling, so a position maps to one global offset and back.
int text_length(const std::vector<Inline>& inlines) {
    int n = 0;
    for (const Inline& inl : inlines) n += static_cast<int>(inl.text.size());
    return n;
}

int global_offset(const std::vector<Inline>& inlines, Pos p) {
    int g = p.offset;
    for (int i = 0; i < p.inl; ++i) g += static_cast<int>(inlines[i].text.size());
    return g;
}

Pos pos_from_global(const std::vector<Inline>& inlines, int block, int cell, int g) {
    const int last = static_cast<int>(inlines.size()) - 1;
    int acc = 0;
    for (int i = 0; i <= last; ++i) {
        const int n = static_cast<int>(inlines[i].text.size());
        if (g <= acc + n || i == last) {
            return Pos{block, i, std::max(0, std::min(g - acc, n)), cell};
        }
        acc += n;
    }
    return Pos{block, 0, 0, cell};
}

// The text containers a selection covers, in order. A selection inside one
// container gives just that one; a selection over blocks skips tables, which
// are only ever removed as a whole.
std::vector<std::pair<int, int>> units_between(const Document& doc, Pos s, Pos e) {
    std::vector<std::pair<int, int>> out;
    if (s.block == e.block) {
        out.emplace_back(s.block, s.cell);
        return out;
    }
    for (int bi = s.block; bi <= e.block; ++bi) {
        if (is_table(doc, bi)) continue;
        out.emplace_back(bi, -1);
    }
    return out;
}

// Sets or clears `flag` on the characters in [lo, hi) of one container.
void restyle_range(std::vector<Inline>& inlines, int lo, int hi, std::uint8_t flag, bool set) {
    const std::uint8_t fallback = inlines.front().style;

    // Split every inline at the range edges, restyle the middle pieces.
    std::vector<Inline> pieces;
    int g = 0;
    for (const Inline& inl : inlines) {
        const int n = static_cast<int>(inl.text.size());
        std::vector<int> cuts{g, g + n};
        for (int x : {lo, hi}) {
            if (x > g && x < g + n) cuts.push_back(x);
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

        for (size_t k = 0; k + 1 < cuts.size(); ++k) {
            const int a = cuts[k];
            const int b = cuts[k + 1];
            const bool inside = a >= lo && b <= hi;
            std::uint8_t style = inl.style;
            if (inside) {
                style = static_cast<std::uint8_t>(set ? (inl.style | flag)
                                                      : (inl.style & ~flag));
            }
            pieces.push_back(Inline{inl.text.substr(a - g, b - a), style, inl.href});
        }
        g += n;
    }

    // Merge neighbours with equal style and drop empty pieces.
    std::vector<Inline> merged;
    for (Inline& piece : pieces) {
        if (piece.text.empty()) continue;
        if (!merged.empty() && merged.back().style == piece.style
            && merged.back().href == piece.href) {
            merged.back().text += piece.text;
        } else {
            merged.push_back(std::move(piece));
        }
    }
    if (merged.empty()) {
        merged.push_back(Inline{"", fallback});
    }
    inlines = std::move(merged);
}

} // namespace

std::pair<Pos, Pos> Editor::selection() const {
    return caret_ < anchor_ ? std::make_pair(caret_, anchor_)
                            : std::make_pair(anchor_, caret_);
}

Pos Editor::block_begin(int block) const { return unit_begin(doc_, block); }

Pos Editor::block_end(int block) const { return unit_end(doc_, block); }

void Editor::ensure_inlines(std::vector<Inline>& inlines) const {
    if (inlines.empty()) {
        inlines.push_back(Inline{});
    }
}

std::vector<Inline>& Editor::cont(Pos p) {
    return inlines_of(doc_.blocks[p.block], p.cell);
}

const std::vector<Inline>& Editor::cont(Pos p) const {
    return inlines_of(doc_.blocks[p.block], p.cell);
}

bool Editor::selectable(Pos a, Pos b) const {
    if (!is_table(doc_, a.block) && !is_table(doc_, b.block)) return true;
    return a.block == b.block && a.cell == b.cell;
}

Pos Editor::move_left(Pos p) const {
    while (true) {
        const Block& b = doc_.blocks[p.block];
        const std::vector<Inline>& inls = inlines_of(b, p.cell);
        const std::string& text = inls[p.inl].text;
        if (p.offset > 0) {
            p.offset = prev_char(text, p.offset);
            return p;
        }
        if (p.inl > 0) {
            // Inline boundaries are not characters: step over the previous
            // inline's last character. Empty inlines are skipped entirely.
            --p.inl;
            const std::string& t = inls[p.inl].text;
            p.offset = 0;
            if (!t.empty()) {
                p.offset = prev_char(t, static_cast<int>(t.size()));
                return p;
            }
            continue;
        }
        if (p.cell > 0) {
            // Previous table cell, at its end.
            --p.cell;
            const std::vector<Inline>& prev = inlines_of(b, p.cell);
            p.inl = static_cast<int>(prev.size()) - 1;
            p.offset = static_cast<int>(prev[p.inl].text.size());
            continue;
        }
        if (p.block > 0) {
            // One step over the block separator.
            return block_end(p.block - 1);
        }
        return p;
    }
}

Pos Editor::move_right(Pos p) const {
    while (true) {
        const Block& b = doc_.blocks[p.block];
        const std::vector<Inline>& inls = inlines_of(b, p.cell);
        const std::string& text = inls[p.inl].text;
        if (p.offset < static_cast<int>(text.size())) {
            p.offset = next_char(text, p.offset);
            return p;
        }
        if (p.inl + 1 < static_cast<int>(inls.size())) {
            ++p.inl;
            const std::string& t = inls[p.inl].text;
            p.offset = 0;
            if (!t.empty()) {
                p.offset = next_char(t, 0);
                return p;
            }
            continue;
        }
        if (p.cell >= 0 && p.cell + 1 < static_cast<int>(b.cells.size())) {
            // Next table cell, at its start.
            ++p.cell;
            p.inl = 0;
            p.offset = 0;
            continue;
        }
        if (p.block + 1 < static_cast<int>(doc_.blocks.size())) {
            return block_begin(p.block + 1);
        }
        return p;
    }
}

void Editor::collapse_to(Pos p) {
    caret_ = p;
    anchor_ = p;
    pending_ = false;
}

std::uint8_t Editor::style_at(Pos p) const {
    return cont(p)[p.inl].style;
}

void Editor::normalize_caret() {
    std::vector<Inline>& inls = cont(caret_);
    ensure_inlines(inls);
    if (inls.size() > 1 && inls[caret_.inl].text.empty()) {
        const int old = caret_.inl;
        if (old + 1 < static_cast<int>(inls.size())) {
            // The next inline slides into index `old` once this one is erased.
            caret_ = Pos{caret_.block, old, 0, caret_.cell};
        } else {
            caret_ = Pos{caret_.block, old - 1,
                         static_cast<int>(inls[old - 1].text.size()), caret_.cell};
        }
        inls.erase(inls.begin() + old);
    }
    anchor_ = caret_;
}

void Editor::click(const Layout& layout, FontBook& fonts, float x, float y, bool extend) {
    caret_ = hit_test(doc_, layout, fonts, x, y);
    if (!extend || !selectable(anchor_, caret_)) {
        anchor_ = caret_;
    }
    goal_x_ = -1.0f;
    pending_ = false;
    blink_tick = SDL_GetTicks();
}

void Editor::move(const Layout& layout, FontBook& fonts, Move m, bool extend) {
    pending_ = false;
    if (has_selection() && !extend) {
        // Left/Right collapse to the corresponding selection edge first.
        const auto [s, e] = selection();
        if (m == Move::Left) {
            collapse_to(s);
            return;
        }
        if (m == Move::Right) {
            collapse_to(e);
            return;
        }
    }

    if (m != Move::Up && m != Move::Down) {
        goal_x_ = -1.0f;
    }

    switch (m) {
        case Move::Left:
            caret_ = move_left(caret_);
            break;
        case Move::Right:
            caret_ = move_right(caret_);
            break;
        case Move::LineStart:
        case Move::LineEnd: {
            float cx = 0;
            const int li = locate_caret(doc_, layout, fonts, caret_, &cx);
            if (li >= 0) {
                const Line& line = layout.lines[li];
                if (line.spans.empty()) {
                    caret_ = Pos{line.block, 0, 0, line.cell};
                } else if (m == Move::LineStart) {
                    const Span& s = line.spans.front();
                    caret_ = Pos{line.block, s.inl, s.byte_begin, line.cell};
                } else {
                    const Span& s = line.spans.back();
                    caret_ = Pos{line.block, s.inl, s.byte_end, line.cell};
                }
            }
            break;
        }
        case Move::DocStart:
            caret_ = block_begin(0);
            break;
        case Move::DocEnd:
            caret_ = doc_end(doc_);
            break;
        case Move::Up:
        case Move::Down: {
            float cx = 0;
            const int li = locate_caret(doc_, layout, fonts, caret_, &cx);
            if (li < 0) break;
            if (goal_x_ < 0) goal_x_ = cx;
            const int target = m == Move::Up ? li - 1 : li + 1;
            if (target < 0 || target >= static_cast<int>(layout.lines.size())) break;
            const Line& line = layout.lines[target];
            caret_ = hit_test(doc_, layout, fonts, goal_x_,
                              line.y + line.height / 2.0f);
            break;
        }
    }

    if (!extend || !selectable(anchor_, caret_)) {
        anchor_ = caret_;
    }
    blink_tick = SDL_GetTicks();
}

void Editor::insert_text(const std::string& utf8) {
    if (has_selection()) {
        delete_selection();
    }
    size_t start = 0;
    while (start <= utf8.size()) {
        const size_t nl = utf8.find('\n', start);
        const std::string piece = utf8.substr(start, nl == std::string::npos
            ? std::string::npos : nl - start);
        if (!piece.empty()) {
            insert_piece(piece);
        }
        if (nl == std::string::npos) break;
        newline();
        start = nl + 1;
    }
    anchor_ = caret_;
    blink_tick = SDL_GetTicks();
}

void Editor::insert_piece(const std::string& piece) {
    Block& target = doc_.blocks[caret_.block];
    if (target.type == BlockType::Rule) {
        target.type = BlockType::Paragraph; // a rule has no text: typing turns it into a paragraph
    }
    if (pending_ && pending_style_ != style_at(caret_)) {
        // Cut the current inline at the caret and put the fresh, styled one between.
        std::vector<Inline>& inls = cont(caret_);
        const int k = caret_.inl;
        const Inline old = inls[k];
        const int off = caret_.offset;

        std::vector<Inline> parts;
        if (off > 0) parts.push_back(Inline{old.text.substr(0, off), old.style, old.href});
        const int fresh = k + static_cast<int>(parts.size());
        parts.push_back(Inline{"", pending_style_});
        if (off < static_cast<int>(old.text.size())) {
            parts.push_back(Inline{old.text.substr(off), old.style, old.href});
        }
        inls.erase(inls.begin() + k);
        inls.insert(inls.begin() + k, parts.begin(), parts.end());
        caret_ = Pos{caret_.block, fresh, 0, caret_.cell};
    }
    pending_ = false;
    cont(caret_)[caret_.inl].text.insert(caret_.offset, piece);
    caret_.offset += static_cast<int>(piece.size());
}

void Editor::newline() {
    pending_ = false;
    if (in_table()) {
        table_below();
        blink_tick = SDL_GetTicks();
        return;
    }

    Block& b = doc_.blocks[caret_.block];

    if (b.type == BlockType::CodeBlock) {
        // Preformatted: a newline stays inside the block.
        b.inlines[caret_.inl].text.insert(caret_.offset, "\n");
        ++caret_.offset;
        anchor_ = caret_;
        blink_tick = SDL_GetTicks();
        return;
    }

    if (b.type == BlockType::ListItem && is_blank(b.inlines)) {
        // Enter on an empty item leaves the list.
        b.type = BlockType::Paragraph;
        b.depth = 0;
        b.ordered = false;
        blink_tick = SDL_GetTicks();
        return;
    }

    Inline& current = b.inlines[caret_.inl];
    Inline tail{current.text.substr(caret_.offset), current.style, current.href};
    current.text.erase(caret_.offset);

    std::vector<Inline> rest;
    rest.push_back(std::move(tail));
    for (size_t i = caret_.inl + 1; i < b.inlines.size(); ++i) {
        rest.push_back(std::move(b.inlines[i]));
    }
    b.inlines.resize(caret_.inl + 1);
    ensure_inlines(b.inlines);

    Block next;
    next.type = (b.type == BlockType::Heading || b.type == BlockType::Rule)
        ? BlockType::Paragraph : b.type;
    next.depth = b.depth;
    next.ordered = b.ordered;
    next.inlines = std::move(rest);
    ensure_inlines(next.inlines);

    doc_.blocks.insert(doc_.blocks.begin() + caret_.block + 1, std::move(next));
    collapse_to(Pos{caret_.block + 1, 0, 0, -1});
    blink_tick = SDL_GetTicks();
}

void Editor::remove_block(int index) {
    pending_ = false;
    if (doc_.blocks.size() == 1) {
        // The document always keeps one block: it becomes an empty paragraph.
        Block& only = doc_.blocks[0];
        only.type = BlockType::Paragraph;
        only.level = 0;
        only.depth = 0;
        only.ordered = false;
        only.cells.clear();
        only.aligns.clear();
        ensure_inlines(only.inlines);
        return;
    }
    doc_.blocks.erase(doc_.blocks.begin() + index);
}

void Editor::backspace() {
    if (in_table()) {
        // Never leaves the cell: at its start there is nothing to delete.
        if (!has_selection()) {
            const Pos target = move_left(caret_);
            if (target == caret_ || target.block != caret_.block || target.cell != caret_.cell) {
                return;
            }
            anchor_ = caret_;
            caret_ = target;
        }
        delete_selection();
        blink_tick = SDL_GetTicks();
        return;
    }

    const bool at_start = caret_.inl == 0 && caret_.offset == 0;
    Block& cur = doc_.blocks[caret_.block];

    if (!has_selection() && at_start) {
        if (cur.type == BlockType::Rule) {
            // A rule is removed as a whole; the caret lands at the end of the block above.
            const int r = caret_.block;
            remove_block(r);
            collapse_to(r > 0 ? block_end(r - 1) : block_begin(0));
            blink_tick = SDL_GetTicks();
            return;
        }
        if (caret_.block > 0 && doc_.blocks[caret_.block - 1].type == BlockType::Rule) {
            // Backspace right below a rule removes the rule.
            remove_block(caret_.block - 1);
            collapse_to(Pos{caret_.block - 1, 0, 0, -1});
            blink_tick = SDL_GetTicks();
            return;
        }
        if (cur.type != BlockType::Paragraph) {
            // At the start of a heading, quote, list item or code block, backspace
            // turns the block back into a paragraph first.
            cur.type = BlockType::Paragraph;
            cur.level = 0;
            cur.depth = 0;
            cur.ordered = false;
            pending_ = false;
            blink_tick = SDL_GetTicks();
            return;
        }
    }

    if (!has_selection()) {
        // Select exactly one step backwards, then delete it. This uniformly
        // handles characters, inline boundaries and block merges.
        const Pos target = move_left(caret_);
        if (target == caret_) return;
        if (is_table(doc_, target.block)) return; // text never merges into a table
        anchor_ = caret_;
        caret_ = target;
    }
    delete_selection();
    blink_tick = SDL_GetTicks();
}

void Editor::delete_forward() {
    if (in_table()) {
        if (!has_selection()) {
            const Pos target = move_right(caret_);
            if (target == caret_ || target.block != caret_.block || target.cell != caret_.cell) {
                return;
            }
            anchor_ = target;
        }
        delete_selection();
        blink_tick = SDL_GetTicks();
        return;
    }

    if (!has_selection()) {
        if (doc_.blocks[caret_.block].type == BlockType::Rule) {
            // Delete on a rule removes it; the caret moves to the block below.
            const int r = caret_.block;
            remove_block(r);
            const int count = static_cast<int>(doc_.blocks.size());
            collapse_to(r < count ? block_begin(r)
                                  : (r > 0 ? block_end(r - 1) : block_begin(0)));
            blink_tick = SDL_GetTicks();
            return;
        }
        const Pos end = block_end(caret_.block);
        const bool at_end = caret_.inl == end.inl && caret_.offset == end.offset;
        if (at_end && caret_.block + 1 < static_cast<int>(doc_.blocks.size())
            && doc_.blocks[caret_.block + 1].type == BlockType::Rule) {
            remove_block(caret_.block + 1);
            blink_tick = SDL_GetTicks();
            return;
        }
        const Pos target = move_right(caret_);
        if (target == caret_) return;
        if (is_table(doc_, target.block)) return; // text never merges into a table
        anchor_ = target;
    }
    delete_selection();
    blink_tick = SDL_GetTicks();
}

void Editor::toggle_style(std::uint8_t flag) {
    if (!has_selection()) {
        const std::uint8_t base = pending_ ? pending_style_ : style_at(caret_);
        pending_style_ = static_cast<std::uint8_t>(base ^ flag);
        pending_ = true;
        blink_tick = SDL_GetTicks();
        return;
    }

    const bool set = (active_style() & flag) != flag;
    const auto [s, e] = selection();
    const int caret_g = global_offset(cont(caret_), caret_);
    const int anchor_g = global_offset(cont(anchor_), anchor_);
    const Pos caret_at = caret_;
    const Pos anchor_at = anchor_;

    for (const auto& [bi, ci] : units_between(doc_, s, e)) {
        std::vector<Inline>& inls = inlines_of(doc_.blocks[bi], ci);
        const int lo = (bi == s.block && ci == s.cell) ? global_offset(inls, s) : 0;
        const int hi = (bi == e.block && ci == e.cell) ? global_offset(inls, e)
                                                       : text_length(inls);
        restyle_range(inls, lo, hi, flag, set);
    }

    caret_ = pos_from_global(cont(caret_at), caret_at.block, caret_at.cell, caret_g);
    anchor_ = pos_from_global(cont(anchor_at), anchor_at.block, anchor_at.cell, anchor_g);
    blink_tick = SDL_GetTicks();
}

std::uint8_t Editor::active_style() const {
    if (!has_selection()) {
        return pending_ ? pending_style_ : style_at(caret_);
    }

    const auto [s, e] = selection();
    std::uint8_t acc = 0xFF;
    bool any = false;
    for (const auto& [bi, ci] : units_between(doc_, s, e)) {
        const std::vector<Inline>& inls = inlines_of(doc_.blocks[bi], ci);
        const int lo = (bi == s.block && ci == s.cell) ? global_offset(inls, s) : 0;
        const int hi = (bi == e.block && ci == e.cell) ? global_offset(inls, e)
                                                       : text_length(inls);
        int g = 0;
        for (const Inline& inl : inls) {
            const int n = static_cast<int>(inl.text.size());
            if (std::min(g + n, hi) > std::max(g, lo)) {
                acc = static_cast<std::uint8_t>(acc & inl.style);
                any = true;
            }
            g += n;
        }
    }
    return any ? acc : 0;
}

void Editor::delete_selection() {
    if (!has_selection()) return;
    pending_ = false;
    const auto [s, e] = selection();

    if (s.block == e.block) {
        std::vector<Inline>& inls = cont(s);
        if (s.inl == e.inl) {
            inls[s.inl].text.erase(s.offset, e.offset - s.offset);
        } else {
            inls[s.inl].text.erase(s.offset);
            inls[e.inl].text.erase(0, e.offset);
            inls.erase(inls.begin() + s.inl + 1, inls.begin() + e.inl);
        }
    } else {
        Block& sb = doc_.blocks[s.block];
        Block& eb = doc_.blocks[e.block];
        sb.inlines[s.inl].text.erase(s.offset);
        sb.inlines.resize(s.inl + 1);

        eb.inlines[e.inl].text.erase(0, e.offset);
        for (size_t i = e.inl; i < eb.inlines.size(); ++i) {
            sb.inlines.push_back(std::move(eb.inlines[i]));
        }
        // A rule has no text of its own: merging into it leaves a paragraph.
        if (sb.type == BlockType::Rule) {
            sb.type = BlockType::Paragraph;
        }
        doc_.blocks.erase(doc_.blocks.begin() + s.block + 1, doc_.blocks.begin() + e.block + 1);
    }

    caret_ = s;
    normalize_caret();
    blink_tick = SDL_GetTicks();
}

void Editor::set_block(BlockType type, int level, bool ordered) {
    if (in_table()) return;
    const auto [s, e] = selection();
    bool all = true;
    for (int bi = s.block; bi <= e.block; ++bi) {
        if (is_table(doc_, bi)) continue;
        all = all && block_has(doc_.blocks[bi], type, level, ordered);
    }

    for (int bi = s.block; bi <= e.block; ++bi) {
        Block& blk = doc_.blocks[bi];
        if (blk.type == BlockType::Table) continue;
        if (all) {
            blk.type = BlockType::Paragraph;
            blk.level = 0;
            blk.depth = 0;
            blk.ordered = false;
            continue;
        }
        blk.type = type;
        blk.level = type == BlockType::Heading ? level : 0;
        blk.ordered = type == BlockType::ListItem && ordered;
        if (type != BlockType::ListItem) {
            blk.depth = 0;
        }
    }
    pending_ = false;
    blink_tick = SDL_GetTicks();
}

bool Editor::indent(int delta) {
    const auto [s, e] = selection();
    bool any = false;
    for (int bi = s.block; bi <= e.block; ++bi) {
        Block& blk = doc_.blocks[bi];
        if (blk.type != BlockType::ListItem) continue;
        blk.depth = std::clamp(blk.depth + delta, 0, kMaxListDepth);
        any = true;
    }
    if (any) blink_tick = SDL_GetTicks();
    return any;
}

void Editor::insert_rule() {
    pending_ = false;
    const int cur = caret_.block;
    int at = cur + 1;
    if (is_blank(doc_.blocks[cur].inlines) && doc_.blocks[cur].type == BlockType::Paragraph) {
        doc_.blocks[cur].type = BlockType::Rule;
    } else {
        Block rule;
        rule.type = BlockType::Rule;
        ensure_inlines(rule.inlines);
        doc_.blocks.insert(doc_.blocks.begin() + at, std::move(rule));
        ++at;
    }

    Block para;
    ensure_inlines(para.inlines);
    doc_.blocks.insert(doc_.blocks.begin() + at, std::move(para));
    collapse_to(Pos{at, 0, 0, -1});
    blink_tick = SDL_GetTicks();
}

void Editor::select_all() {
    if (in_table()) {
        // Inside a table, select the cell's text only.
        const std::vector<Inline>& inls = cont(caret_);
        const int last = static_cast<int>(inls.size()) - 1;
        anchor_ = Pos{caret_.block, 0, 0, caret_.cell};
        caret_ = Pos{caret_.block, last, static_cast<int>(inls[last].text.size()), caret_.cell};
        return;
    }
    // Tables cannot be part of a selection end, so the range runs between the
    // first and last blocks that are not tables.
    const int count = static_cast<int>(doc_.blocks.size());
    int first = 0;
    while (first < count && is_table(doc_, first)) ++first;
    int last = count - 1;
    while (last >= 0 && is_table(doc_, last)) --last;
    if (first > last) return;
    anchor_ = unit_begin(doc_, first);
    caret_ = unit_end(doc_, last);
}

std::string Editor::selected_text() const {
    if (!has_selection()) return {};
    const auto [s, e] = selection();
    const auto units = units_between(doc_, s, e);
    std::string out;

    for (size_t k = 0; k < units.size(); ++k) {
        const auto [bi, ci] = units[k];
        const std::vector<Inline>& inls = inlines_of(doc_.blocks[bi], ci);
        const bool first_unit = bi == s.block && ci == s.cell;
        const bool last_unit = bi == e.block && ci == e.cell;
        const int first_inl = first_unit ? s.inl : 0;
        const int last_inl = last_unit ? e.inl : static_cast<int>(inls.size()) - 1;
        for (int i = first_inl; i <= last_inl; ++i) {
            const std::string& text = inls[i].text;
            const int begin = (first_unit && i == s.inl) ? s.offset : 0;
            const int end = (last_unit && i == e.inl) ? e.offset
                : static_cast<int>(text.size());
            if (end > begin) out.append(text, begin, end - begin);
        }
        if (k + 1 < units.size()) out.push_back('\n');
    }
    return out;
}

void Editor::copy_selection() const {
    const std::string text = selected_text();
    if (!text.empty()) {
        SDL_SetClipboardText(text.c_str());
    }
}

void Editor::cut_selection() {
    copy_selection();
    delete_selection();
}

void Editor::paste() {
    char* text = SDL_GetClipboardText();
    if (!text) return;
    insert_text(text);
    SDL_free(text);
}

bool Editor::in_table() const {
    return is_table(doc_, caret_.block);
}

void Editor::insert_table(int cols, int rows) {
    pending_ = false;
    Block table;
    table.type = BlockType::Table;
    table.inlines.push_back(Inline{});
    table.aligns.assign(static_cast<size_t>(cols), TableAlign::None);
    table.cells.assign(static_cast<size_t>(cols * rows), std::vector<Inline>{Inline{}});

    Block para;
    ensure_inlines(para.inlines);

    const int at = caret_.block + 1;
    doc_.blocks.insert(doc_.blocks.begin() + at, std::move(table));
    doc_.blocks.insert(doc_.blocks.begin() + at + 1, std::move(para));
    caret_ = Pos{at, 0, 0, 0};
    anchor_ = caret_;
    pending_ = false;
    blink_tick = SDL_GetTicks();
}

void Editor::go_cell(int row, int col) {
    const int cols = table_cols(doc_.blocks[caret_.block]);
    caret_ = Pos{caret_.block, 0, 0, row * cols + col};
    anchor_ = caret_;
    pending_ = false;
    goal_x_ = -1.0f;
    blink_tick = SDL_GetTicks();
}

void Editor::table_below() {
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int row = caret_.cell / cols;
    const int col = caret_.cell % cols;
    if (row + 1 >= table_rows(t)) {
        t.cells.insert(t.cells.end(), static_cast<size_t>(cols), std::vector<Inline>{Inline{}});
    }
    go_cell(row + 1, col);
}

void Editor::table_add_row() {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int row = caret_.cell / cols;
    const int col = caret_.cell % cols;
    t.cells.insert(t.cells.begin() + (row + 1) * cols, static_cast<size_t>(cols),
                   std::vector<Inline>{Inline{}});
    go_cell(row + 1, col);
}

void Editor::table_remove_row() {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int rows = table_rows(t);
    if (rows <= 1) return;
    const int row = caret_.cell / cols;
    const int col = caret_.cell % cols;
    t.cells.erase(t.cells.begin() + row * cols, t.cells.begin() + (row + 1) * cols);
    go_cell(std::min(row, rows - 2), col);
}

void Editor::table_add_column() {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int rows = table_rows(t);
    const int row = caret_.cell / cols;
    const int col = caret_.cell % cols;
    for (int r = rows - 1; r >= 0; --r) {
        t.cells.insert(t.cells.begin() + r * cols + col + 1, std::vector<Inline>{Inline{}});
    }
    t.aligns.insert(t.aligns.begin() + col + 1, TableAlign::None);
    go_cell(row, col + 1);
}

void Editor::table_remove_column() {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int rows = table_rows(t);
    if (cols <= 1) return;
    const int row = caret_.cell / cols;
    const int col = caret_.cell % cols;
    for (int r = rows - 1; r >= 0; --r) {
        t.cells.erase(t.cells.begin() + r * cols + col);
    }
    t.aligns.erase(t.aligns.begin() + col);
    go_cell(row, std::min(col, cols - 2));
}

void Editor::table_remove() {
    if (!in_table()) return;
    const int b = caret_.block;
    remove_block(b);
    collapse_to(b > 0 ? block_end(b - 1) : block_begin(0));
    blink_tick = SDL_GetTicks();
}

void Editor::table_set_align(TableAlign align) {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    t.aligns[caret_.cell % table_cols(t)] = align;
}

TableAlign Editor::table_align() const {
    if (!in_table()) return TableAlign::None;
    const Block& t = doc_.blocks[caret_.block];
    return t.aligns[caret_.cell % table_cols(t)];
}

void Editor::table_step(bool forward) {
    if (!in_table()) return;
    Block& t = doc_.blocks[caret_.block];
    const int cols = table_cols(t);
    const int rows = table_rows(t);
    int row = caret_.cell / cols;
    int col = caret_.cell % cols;

    if (forward) {
        if (col + 1 < cols) {
            ++col;
        } else if (row + 1 < rows) {
            ++row;
            col = 0;
        } else {
            // Past the last cell: a new row is added.
            t.cells.insert(t.cells.end(), static_cast<size_t>(cols), std::vector<Inline>{Inline{}});
            ++row;
            col = 0;
        }
    } else {
        if (col > 0) {
            --col;
        } else if (row > 0) {
            --row;
            col = cols - 1;
        }
    }
    go_cell(row, col);
}

} // namespace md

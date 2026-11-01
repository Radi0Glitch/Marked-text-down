#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <utility>

#include "document.hpp"
#include "fontbook.hpp"
#include "layout.hpp"

namespace md {

// Cursor + selection state and every operation that mutates the document.
// All movement methods keep the selection anchor when `extend` is true.
class Editor {
public:
    explicit Editor(Document& doc) : doc_(doc) {}

    Pos caret() const { return caret_; }
    Pos anchor() const { return anchor_; }
    void set_caret(Pos p, bool extend = false) {
        caret_ = p;
        if (!extend) anchor_ = p;
        pending_ = false;
        blink_tick = SDL_GetTicks();
    }
    bool has_selection() const { return !(caret_ == anchor_); }
    std::pair<Pos, Pos> selection() const; // ordered: first <= second

    Uint64 blink_tick = 0;

    // Pointer input, document coordinates.
    void click(const Layout& layout, FontBook& fonts, float x, float y, bool extend);

    enum class Move { Left, Right, Up, Down, LineStart, LineEnd, DocStart, DocEnd };
    void move(const Layout& layout, FontBook& fonts, Move m, bool extend);

    // Editing. All of these collapse the selection and reset the blink timer.
    void insert_text(const std::string& utf8); // may contain '\n'
    // A link, or an image when `image` is set, replaces the selection or goes at the caret.
    void insert_link(const std::string& text, const std::string& href, bool image);
    void newline();
    void backspace();
    void delete_forward();
    void delete_selection();
    void select_all();

    // Inline styles (StyleBold, StyleItalic, StyleCode, ...). With a selection the
    // flag is set or cleared on every selected character; without one it applies
    // to the next typed text.
    void toggle_style(std::uint8_t flag);
    // Bits that are set on the whole selection, or on the caret / pending style.
    std::uint8_t active_style() const;

    // Block operations apply to every block the selection touches. When all of
    // them already have the requested form, the form is removed instead.
    void set_block(BlockType type, int level = 0, bool ordered = false);

    // Tables. The caret sits in one cell; commands act on the table it is in.
    // Enter moves down a row, Tab and Shift+Tab move between cells.
    bool in_table() const;
    void insert_table(int cols, int rows); // after the caret block
    void table_add_row();                  // below the caret row
    void table_remove_row();
    void table_add_column();               // right of the caret column
    void table_remove_column();
    void table_remove();
    void table_set_align(TableAlign align); // of the caret column
    TableAlign table_align() const;
    void table_step(bool forward);
    // Nests (+1) or un-nests (-1) selected list items. False if none is selected.
    bool indent(int delta);
    // Code blocks: Tab indents by four spaces (every line of a selection), Shift+Tab
    // removes a leading tab or up to four leading spaces. False when the caret is
    // outside a code block or Shift+Tab finds nothing to remove.
    bool code_tab(bool outdent);
    // Puts a horizontal rule after the caret's block and moves the caret below it.
    void insert_rule();
    const Block& caret_block() const { return doc_.blocks[caret_.block]; }

    std::string selected_text() const;
    void copy_selection() const;
    void cut_selection();
    void paste();

private:
    Document& doc_;
    Pos caret_{0, 0, 0};
    Pos anchor_{0, 0, 0};
    float goal_x_ = -1.0f; // remembered x for Up/Down
    bool pending_ = false;  // style for the next typed text
    std::uint8_t pending_style_ = 0;

    Pos block_begin(int block) const;
    Pos block_end(int block) const;
    Pos move_left(Pos p) const;
    Pos move_right(Pos p) const;
    void collapse_to(Pos p);
    void normalize_caret();
    void remove_block(int index);
    void ensure_inlines(std::vector<Inline>& inlines) const;
    std::vector<Inline>& cont(Pos p);
    const std::vector<Inline>& cont(Pos p) const;
    bool selectable(Pos a, Pos b) const;
    void go_cell(int row, int col);
    void table_below();
    std::uint8_t style_at(Pos p) const;
    void insert_piece(const std::string& piece);
};

} // namespace md

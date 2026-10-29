#pragma once

#include "document.hpp"
#include "fontbook.hpp"
#include "layout.hpp"
#include "ui.hpp"

namespace md {

// What the editor overlays on top of the document.
struct EditView {
    bool selecting = false;
    Pos sel_start, sel_end; // ordered
    bool caret_visible = false;
    Pos caret;
};

// Draws the laid out document into `viewport` (window coordinates), shifted by
// scroll_y. Sets and resets the renderer clip rect.
void render_document(ui::Context& ctx, const Document& doc, const Layout& layout,
                     FontBook& fonts, ui::Rect viewport, float scroll_y,
                     const EditView* edit);

} // namespace md

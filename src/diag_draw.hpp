#pragma once

#include "diag_model.hpp"
#include "fontbook.hpp"
#include "ui.hpp"

#include <vector>

// Native vector rendering of a DiagModel: boxes, arrows, lifelines and labels
// drawn with SDL primitives and plain fonts. No external renderers, no code on
// screen. Called with ctx == nullptr it only measures (size and slot rects).
namespace md {
namespace diagdraw {

struct Size {
    float w = 0;
    float h = 0;
};

// A clickable element: a node (edge == false) or an edge (edge == true),
// with `index` counting inside that group.
struct Slot {
    int index = 0;
    bool edge = false;
    ui::Rect rect;
};

// The element the editor marks with the accent and the text caret.
struct Selection {
    int index = -1;
    bool edge = false;
    bool matches(const Slot& s) const {
        return index >= 0 && index == s.index && edge == s.edge;
    }
};

// Measure (ctx == nullptr) or draw the diagram with its top-left corner at
// (x, y), laid out to fit into `max_w`. `slots` is filled in hit-test order:
// nodes first, then edges, so a node wins an overlap.
Size render(ui::Context* ctx, FontBook& fonts, const DiagModel& model,
            float x, float y, float max_w,
            const Selection* selected = nullptr,
            std::vector<Slot>* slots = nullptr);

// Renders a whole code block: parses its text into the model and draws it,
// or a neutral placeholder when the code has no visual form. The source text
// itself is never drawn.
Size render_block(ui::Context* ctx, FontBook& fonts, const std::string& code,
                  float x, float y, float max_w);

} // namespace diagdraw
} // namespace md

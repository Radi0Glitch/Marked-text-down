#pragma once

#include "fontbook.hpp"
#include "math_model.hpp"
#include "ui.hpp"

#include <vector>

// Native vector rendering of a MathNode tree. Draws with SDL primitives and
// plain fonts: fractions get a rule, roots get an overline, super/subscript
// are scaled and offset. No LaTeX engine, no external tools, no network.
// Called with ctx == nullptr it only measures (sizes and slot rects).
namespace md {
namespace mathdraw {

struct Size {
    float w = 0;
    float h = 0;
};

// A clickable text field: every Text leaf (and every empty sequence) reports
// the rect it occupies plus the path that reaches it in the tree.
struct Slot {
    std::vector<int> path;
    ui::Rect rect;
};

// Measure (ctx == nullptr) or draw the node with its top-left corner at (x, y).
// `size` is the base font size; smaller pieces scale down from it.
// `highlight` — path of the Text slot to mark with the cursor.
// `slots` — collected in drawing order (used by the editor for hit testing).
Size render(ui::Context* ctx, FontBook& fonts, const MathNode& node,
            float x, float y, float size,
            const std::vector<int>* highlight,
            std::vector<Slot>* slots,
            const std::vector<int>& path = {});

} // namespace mathdraw
} // namespace md

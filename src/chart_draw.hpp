#pragma once

#include "chart_model.hpp"
#include "fontbook.hpp"
#include "ui.hpp"

#include <vector>

// Native vector rendering of a ChartModel. Bar, line and pie charts drawn
// with SDL primitives and plain fonts. No external renderers, no code on
// screen. Called with ctx == nullptr it only measures (size and slot rects).
namespace md {
namespace chartdraw {

struct Size {
    float w = 0;
    float h = 0;
};

// A clickable element: a data row with `index`. When `value == true` the
// numeric cell is selected, otherwise the category label.
struct Slot {
    int index = 0;
    bool value = false;
    ui::Rect rect;
};

struct Selection {
    int index = -1;
    bool value = false;
    bool matches(const Slot& s) const {
        return index >= 0 && index == s.index && value == s.value;
    }
};

// Measure (ctx == nullptr) or draw the chart with its top-left corner at
// (x, y), laid out to fit into `max_w`. `slots` is filled with clickable
// category/value cells in editor order.
Size render(ui::Context* ctx, FontBook& fonts, const ChartModel& model,
            float x, float y, float max_w,
            const Selection* selected = nullptr,
            std::vector<Slot>* slots = nullptr);

// Renders a whole code block: parses its text into the model and draws it,
// or a neutral placeholder when the code has no visual form. The source text
// itself is never drawn.
Size render_block(ui::Context* ctx, FontBook& fonts, const std::string& code,
                  float x, float y, float max_w);

} // namespace chartdraw
} // namespace md

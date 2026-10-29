#pragma once

#include <string>
#include <vector>

// Visual chart model. Like formulas and diagrams, charts are edited visually:
// the user never sees or types code. The mermaid-flavoured text exists only
// inside the saved .md file (xychart-beta for bar/line, pie for round charts).
namespace md {

enum class ChartKind { Bar, Line, Pie };

struct ChartModel {
    ChartKind kind = ChartKind::Bar;
    std::string title;
    std::vector<std::string> labels; // categories, edited as text
    std::vector<std::string> values; // numbers, edited as text
};

// True when a code block's text looks like a chart (mermaid xychart-beta or
// pie). Such blocks are drawn as a picture; their code is never shown.
bool is_chart_text(const std::string& text);

// A new chart starts with one empty category so the user can type right away.
ChartModel chart_new();

// Serialize to the mermaid subset chart_from_text reads back. For the file only.
std::string chart_to_mermaid(const ChartModel& model);

// Parse mermaid xychart-beta / pie code back into the visual model. Sets
// *ok = false when the code has no visual counterpart: such blocks are then
// left untouched, so nothing the user cannot see is ever lost.
ChartModel chart_from_text(const std::string& text, bool* ok = nullptr);

// Numeric value of a data point (accepts a comma as the decimal separator).
double chart_value(const std::string& s);

} // namespace md

#pragma once

#include <string>
#include <vector>

namespace md {

// Visual diagram model. Like formulas, diagrams are built element by element in
// a visual editor: the user never sees or types code. The mermaid-flavoured
// text exists only inside the saved .md file, produced on save and parsed back
// when the block is opened for editing.
enum class DiagShape { Box, Rounded, Diamond, Ellipse, Stadium };
enum class DiagKind { Flowchart, Sequence };

struct DiagNode {
    std::string id;    // stable identifier, never shown to the user
    std::string label; // the visible text
    DiagShape shape = DiagShape::Box;
};

struct DiagEdge {
    std::string from = "";
    std::string to = "";
    std::string label;
    bool dashed = false;
};

struct DiagModel {
    DiagKind kind = DiagKind::Flowchart;
    bool lr = false; // flowchart direction: false = top-down, true = left-right
    std::vector<DiagNode> nodes;
    std::vector<DiagEdge> edges;
};

// Code-block languages that hold a diagram. Those blocks are drawn as a
// picture; their code is never shown on screen.
bool is_diagram_lang(const std::string& lang);

// A new diagram starts with one empty node so the user can type right away.
DiagModel diag_new();

// Serialize to the mermaid subset diag_from_text reads back. For the file only.
std::string diag_to_mermaid(const DiagModel& model);

// Parse mermaid / plantuml / graphviz-flavoured code back into the visual
// model. Sets *ok = false when the code has no visual counterpart (class
// diagrams, gantt, ...): such blocks are then left untouched, so nothing the
// user cannot see is ever lost.
DiagModel diag_from_text(const std::string& text, bool* ok = nullptr);

// Index of the first node with this id, or -1.
int diag_find_node(const DiagModel& model, const std::string& id);
// A fresh unique node id ("n1", "n2", ...).
std::string diag_new_id(const DiagModel& model);

} // namespace md

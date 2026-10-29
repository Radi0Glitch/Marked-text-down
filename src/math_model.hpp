#pragma once

#include <string>
#include <vector>

namespace md {

// Visual formula model. We never ask the user to type LaTeX, we build this tree
// from the visual editor and convert to LaTeX only for Pandoc/export.
struct MathNode {
    enum Type {
        Text,      // plain letters/numbers/symbols typed directly
        Seq,       // horizontal row of nodes (a + b)
        Frac,      // numerator / denominator
        Sqrt,      // sqrt with optional index? keep simple: sqrt(child)
        Root,      // n-th root
        Sup,       // base^sup
        Sub,       // base_sub
        SupSub,    // base^sup_sub
        Sum,       // sum with limits
        Prod,      // product with limits
        Int,       // integral with limits
        Parens,    // ( ... )
        Brackets,  // [ ... ]
        Braces,    // { ... }
        Abs,       // | ... |
        Matrix,    // simple matrix
        Piecewise, // cases
    };

    Type type = Text;
    std::string text;
    std::vector<MathNode> slots; // children in order of appearance
};

MathNode math_text(const std::string& s);
MathNode math_seq(std::vector<MathNode> children = {});
MathNode math_frac(MathNode num, MathNode den);
MathNode math_sqrt(MathNode radicand);
MathNode math_root(MathNode index, MathNode radicand);
MathNode math_sup(MathNode base, MathNode sup);
MathNode math_sub(MathNode base, MathNode sub);
MathNode math_supsup(MathNode base, MathNode sup, MathNode sub);
MathNode math_sum();
MathNode math_prod();
MathNode math_int();
MathNode math_parens(MathNode inner);
MathNode math_brackets(MathNode inner);
MathNode math_matrix(int rows, int cols);

// Convert visual model to LaTeX for Pandoc export.
std::string math_to_latex(const MathNode& node);

// Parse the LaTeX subset we generate back into the visual model. Used to open
// formulas loaded from a .md file in the visual editor. Unknown commands are
// skipped gracefully; the result always renders.
MathNode math_from_latex(const std::string& latex);

// Path helpers: a path is a list of slot indices from the root.
MathNode* node_at_path(MathNode& root, const std::vector<int>& path);
const MathNode* node_at_path(const MathNode& root, const std::vector<int>& path);
// Path of the first Text leaf under `base` (base itself if it is a Text).
// Returns false when the subtree holds no text slot.
bool first_text_path(const MathNode& node, const std::vector<int>& base,
                     std::vector<int>& out);

// Simple helpers
bool math_is_empty(const MathNode& node);

} // namespace md

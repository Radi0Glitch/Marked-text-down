#include "math_model.hpp"

#include <cstring>

namespace md {

namespace {

std::string latex_escape_text(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\textbackslash{}"; break;
            case '{': out += "\\{"; break;
            case '}': out += "\\}"; break;
            case '$': out += "\\$"; break;
            case '%': out += "\\%"; break;
            case '&': out += "\\&"; break;
            case '#': out += "\\#"; break;
            case '_': out += "\\_"; break;
            case '^': out += "\\^{}"; break;
            default: out += c; break;
        }
    }
    return out;
}

} // namespace

MathNode math_text(const std::string& s) {
    MathNode n;
    n.type = MathNode::Text;
    n.text = s;
    return n;
}

MathNode math_seq(std::vector<MathNode> children) {
    MathNode n;
    n.type = MathNode::Seq;
    n.slots = std::move(children);
    return n;
}

MathNode math_frac(MathNode num, MathNode den) {
    MathNode n;
    n.type = MathNode::Frac;
    n.slots.push_back(std::move(num));
    n.slots.push_back(std::move(den));
    return n;
}

MathNode math_sqrt(MathNode radicand) {
    MathNode n;
    n.type = MathNode::Sqrt;
    n.slots.push_back(std::move(radicand));
    return n;
}

MathNode math_root(MathNode index, MathNode radicand) {
    MathNode n;
    n.type = MathNode::Root;
    n.slots.push_back(std::move(index));
    n.slots.push_back(std::move(radicand));
    return n;
}

MathNode math_sup(MathNode base, MathNode sup) {
    MathNode n;
    n.type = MathNode::Sup;
    n.slots.push_back(std::move(base));
    n.slots.push_back(std::move(sup));
    return n;
}

MathNode math_sub(MathNode base, MathNode sub) {
    MathNode n;
    n.type = MathNode::Sub;
    n.slots.push_back(std::move(base));
    n.slots.push_back(std::move(sub));
    return n;
}

MathNode math_supsup(MathNode base, MathNode sup, MathNode sub) {
    MathNode n;
    n.type = MathNode::SupSub;
    n.slots.push_back(std::move(base));
    n.slots.push_back(std::move(sup));
    n.slots.push_back(std::move(sub));
    return n;
}

MathNode math_sum() {
    MathNode n;
    n.type = MathNode::Sum;
    n.slots.resize(3);
    return n;
}

MathNode math_prod() {
    MathNode n;
    n.type = MathNode::Prod;
    n.slots.resize(3);
    return n;
}

MathNode math_int() {
    MathNode n;
    n.type = MathNode::Int;
    n.slots.resize(3);
    return n;
}

MathNode math_parens(MathNode inner) {
    MathNode n;
    n.type = MathNode::Parens;
    n.slots.push_back(std::move(inner));
    return n;
}

MathNode math_brackets(MathNode inner) {
    MathNode n;
    n.type = MathNode::Brackets;
    n.slots.push_back(std::move(inner));
    return n;
}

MathNode math_matrix(int rows, int cols) {
    MathNode n;
    n.type = MathNode::Matrix;
    n.text = std::to_string(rows) + "x" + std::to_string(cols);
    n.slots.assign(static_cast<size_t>(rows * cols), MathNode{MathNode::Text, "", {}});
    return n;
}

bool math_is_empty(const MathNode& node) {
    if (node.type == MathNode::Text) return node.text.empty();
    for (const auto& s : node.slots) {
        if (!math_is_empty(s)) return false;
    }
    return true;
}

MathNode* node_at_path(MathNode& root, const std::vector<int>& path) {
    MathNode* n = &root;
    for (int i : path) {
        if (i < 0 || static_cast<size_t>(i) >= n->slots.size()) return nullptr;
        n = &n->slots[static_cast<size_t>(i)];
    }
    return n;
}

const MathNode* node_at_path(const MathNode& root, const std::vector<int>& path) {
    const MathNode* n = &root;
    for (int i : path) {
        if (i < 0 || static_cast<size_t>(i) >= n->slots.size()) return nullptr;
        n = &n->slots[static_cast<size_t>(i)];
    }
    return n;
}

bool first_text_path(const MathNode& node, const std::vector<int>& base,
                     std::vector<int>& out) {
    if (node.type == MathNode::Text) {
        out = base;
        return true;
    }
    if (node.type == MathNode::Seq) {
        for (size_t i = 0; i < node.slots.size(); ++i) {
            std::vector<int> child = base;
            child.push_back(static_cast<int>(i));
            if (first_text_path(node.slots[i], child, out)) return true;
        }
        out = base;
        return true; // empty sequence: the sequence itself is a place to type
    }
    // A structure: descend into its slots (each is a sequence in our model).
    for (size_t i = 0; i < node.slots.size(); ++i) {
        std::vector<int> child = base;
        child.push_back(static_cast<int>(i));
        if (first_text_path(node.slots[i], child, out)) return true;
    }
    out = base;
    return false;
}

static std::string to_latex_inner(const MathNode& node) {
    switch (node.type) {
        case MathNode::Seq: {
            std::string out;
            for (const auto& c : node.slots) out += to_latex_inner(c);
            return out;
        }
        case MathNode::Text:
            if (node.text.empty()) return "\\,";
            return latex_escape_text(node.text);
        case MathNode::Frac:
            if (node.slots.size() < 2) return "\\frac{\\,}{\\,}";
            return "\\frac{" + to_latex_inner(node.slots[0]) + "}{" + to_latex_inner(node.slots[1]) + "}";
        case MathNode::Sqrt:
            if (node.slots.empty()) return "\\sqrt{\\,}";
            return "\\sqrt{" + to_latex_inner(node.slots[0]) + "}";
        case MathNode::Root:
            if (node.slots.size() < 2) return "\\sqrt[\\,]{\\,}";
            return "\\sqrt[" + to_latex_inner(node.slots[0]) + "]{" + to_latex_inner(node.slots[1]) + "}";
        case MathNode::Sup:
            if (node.slots.size() < 2) return to_latex_inner(node.slots.empty() ? MathNode{} : node.slots[0]) + "^{\\,}";
            return "{" + to_latex_inner(node.slots[0]) + "}^{ " + to_latex_inner(node.slots[1]) + " }";
        case MathNode::Sub:
            if (node.slots.size() < 2) return to_latex_inner(node.slots.empty() ? MathNode{} : node.slots[0]) + "_{\\,}";
            return "{" + to_latex_inner(node.slots[0]) + "}_{ " + to_latex_inner(node.slots[1]) + " }";
        case MathNode::SupSub:
            if (node.slots.size() < 3) return "\\,";
            return "{" + to_latex_inner(node.slots[0]) + "}_{ " + to_latex_inner(node.slots[1]) + " }^{ " + to_latex_inner(node.slots[2]) + " }";
        case MathNode::Sum:
            if (node.slots.size() == 3)
                return "\\sum_{ " + to_latex_inner(node.slots[0]) + " }^{ " + to_latex_inner(node.slots[1]) + " } " + to_latex_inner(node.slots[2]);
            return "\\sum";
        case MathNode::Prod:
            if (node.slots.size() == 3)
                return "\\prod_{ " + to_latex_inner(node.slots[0]) + " }^{ " + to_latex_inner(node.slots[1]) + " } " + to_latex_inner(node.slots[2]);
            return "\\prod";
        case MathNode::Int:
            if (node.slots.size() == 3)
                return "\\int_{ " + to_latex_inner(node.slots[0]) + " }^{ " + to_latex_inner(node.slots[1]) + " } " + to_latex_inner(node.slots[2]);
            return "\\int";
        case MathNode::Parens:
            return "\\left(" + (node.slots.empty() ? "\\," : to_latex_inner(node.slots[0])) + "\\right)";
        case MathNode::Brackets:
            return "\\left[" + (node.slots.empty() ? "\\," : to_latex_inner(node.slots[0])) + "\\right]";
        case MathNode::Braces:
            return "\\left\\{" + (node.slots.empty() ? "\\," : to_latex_inner(node.slots[0])) + "\\right\\}";
        case MathNode::Abs:
            return "\\left| " + (node.slots.empty() ? "\\," : to_latex_inner(node.slots[0])) + " \\right|";
        case MathNode::Matrix: {
            int rows = 1, cols = 1;
            if (!node.text.empty()) {
                size_t x = node.text.find('x');
                if (x != std::string::npos) {
                    rows = std::max(1, std::stoi(node.text.substr(0, x)));
                    cols = std::max(1, std::stoi(node.text.substr(x + 1)));
                }
            }
            std::string body;
            size_t idx = 0;
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    if (idx < node.slots.size()) body += to_latex_inner(node.slots[idx++]);
                    else body += "\\,";
                    if (c < cols - 1) body += " & ";
                }
                if (r < rows - 1) body += " \\\\ ";
            }
            return "\\begin{bmatrix}" + body + "\\end{bmatrix}";
        }
        case MathNode::Piecewise:
            if (node.slots.empty()) return "\\begin{cases}\\,\\end{cases}";
            return "\\begin{cases}" + to_latex_inner(node.slots[0]) + "\\end{cases}";
        default:
            break;
    }
    return "\\,";
}

std::string math_to_latex(const MathNode& node) {
    std::string inner = to_latex_inner(node);
    if (inner.empty()) inner = "\\,";
    return "$$" + inner + "$$";
}

namespace {

// Recursive-descent parser for the exact LaTeX subset that math_to_latex
// emits, plus tolerant handling of hand-written variants ({}^{..}, bare
// groups). Unknown commands are skipped as text.
struct LatexParser {
    const std::string& s;
    size_t p = 0;

    void ws() {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\t')) ++p;
    }

    bool lit(const char* t) {
        const size_t n = std::char_traits<char>::length(t);
        if (s.compare(p, n, t) == 0) { p += n; return true; }
        return false;
    }

    // Consumes a balanced {...} group and returns its raw inner text.
    std::string group() {
        ws();
        if (p >= s.size() || s[p] != '{') return std::string();
        ++p;
        const size_t start = p;
        int depth = 1;
        while (p < s.size() && depth > 0) {
            if (s[p] == '\\' && p + 1 < s.size()) { p += 2; continue; }
            if (s[p] == '{') ++depth;
            else if (s[p] == '}') { --depth; if (depth == 0) break; }
            ++p;
        }
        std::string inner = s.substr(start, p - start);
        if (p < s.size()) ++p; // consume '}'
        return inner;
    }

    // Parses until end of input or a closing '}'. Consumes the '}'.
    MathNode parse_seq() {
        std::vector<MathNode> kids;
        auto push_text = [&](const std::string& t) {
            if (!kids.empty() && kids.back().type == MathNode::Text) {
                kids.back().text += t;
            } else {
                kids.push_back(MathNode{MathNode::Text, t, {}});
            }
        };
        while (true) {
            ws();
            if (p >= s.size()) break;
            if (s[p] == '}') { ++p; break; }
            if (s[p] == '{') {
                // bare group: parse inner as a sequence and splice children
                const std::string inner = group();
                MathNode sub = math_from_latex(inner);
                for (auto& c : sub.slots) kids.push_back(std::move(c));
                continue;
            }
            if (s[p] == '\\') {
                if (lit("\\frac")) {
                    MathNode n{MathNode::Frac, {}, {}};
                    n.slots.push_back(math_from_latex(group()));
                    n.slots.push_back(math_from_latex(group()));
                    kids.push_back(std::move(n));
                    continue;
                }
                if (lit("\\sqrt")) {
                    ws();
                    if (p < s.size() && s[p] == '[') {
                        ++p;
                        const size_t end = s.find(']', p);
                        const std::string idx = s.substr(p, end == std::string::npos ? std::string::npos : end - p);
                        p = end == std::string::npos ? s.size() : end + 1;
                        MathNode n{MathNode::Root, {}, {}};
                        n.slots.push_back(math_from_latex(idx));
                        n.slots.push_back(math_from_latex(group()));
                        kids.push_back(std::move(n));
                    } else {
                        MathNode n{MathNode::Sqrt, {}, {}};
                        n.slots.push_back(math_from_latex(group()));
                        kids.push_back(std::move(n));
                    }
                    continue;
                }
                MathNode::Type big = MathNode::Text;
                if (lit("\\sum")) big = MathNode::Sum;
                else if (lit("\\prod")) big = MathNode::Prod;
                else if (lit("\\int")) big = MathNode::Int;
                if (big != MathNode::Text) {
                    MathNode n{big, {}, {}};
                    n.slots.resize(3); // lower, upper, body — defaults empty
                    // limits in either order
                    for (int k = 0; k < 2; ++k) {
                        ws();
                        if (p < s.size() && (s[p] == '_' || s[p] == '^')) {
                            const bool lower = s[p] == '_';
                            ++p;
                            const std::string g = group();
                            n.slots[lower ? 0 : 1] = math_from_latex(g);
                        }
                    }
                    n.slots[2] = parse_seq(); // body until stop
                    kids.push_back(std::move(n));
                    continue;
                }
                if (lit("\\left")) {
                    ws();
                    if (p >= s.size()) break;
                    MathNode::Type t = MathNode::Text;
                    const char c = s[p];
                    if (c == '(') t = MathNode::Parens;
                    else if (c == '[') t = MathNode::Brackets;
                    else if (c == '|') t = MathNode::Abs;
                    else if (c == '\\') { ++p; if (p < s.size() && s[p] == '{') t = MathNode::Braces; }
                    if (t == MathNode::Text) { ++p; continue; } // unknown \leftX: skip
                    ++p; // consume the delimiter char
                    MathNode n{t, {}, {}};
                    // inner until \right<same>
                    std::string stop = "\\right";
                    if (t == MathNode::Parens) stop += ")";
                    else if (t == MathNode::Brackets) stop += "]";
                    else if (t == MathNode::Abs) stop += "|";
                    else stop += "\\}";
                    const size_t end = s.find(stop, p);
                    const std::string inner = s.substr(p, end == std::string::npos ? std::string::npos : end - p);
                    p = end == std::string::npos ? s.size() : end + stop.size();
                    n.slots.push_back(math_from_latex(inner));
                    kids.push_back(std::move(n));
                    continue;
                }
                if (lit("\\begin{bmatrix}")) {
                    const size_t end = s.find("\\end{bmatrix}", p);
                    const std::string body = s.substr(p, end == std::string::npos ? std::string::npos : end - p);
                    p = end == std::string::npos ? s.size() : end + 13;
                    // split rows on \\ then cells on &
                    std::vector<std::vector<std::string>> rows(1);
                    rows[0].push_back(std::string());
                    size_t q = 0;
                    while (q < body.size()) {
                        if (body[q] == '\\' && q + 1 < body.size() && body[q + 1] == '\\') {
                            rows.emplace_back();
                            rows.back().push_back(std::string());
                            q += 2;
                            continue;
                        }
                        if (body[q] == '&') { rows.back().push_back(std::string()); ++q; continue; }
                        if (body[q] == '\\') { ++q; if (q < body.size()) ++q; continue; }
                        rows.back().back() += body[q]; // back() exists: every row starts with one cell
                        ++q;
                    }
                    for (auto& r : rows) if (r.empty()) r.push_back(std::string());
                    MathNode n{MathNode::Matrix, std::to_string(rows.size()) + "x" + std::to_string(rows[0].size()), {}};
                    for (auto& r : rows) {
                        for (auto& c : r) n.slots.push_back(math_from_latex(c));
                    }
                    kids.push_back(std::move(n));
                    continue;
                }
                if (lit("\\begin{cases}")) {
                    const size_t end = s.find("\\end{cases}", p);
                    const std::string body = s.substr(p, end == std::string::npos ? std::string::npos : end - p);
                    p = end == std::string::npos ? s.size() : end + 11;
                    MathNode n{MathNode::Piecewise, {}, {}};
                    n.slots.push_back(math_from_latex(body));
                    kids.push_back(std::move(n));
                    continue;
                }
                if (lit("\\,")) continue; // thin space: cosmetic only
                if (lit("\\textbackslash{}")) { push_text("\\"); continue; }
                if (lit("\\^{ }")) { push_text("^"); continue; }
                if (lit("\\^{}")) { push_text("^"); continue; }
                // single-character escapes: \{ \} \$ \% \& \# \_
                if (p + 1 < s.size() && std::strchr("{}$%&#_", s[p + 1]) != nullptr) {
                    push_text(std::string(1, s[p + 1]));
                    p += 2;
                    continue;
                }
                // unknown command: skip the backslash and the word
                ++p;
                while (p < s.size() && ((s[p] >= 'a' && s[p] <= 'z') || (s[p] >= 'A' && s[p] <= 'Z'))) ++p;
                continue;
            }
            if (s[p] == '^' || s[p] == '_') {
                const bool lower = s[p] == '_';
                ++p;
                const std::string g = group();
                // base is the previous child
                MathNode base = kids.empty() ? MathNode{MathNode::Seq, {}, {}}
                                             : std::move(kids.back());
                if (!kids.empty()) kids.pop_back();
                MathNode::Type t;
                if (lower) {
                    t = MathNode::Sub;
                } else {
                    t = MathNode::Sup;
                }
                MathNode n{t, {}, {}};
                n.slots.push_back(std::move(base));
                n.slots.push_back(math_from_latex(g));
                kids.push_back(std::move(n));
                continue;
            }
            // raw text run
            std::string run;
            while (p < s.size() && s[p] != '\\' && s[p] != '{' && s[p] != '}'
                   && s[p] != '^' && s[p] != '_') {
                run += s[p];
                ++p;
            }
            if (!run.empty()) push_text(run);
        }
        return MathNode{MathNode::Seq, {}, std::move(kids)};
    }
};

} // namespace

MathNode math_from_latex(const std::string& latex) {
    std::string s = latex;
    // strip surrounding $$ ... $$
    while (!s.empty() && (s.front() == ' ' || s.front() == '\n')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n')) s.pop_back();
    if (s.size() >= 4 && s.compare(0, 2, "$$") == 0 && s.compare(s.size() - 2, 2, "$$") == 0) {
        s = s.substr(2, s.size() - 4);
    }
    if (s == "\\,") s.clear();
    LatexParser lp{s};
    return lp.parse_seq();
}

} // namespace md

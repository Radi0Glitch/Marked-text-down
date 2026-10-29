#include "diag_model.hpp"

#include <algorithm>

namespace md {

namespace {

// --- small string helpers --------------------------------------------------

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

size_t cstr_len(const char* p) {
    size_t n = 0;
    while (p[n]) ++n;
    return n;
}

bool starts_with(const std::string& s, const char* p) {
    const size_t n = cstr_len(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

// `w` must be a whole word: followed by nothing or by whitespace.
bool word_prefix(const std::string& s, const char* w) {
    const size_t n = cstr_len(w);
    if (s.size() < n || s.compare(0, n, w) != 0) return false;
    return s.size() == n || s[n] == ' ' || s[n] == '\t';
}

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// Characters that end a node identifier: they start shapes, arrows or labels.
bool id_stop(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n'
        || c == '-' || c == '>' || c == '<'
        || c == '[' || c == ']' || c == '(' || c == ')'
        || c == '{' || c == '}' || c == '|' || c == ':' || c == ';' || c == '=';
}

std::string parse_id(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    const size_t a = i;
    while (i < s.size() && !id_stop(s[i])) ++i;
    return s.substr(a, i - a);
}

// Arrow tokens, longest first so "-->>" wins over "->>" and "-->" over "->".
const char* const kArrows[] = {"-->>", "-.->", "->>", "-->", "==>", "---", "->"};

bool match_arrow(const std::string& s, size_t& i, std::string& out) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    for (const char* a : kArrows) {
        const size_t n = cstr_len(a);
        if (s.compare(i, n, a) == 0) {
            out = a;
            i += n;
            return true;
        }
    }
    return false;
}

size_t find_any_arrow(const std::string& s) {
    for (size_t p = 0; p < s.size(); ++p) {
        for (const char* a : kArrows) {
            if (s.compare(p, cstr_len(a), a) == 0) return p;
        }
    }
    return std::string::npos;
}

// One parsed node expression: an id and/or a shape around its label.
struct NodeRef {
    std::string id;
    bool has_shape = false;
    DiagShape shape = DiagShape::Box;
    bool has_label = false;
    std::string label;
};

// Extracts the visible label from raw shape content. Unquoted content may
// carry graphviz attributes (`label="Text" shape=box`), in which case only
// the label part is kept.
std::string label_from_content(const std::string& content) {
    const size_t at = content.find("label=");
    if (at != std::string::npos) {
        std::string v = content.substr(at + 6);
        if (!v.empty() && v.front() == '"') {
            const size_t e = v.find('"', 1);
            return e == std::string::npos ? v.substr(1) : v.substr(1, e - 1);
        }
        const size_t e = v.find_first_of(", \t");
        return e == std::string::npos ? v : v.substr(0, e);
    }
    if (content.find('=') != std::string::npos) return ""; // attributes, no label
    if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
        return content.substr(1, content.size() - 2);
    }
    return content;
}

// Parses `id`, an optional shape (`[...]`, `(...)`, `{...}`, `([..])`,
// `((..))`) or a bare shape without an id. Returns false if neither matches.
bool parse_node_expr(const std::string& s, size_t& i, NodeRef& out) {
    out = NodeRef{};
    const std::string id = parse_id(s, i);
    out.id = id;
    const size_t after_id = i;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i >= s.size() || (s[i] != '[' && s[i] != '(' && s[i] != '{')) {
        i = after_id;
        return !id.empty();
    }

    std::string open, close;
    if (s.compare(i, 2, "([") == 0) {
        out.shape = DiagShape::Stadium;
        open = "([";
        close = "])";
    } else if (s.compare(i, 2, "((") == 0) {
        out.shape = DiagShape::Ellipse;
        open = "((";
        close = "))";
    } else if (s[i] == '[') {
        out.shape = DiagShape::Box;
        open = "[";
        close = "]";
    } else if (s[i] == '(') {
        out.shape = DiagShape::Rounded;
        open = "(";
        close = ")";
    } else {
        out.shape = DiagShape::Diamond;
        open = "{";
        close = "}";
    }
    i += open.size();
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;

    out.has_shape = true;
    if (i < s.size() && s[i] == '"') {
        // A quoted label may contain brackets; it ends at the next quote.
        const size_t e = s.find('"', i + 1);
        if (e == std::string::npos) return false;
        out.has_label = true;
        out.label = s.substr(i + 1, e - i - 1);
        i = e + 1;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (s.compare(i, close.size(), close) != 0) return false;
        i += close.size();
        return true;
    }
    const size_t e = s.find(close, i);
    if (e == std::string::npos) return false;
    const std::string content = s.substr(i, e - i);
    i = e + close.size();
    const std::string label = label_from_content(content);
    // `A[]`-style empty labels count as declared; attribute lists do not.
    if (!(content.find('=') != std::string::npos && content.find("label=") == std::string::npos)) {
        out.has_label = true;
        out.label = label;
    }
    return true;
}

// Creates the node when it is new; updates label/shape when they are given.
void ensure_node(DiagModel& m, const NodeRef& r) {
    if (r.id.empty()) return;
    const int idx = diag_find_node(m, r.id);
    if (idx < 0) {
        DiagNode n;
        n.id = r.id;
        n.label = r.has_label ? r.label : r.id;
        n.shape = r.has_shape ? r.shape : DiagShape::Box;
        m.nodes.push_back(std::move(n));
        return;
    }
    if (r.has_label) m.nodes[static_cast<size_t>(idx)].label = r.label;
    if (r.has_shape) m.nodes[static_cast<size_t>(idx)].shape = r.shape;
}

std::string fresh_id(DiagModel& m, NodeRef& r) {
    if (r.id.empty()) r.id = diag_new_id(m);
    return r.id;
}

// --- lines that carry no diagram element -----------------------------------

// Comments, wrapper tags, section headers, notes and styling directives.
// Everything else must parse fully; unrecognised code makes the whole block
// non-editable instead of silently rewriting it.
bool is_noise(const std::string& t) {
    if (t.empty() || t == ";" || t == "---" || t == "{" || t == "}") return true;
    if (t[0] == '%' || t[0] == '@') return true; // %% comments, @startuml/@enduml
    const std::string low = lower(t);
    if (word_prefix(low, "sequencediagram")) return true;
    if (word_prefix(low, "graph") || word_prefix(low, "flowchart")
        || word_prefix(low, "digraph")) {
        return true;
    }
    static const char* const kNoise[] = {
        "direction ", "title ", "title:", "subtitle", "autonumber",
        "note ", "note\t", "activate ", "deactivate ",
        "classdef ", "class ", "style ", "linkstyle ", "click ",
        "subgraph ", "rect ", "legend ", "footer ",
        "loop ", "alt ", "opt ", "else ", "break ", "par ", "critical ",
        "group ", "box ",
    };
    for (const char* p : kNoise) {
        if (starts_with(low, p)) return true;
    }
    return low == "end" || low == "stop" || low == "start" || low == "title";
}

DiagKind detect_kind(const std::vector<std::string>& lines) {
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (t.empty() || t[0] == '%' || t[0] == '@' || t == "---") continue;
        const std::string low = lower(t);
        if (word_prefix(low, "sequencediagram")) return DiagKind::Sequence;
        if (word_prefix(low, "graph") || word_prefix(low, "flowchart")
            || word_prefix(low, "digraph")) {
            return DiagKind::Flowchart;
        }
    }
    // PlantUML diagrams carry no header: `Alice -> Bob: text` gives it away.
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (t.empty() || t[0] == '%' || t[0] == '@') continue;
        const size_t p = find_any_arrow(t);
        if (p == std::string::npos) continue;
        if (t.find(':', p) != std::string::npos) return DiagKind::Sequence;
    }
    return DiagKind::Flowchart;
}

// --- element lines ----------------------------------------------------------

// `A["Label"] --> B{Decision} -->|Да| C` — a declaration, an edge chain, or
// both. Returns false on anything not fully understood.
bool parse_flow_line(const std::string& t, DiagModel& m) {
    size_t i = 0;
    NodeRef first;
    if (!parse_node_expr(t, i, first)) return false;
    fresh_id(m, first);
    ensure_node(m, first);
    std::string src = first.id;

    while (true) {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
        if (i >= t.size()) break;
        if (t[i] == ';') {
            ++i;
            continue;
        }
        std::string arrow;
        const size_t before = i;
        if (!match_arrow(t, i, arrow)) return false;
        if (i == before) return false;

        // mermaid edge label: `-->|Да|`
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
        std::string label;
        if (i < t.size() && t[i] == '|') {
            const size_t e = t.find('|', i + 1);
            if (e == std::string::npos) return false;
            label = t.substr(i + 1, e - i - 1);
            i = e + 1;
        }

        NodeRef second;
        if (!parse_node_expr(t, i, second)) return false;
        fresh_id(m, second);
        ensure_node(m, second);

        // plantuml/dot style label after the target: `A -> B : text`
        size_t k = i;
        while (k < t.size() && (t[k] == ' ' || t[k] == '\t')) ++k;
        if (k < t.size() && t[k] == ':') {
            label = trim(t.substr(k + 1));
            i = t.size();
        }

        DiagEdge e;
        e.from = src;
        e.to = second.id;
        e.label = label;
        e.dashed = (arrow == "-.->");
        m.edges.push_back(std::move(e));
        src = second.id;
    }
    return true;
}

// `participant A as Имя`, `A->>B: Запрос`, `Alice -> Bob: Привет`.
bool parse_seq_line(const std::string& t, DiagModel& m) {
    size_t i = 0;
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;

    for (const char* kw : {"participant", "actor"}) {
        const size_t n = cstr_len(kw);
        if (t.compare(0, n, kw) == 0 && (t.size() == n || t[n] == ' ' || t[n] == '\t')) {
            i = n;
            NodeRef r;
            if (!parse_node_expr(t, i, r)) return false;
            fresh_id(m, r);
            ensure_node(m, r);
            const std::string id = r.id;
            while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
            if (t.compare(i, 3, "as ") == 0 || t.compare(i, 3, "as\t") == 0) {
                std::string label = trim(t.substr(i + 3));
                if (label.size() >= 2 && label.front() == '"' && label.back() == '"') {
                    label = label.substr(1, label.size() - 2);
                }
                const int idx = diag_find_node(m, id);
                if (idx >= 0) m.nodes[static_cast<size_t>(idx)].label = label;
            }
            return true;
        }
    }

    NodeRef from;
    if (!parse_node_expr(t, i, from)) return false;
    fresh_id(m, from);
    ensure_node(m, from);

    std::string arrow;
    if (!match_arrow(t, i, arrow)) return false;

    NodeRef to;
    if (!parse_node_expr(t, i, to)) return false;
    fresh_id(m, to);
    ensure_node(m, to);

    std::string label;
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) ++i;
    if (i < t.size()) {
        if (t[i] != ':') return false;
        label = trim(t.substr(i + 1));
    }

    DiagEdge e;
    e.from = from.id;
    e.to = to.id;
    e.label = label;
    e.dashed = arrow.size() >= 2 && arrow[0] == '-' && arrow[1] == '-';
    m.edges.push_back(std::move(e));
    return true;
}

// --- emitting ---------------------------------------------------------------

// Labels travel through the file only; quotes and line breaks are flattened
// so the result always reads back.
std::string clean(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

std::string node_decl(const DiagNode& n) {
    const std::string label = clean(n.label);
    switch (n.shape) {
        case DiagShape::Rounded: return "(\"" + label + "\")";
        case DiagShape::Stadium: return "([\"" + label + "\"])";
        case DiagShape::Ellipse: return "(('" + label + "'))";
        case DiagShape::Diamond: return "{\"" + label + "\"}";
        case DiagShape::Box:
        default: return "[\"" + label + "\"]";
    }
}

} // namespace

// --- public -----------------------------------------------------------------

bool is_diagram_lang(const std::string& lang) {
    return lang == "mermaid" || lang == "plantuml" || lang == "puml"
        || lang == "dot" || lang == "graphviz"
        || lang == "diagram" || lang == "diagrams";
}

DiagModel diag_new() {
    DiagModel m;
    DiagNode n;
    n.id = "n1";
    n.label = "";
    m.nodes.push_back(std::move(n));
    return m;
}

int diag_find_node(const DiagModel& model, const std::string& id) {
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        if (model.nodes[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

std::string diag_new_id(const DiagModel& model) {
    for (int k = 1;; ++k) {
        const std::string id = "n" + std::to_string(k);
        if (diag_find_node(model, id) < 0) return id;
    }
}

std::string diag_to_mermaid(const DiagModel& model) {
    std::string out;
    if (model.kind == DiagKind::Sequence) {
        out += "sequenceDiagram\n";
        for (const DiagNode& n : model.nodes) {
            out += "  participant " + n.id;
            const std::string label = clean(n.label);
            if (label != n.id) out += " as " + label;
            out += "\n";
        }
        for (const DiagEdge& e : model.edges) {
            out += "  " + e.from + (e.dashed ? "-->>" : "->>") + e.to;
            const std::string label = clean(e.label);
            if (!label.empty()) out += ": " + label;
            out += "\n";
        }
        return out;
    }

    out += model.lr ? "graph LR\n" : "graph TD\n";
    for (const DiagNode& n : model.nodes) {
        out += "  " + n.id + node_decl(n) + "\n";
    }
    for (const DiagEdge& e : model.edges) {
        std::string label = clean(e.label);
        const size_t p = label.find('|');
        if (p != std::string::npos) label[p] = '/';
        out += "  " + e.from + (e.dashed ? " -.->" : " -->");
        if (!label.empty()) out += "|" + label + "|";
        out += " " + e.to + "\n";
    }
    return out;
}

DiagModel diag_from_text(const std::string& text, bool* ok) {
    if (ok) *ok = true;
    DiagModel m;

    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t nl = text.find('\n', start);
        lines.push_back(text.substr(start, nl == std::string::npos ? nl : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    m.kind = detect_kind(lines);

    bool failed = false;
    for (const std::string& raw : lines) {
        if (failed) break;
        const std::string t = trim(raw);
        if (is_noise(t)) continue;
        if (m.kind == DiagKind::Flowchart) {
            if (!parse_flow_line(t, m)) failed = true;
        } else {
            if (!parse_seq_line(t, m)) failed = true;
        }
    }

    if (failed) {
        if (ok) *ok = false;
        return m;
    }
    // A left-to-right graph only comes from the header we emit or parse.
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (lower(t).rfind("graph lr", 0) == 0) m.lr = true;
    }
    return m;
}

} // namespace md

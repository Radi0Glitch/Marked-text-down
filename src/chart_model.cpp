#include "chart_model.hpp"

#include <algorithm>
#include <cstdlib>

namespace md {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool starts_with(const std::string& s, const char* p) {
    size_t n = 0;
    while (p[n]) ++n;
    return s.size() >= n && s.compare(0, n, p) == 0;
}

// Flattens a label for the file: quotes and line breaks would break parsing.
std::string clean(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

// The content of `[...]` or `(...)`, honouring quotes so a label may hold
// commas. Returns false when the bracket is not closed.
bool bracket_content(const std::string& s, size_t open, char close,
                     std::string& out, size_t& end) {
    std::string cur;
    bool quoted = false;
    for (size_t i = open + 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '"') quoted = !quoted;
        if (c == close && !quoted) {
            out = cur;
            end = i + 1;
            return true;
        }
        cur.push_back(c);
    }
    return false;
}

// Splits a comma-separated list, trimming and unquoting each item.
std::vector<std::string> split_items(const std::string& body) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (char c : body) {
        if (c == '"') {
            quoted = !quoted;
            continue;
        }
        if (c == ',' && !quoted) {
            out.push_back(trim(cur));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    if (!trim(cur).empty() || !out.empty()) out.push_back(trim(cur));
    out.erase(std::remove_if(out.begin(), out.end(),
                             [](const std::string& s) { return s.empty(); }),
              out.end());
    return out;
}

} // namespace

bool is_chart_text(const std::string& text) {
    size_t start = 0;
    int checked = 0;
    while (start <= text.size() && checked < 3) {
        const size_t nl = text.find('\n', start);
        const std::string t =
            lower(trim(text.substr(start, nl == std::string::npos ? nl : nl - start)));
        if (starts_with(t, "xychart") || starts_with(t, "pie")) return true;
        if (nl == std::string::npos) break;
        start = nl + 1;
        ++checked;
    }
    return false;
}

ChartModel chart_new() {
    ChartModel m;
    m.labels.push_back("");
    m.values.push_back("");
    return m;
}

double chart_value(const std::string& s) {
    std::string t = trim(s);
    for (char& c : t) {
        if (c == ',') c = '.'; // accept the comma decimal separator
    }
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str()) return 0.0;
    // Trailing junk (units, words) is ignored: the user may type "12 шт."
    return v;
}

std::string chart_to_mermaid(const ChartModel& model) {
    std::string out;
    if (model.kind == ChartKind::Pie) {
        out += "pie";
        const std::string title = clean(model.title);
        if (!title.empty()) out += " title \"" + title + "\"";
        out += "\n";
        for (size_t i = 0; i < model.labels.size(); ++i) {
            const std::string label = clean(model.labels[i]);
            const double v = i < model.values.size() ? chart_value(model.values[i]) : 0.0;
            out += "    \"" + (label.empty() ? std::string("Доля") : label) + "\" : "
                 + std::to_string(static_cast<long long>(v)) + "\n";
        }
        return out;
    }

    out += "xychart-beta\n";
    const std::string title = clean(model.title);
    if (!title.empty()) out += "    title \"" + title + "\"\n";
    out += "    x-axis [";
    for (size_t i = 0; i < model.labels.size(); ++i) {
        if (i > 0) out += ", ";
        out += "\"" + clean(model.labels[i]) + "\"";
    }
    out += "]\n    y-axis 0 --> ";
    double vmax = 0.0;
    for (size_t i = 0; i < model.values.size(); ++i) {
        vmax = std::max(vmax, chart_value(model.values[i]));
    }
    long long nice = 10;
    while (nice < static_cast<long long>(vmax) + 1) nice *= 2;
    out += std::to_string(nice) + "\n    ";
    out += model.kind == ChartKind::Line ? "line [" : "bar [";
    for (size_t i = 0; i < model.values.size(); ++i) {
        if (i > 0) out += ", ";
        const double v = chart_value(model.values[i]);
        const long long iv = static_cast<long long>(v);
        out += (v == static_cast<double>(iv)) ? std::to_string(iv)
                                              : std::to_string(v);
    }
    out += "]\n";
    return out;
}

ChartModel chart_from_text(const std::string& text, bool* ok) {
    if (ok) *ok = true;
    ChartModel m;

    size_t start = 0;
    std::vector<std::string> lines;
    while (start <= text.size()) {
        const size_t nl = text.find('\n', start);
        lines.push_back(text.substr(start, nl == std::string::npos ? nl : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    bool pie = false;
    bool header_seen = false;
    bool failed = false;
    std::string bar_body, line_body, axis_body;

    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (t.empty() || t[0] == '%') continue;
        const std::string low = lower(t);

        if (starts_with(low, "xychart")) {
            header_seen = true;
            pie = false;
            continue;
        }
        if (starts_with(low, "pie")) {
            header_seen = true;
            pie = true;
            m.kind = ChartKind::Pie;
            // `pie title "Text"` — the rest of the header line carries the title.
            size_t q = t.find('"');
            if (q != std::string::npos) {
                const size_t e = t.find('"', q + 1);
                m.title = e == std::string::npos ? t.substr(q + 1) : t.substr(q + 1, e - q - 1);
            }
            continue;
        }
        if (!header_seen) {
            failed = true;
            continue;
        }

        if (pie) {
            // "Label" : 42
            const size_t colon = t.find(':');
            if (colon == std::string::npos) {
                failed = true;
                continue;
            }
            std::string label = trim(t.substr(0, colon));
            if (label.size() >= 2 && label.front() == '"' && label.back() == '"') {
                label = label.substr(1, label.size() - 2);
            }
            const std::string val = trim(t.substr(colon + 1));
            m.labels.push_back(label);
            m.values.push_back(val);
            continue;
        }

        if (starts_with(low, "title")) {
            const size_t q = t.find('"');
            if (q != std::string::npos) {
                const size_t e = t.find('"', q + 1);
                m.title = e == std::string::npos ? t.substr(q + 1) : t.substr(q + 1, e - q - 1);
            }
            continue;
        }
        if (starts_with(low, "x-axis")) {
            const size_t open = t.find('[');
            size_t end = 0;
            std::string body;
            if (open == std::string::npos || !bracket_content(t, open, ']', body, end)) {
                failed = true;
                continue;
            }
            axis_body = body;
            continue;
        }
        if (starts_with(low, "y-axis")) continue; // the scale is recomputed
        if (starts_with(low, "bar")) {
            const size_t open = t.find('[');
            size_t end = 0;
            std::string body;
            if (open == std::string::npos || !bracket_content(t, open, ']', body, end)) {
                failed = true;
                continue;
            }
            bar_body = body;
            m.kind = ChartKind::Bar;
            continue;
        }
        if (starts_with(low, "line")) {
            const size_t open = t.find('[');
            size_t end = 0;
            std::string body;
            if (open == std::string::npos || !bracket_content(t, open, ']', body, end)) {
                failed = true;
                continue;
            }
            line_body = body;
            if (bar_body.empty()) m.kind = ChartKind::Line;
            continue;
        }
        failed = true; // unrecognised code: never rewrite what we cannot read
    }

    if (failed) {
        if (ok) *ok = false;
        return m;
    }

    if (pie) {
        if (m.labels.empty()) {
            if (ok) *ok = false;
        }
        return m;
    }

    m.labels = split_items(axis_body);
    m.values = split_items(!bar_body.empty() ? bar_body : line_body);
    // mermaid allows mismatched axis/values; pad the shorter side.
    const size_t n = std::max(m.labels.size(), m.values.size());
    if (n == 0) {
        if (ok) *ok = false;
        return m;
    }
    m.labels.resize(n);
    m.values.resize(n);
    return m;
}

} // namespace md

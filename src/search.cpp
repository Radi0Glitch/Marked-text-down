#include "search.hpp"

namespace md {

namespace {

// Lowercases ASCII and Cyrillic letters. Every mapping keeps the byte length,
// so offsets in the folded text are valid offsets in the original text.
std::string fold_case(const std::string& s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(out[i]);
        if (c >= 'A' && c <= 'Z') {
            out[i] = static_cast<char>(c + 32);
        } else if (c == 0xD0 && i + 1 < out.size()) {
            const unsigned char n = static_cast<unsigned char>(out[i + 1]);
            if (n == 0x81) {                // Ё -> ё
                out[i] = static_cast<char>(0xD1);
                out[i + 1] = static_cast<char>(0x91);
            } else if (n >= 0x90 && n <= 0x9F) { // А-П -> а-п
                out[i + 1] = static_cast<char>(n + 0x20);
            } else if (n >= 0xA0 && n <= 0xAF) { // Р-Я -> р-я
                out[i] = static_cast<char>(0xD1);
                out[i + 1] = static_cast<char>(n - 0x20);
            }
            ++i;
        }
    }
    return out;
}

void search_container(const std::vector<Inline>& inls, int block, int cell,
                      const std::string& query, std::vector<Match>& out) {
    for (int ii = 0; ii < static_cast<int>(inls.size()); ++ii) {
        const std::string text = fold_case(inls[ii].text);
        size_t pos = text.find(query);
        while (pos != std::string::npos) {
            const int b = static_cast<int>(pos);
            const int e = static_cast<int>(pos + query.size());
            out.push_back(Match{Pos{block, ii, b, cell}, Pos{block, ii, e, cell}});
            pos = text.find(query, pos + query.size());
        }
    }
}

size_t count_words_in(const std::string& text) {
    size_t words = 0;
    bool in_word = false;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = false;
        } else if (!in_word) {
            ++words;
            in_word = true;
        }
    }
    return words;
}

} // namespace

std::vector<Match> find_all(const Document& doc, const std::string& query) {
    std::vector<Match> out;
    if (query.empty()) return out;
    const std::string folded = fold_case(query);
    for (int bi = 0; bi < static_cast<int>(doc.blocks.size()); ++bi) {
        const Block& block = doc.blocks[bi];
        if (block.type == BlockType::Rule) continue;
        if (block.type == BlockType::Table) {
            for (int c = 0; c < static_cast<int>(block.cells.size()); ++c) {
                search_container(block.cells[c], bi, c, folded, out);
            }
        } else {
            search_container(block.inlines, bi, -1, folded, out);
        }
    }
    return out;
}

std::size_t count_words(const Document& doc) {
    std::size_t words = 0;
    for (const Block& block : doc.blocks) {
        if (block.type == BlockType::Rule) continue;
        if (block.type == BlockType::Table) {
            for (const auto& cell : block.cells) {
                std::string text;
                for (const Inline& inl : cell) text += inl.text;
                words += count_words_in(text);
            }
        } else {
            std::string text;
            for (const Inline& inl : block.inlines) text += inl.text;
            words += count_words_in(text);
        }
    }
    return words;
}

} // namespace md

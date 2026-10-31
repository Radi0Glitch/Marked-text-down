#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "document.hpp"
#include "layout.hpp"

namespace md {

struct Match {
    Pos begin;
    Pos end;
};

// Case-insensitive search for ASCII and Cyrillic. A match never spans two
// inline runs, so text with different styles is found only inside one run.
std::vector<Match> find_all(const Document& doc, const std::string& query);

// Number of words in the document text (tables included, markup excluded).
std::size_t count_words(const Document& doc);

} // namespace md

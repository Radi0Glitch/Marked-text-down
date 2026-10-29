#pragma once

#include <string>

#include "document.hpp"

namespace md {

// Parses Markdown into the document model. `gfm` selects GitHub Flavored
// Markdown (tables, strikethrough, task lists) instead of plain CommonMark.
// Constructs the model cannot represent are flattened, not dropped silently
// where text is involved.
bool import_markdown(const std::string& source, bool gfm, Document& out);

// Writes the document as Markdown. Strikethrough is only written when `gfm`.
std::string export_markdown(const Document& doc, bool gfm);

// True when the source uses GFM-only syntax (strikethrough, task lists or
// tables) outside fenced code blocks.
bool looks_like_gfm(const std::string& source);

// Convert all Table blocks to plain paragraphs showing pipe syntax (for CommonMark view).
void tables_to_paragraphs(Document& doc, bool gfm);
// Try to convert consecutive paragraphs that look like a pipe table back into a Table block (for GFM).
bool paragraphs_to_tables(Document& doc);

bool read_text_file(const std::string& path, std::string& out);
bool write_text_file(const std::string& path, const std::string& text);

} // namespace md

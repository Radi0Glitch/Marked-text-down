#pragma once

#include <cstddef>
#include <vector>

#include "document.hpp"
#include "layout.hpp"

namespace md {

// What an input event did to the text, so consecutive keystrokes of the same
// kind can be undone together.
enum class EditKind { Other, Typing, Deleting };

// Undo/redo by snapshots of the whole document. The owner calls commit() after
// every input event with the document and caret as they are now.
class History {
public:
    void reset(const Document& doc, Pos caret, Pos anchor);
    // Returns true when the document changed since the previous commit.
    bool commit(const Document& doc, Pos caret, Pos anchor, EditKind kind);

    // On success replaces `doc` and returns the caret and anchor to restore.
    bool undo(Document& doc, Pos& caret, Pos& anchor);
    bool redo(Document& doc, Pos& caret, Pos& anchor);

private:
    struct State {
        Document doc;
        Pos caret;
        Pos anchor;
    };

    static constexpr std::size_t kLimit = 200;

    std::vector<State> undo_;
    std::vector<State> redo_;
    State current_;
    EditKind last_kind_ = EditKind::Other;
};

} // namespace md

#include "history.hpp"

namespace md {

void History::reset(const Document& doc, Pos caret, Pos anchor) {
    undo_.clear();
    redo_.clear();
    current_ = State{doc, caret, anchor};
    last_kind_ = EditKind::Other;
}

bool History::commit(const Document& doc, Pos caret, Pos anchor, EditKind kind) {
    if (doc == current_.doc) {
        current_.caret = caret;
        current_.anchor = anchor;
        last_kind_ = kind;
        return false;
    }

    // Keystrokes of one kind in a row share one undo step. The step keeps the
    // state from before its first keystroke, so merging just skips the push.
    const bool merge = kind != EditKind::Other && kind == last_kind_ && !undo_.empty();
    if (!merge) {
        undo_.push_back(current_);
        if (undo_.size() > kLimit) undo_.erase(undo_.begin());
    }
    redo_.clear();
    current_ = State{doc, caret, anchor};
    last_kind_ = kind;
    return true;
}

bool History::undo(Document& doc, Pos& caret, Pos& anchor) {
    if (undo_.empty()) return false;
    redo_.push_back(current_);
    current_ = std::move(undo_.back());
    undo_.pop_back();
    doc = current_.doc;
    caret = current_.caret;
    anchor = current_.anchor;
    last_kind_ = EditKind::Other;
    return true;
}

bool History::redo(Document& doc, Pos& caret, Pos& anchor) {
    if (redo_.empty()) return false;
    undo_.push_back(current_);
    current_ = std::move(redo_.back());
    redo_.pop_back();
    doc = current_.doc;
    caret = current_.caret;
    anchor = current_.anchor;
    last_kind_ = EditKind::Other;
    return true;
}

} // namespace md

#pragma once

#include <string>

namespace dialogs {

// Each returns an empty string when the user cancels.
std::string open_markdown();
std::string save_markdown(const std::string& suggested_name);
std::string save_export(const std::string& suggested_name, const std::string& filter_pattern,
                        const std::string& filter_desc);

// 1 = save, 2 = discard, 0 = cancel.
int ask_save_changes(const std::string& document_name);

void show_error(const std::string& message);
void show_info(const std::string& title, const std::string& message);

} // namespace dialogs

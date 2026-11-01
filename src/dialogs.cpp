#include "dialogs.hpp"

#include "tinyfiledialogs.h"

namespace dialogs {

std::string open_markdown() {
    char const* filters[] = {"*.md", "*.markdown", "*.txt"};
    const char* path = tinyfd_openFileDialog("Открыть Markdown", "", 3, filters,
                                             "Markdown-файлы", 0);
    return path ? path : "";
}

std::string open_any_file() {
    const char* path = tinyfd_openFileDialog("Выбрать файл", "", 0, nullptr, nullptr, 0);
    return path ? path : "";
}

std::string save_markdown(const std::string& suggested_name) {
    char const* filters[] = {"*.md"};
    const char* path = tinyfd_saveFileDialog("Сохранить как", suggested_name.c_str(), 1,
                                             filters, "Markdown-файлы");
    return path ? path : "";
}

std::string save_export(const std::string& suggested_name, const std::string& filter_pattern,
                        const std::string& filter_desc) {
    char const* filters[] = {filter_pattern.c_str()};
    const char* path = tinyfd_saveFileDialog("Экспорт документа", suggested_name.c_str(), 1,
                                             filters, filter_desc.c_str());
    return path ? path : "";
}

int ask_save_changes(const std::string& document_name) {
    const std::string message = "В документе «" + document_name
        + "» есть несохранённые изменения.\n\nСохранить их?";
    return tinyfd_messageBox("Несохранённые изменения", message.c_str(),
                             "yesnocancel", "question", 1);
}

bool ask_restore_autosave() {
    return tinyfd_messageBox("Восстановление",
                             "Найдена несохранённая копия документа.\n\nВосстановить её?",
                             "yesno", "question", 1) == 1;
}

void show_error(const std::string& message) {
    tinyfd_messageBox("Marked-text-down", message.c_str(), "ok", "error", 1);
}

void show_info(const std::string& title, const std::string& message) {
    tinyfd_messageBox(title.c_str(), message.c_str(), "ok", "info", 1);
}

} // namespace dialogs

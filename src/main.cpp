#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <vector>

namespace {

struct PTemplate {
    const char* label_ru;
    const char* label_en;
    const char* lang;
    const char* text_ru;
    const char* text_en;
};

static const PTemplate kMathTemplates[] = {
    {"Дробь", "Fraction", "latex", "\\frac{a}{b}\n", "\\frac{a}{b}\n"},
    {"Квадратное уравнение", "Quadratic equation", "latex", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}\n", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}\n"},
    {"Система уравнений", "System of equations", "latex", "\\begin{cases}\nx + y = 0,\\\\\n2x - y = 3\n\\end{cases}\n", "\\begin{cases}\nx + y = 0,\\\\\n2x - y = 3\n\\end{cases}\n"},
    {"Матрица", "Matrix", "latex", "\\begin{bmatrix}\na&b\\\\\nc&d\n\\end{bmatrix}\n", "\\begin{bmatrix}\na&b\\\\\nc&d\n\\end{bmatrix}\n"},
    {"Сумма", "Sum", "latex", "\\sum_{i=1}^{n} x_i\n", "\\sum_{i=1}^{n} x_i\n"},
    {"Интеграл", "Integral", "latex", "\\int_{a}^{b} f(x)\\,dx\n", "\\int_{a}^{b} f(x)\\,dx\n"},
    {"Предел", "Limit", "latex", "\\lim_{x\\to 0}\\frac{\\sin x}{x}=1\n", "\\lim_{x\\to 0}\\frac{\\sin x}{x}=1\n"},
    {"Степень/индекс", "Power / Subscript", "latex", "x^{n},\\quad a_{i,j}\n", "x^{n},\\quad a_{i,j}\n"},
};

static const PTemplate kDiagramTemplates[] = {
    {"Схема: Процесс", "Flow: Process", "mermaid",
     "graph TD\n  A[\"Начало\"] --> B[\"Действие\"]\n  B --> C[\"Конец\"]\n",
     "graph TD\n  A[\"Start\"] --> B[\"Action\"]\n  B --> C[\"End\"]\n"},
    {"Схема: Ветвление", "Flow: Branching", "mermaid",
     "graph TD\n  A[\"Начало\"] --> B{\"Условие\"}\n  B -->|Да| C[\"Действие\"]\n  B -->|Нет| D[\"Конец\"]\n",
     "graph TD\n  A[\"Start\"] --> B{\"Condition\"}\n  B -->|Yes| C[\"Action\"]\n  B -->|No| D[\"End\"]\n"},
    {"Схема: Слева направо", "Flow: Left to Right", "mermaid",
     "graph LR\n  A[\"Идея\"] --> B[\"План\"]\n  B --> C[\"Результат\"]\n",
     "graph LR\n  A[\"Idea\"] --> B[\"Plan\"]\n  B --> C[\"Result\"]\n"},
    {"Схема: Иерархия", "Flow: Hierarchy", "mermaid",
     "graph TD\n  A[\"Раздел\"] --> B[\"Тема\"]\n  A --> C[\"Пример\"]\n  B --> D[\"Вопрос\"]\n  C --> D\n",
     "graph TD\n  A[\"Section\"] --> B[\"Topic\"]\n  A --> C[\"Example\"]\n  B --> D[\"Question\"]\n  C --> D\n"},
    {"Диалог: Последовательность", "Sequence: Dialog", "mermaid",
     "sequenceDiagram\n  participant A as Пользователь\n  participant B as Сервер\n  A->>B: Запрос\n  B-->>A: Ответ\n",
     "sequenceDiagram\n  participant A as User\n  participant B as Server\n  A->>B: Request\n  B-->>A: Response\n"},
    {"Обмен: Клиент-сервер", "Sequence: Client-Server", "mermaid",
     "sequenceDiagram\n  participant C as Клиент\n  participant S as Сервер\n  C->>S: Привет\n  S-->>C: Привет!\n  C->>S: Данные\n  S-->>C: Готово\n",
     "sequenceDiagram\n  participant C as Client\n  participant S as Server\n  C->>S: Hello\n  S-->>C: Hi!\n  C->>S: Data\n  S-->>C: Done\n"},
};

static const PTemplate kChartTemplates[] = {
    {"График: Столбчатый", "Chart: Bar", "mermaid",
     "xychart-beta\n    title \"Продажи\"\n    x-axis [\"Янв\", \"Фев\", \"Мар\", \"Апр\"]\n    y-axis 0 --> 40\n    bar [12, 18, 25, 34]\n",
     "xychart-beta\n    title \"Sales\"\n    x-axis [\"Jan\", \"Feb\", \"Mar\", \"Apr\"]\n    y-axis 0 --> 40\n    bar [12, 18, 25, 34]\n"},
    {"График: Линейный", "Chart: Line", "mermaid",
     "xychart-beta\n    title \"Динамика\"\n    x-axis [\"Пн\", \"Вт\", \"Ср\", \"Чт\", \"Пт\"]\n    y-axis 0 --> 20\n    line [4, 8, 6, 12, 16]\n",
     "xychart-beta\n    title \"Trend\"\n    x-axis [\"Mon\", \"Tue\", \"Wed\", \"Thu\", \"Fri\"]\n    y-axis 0 --> 20\n    line [4, 8, 6, 12, 16]\n"},
    {"График: Круговой", "Chart: Pie", "mermaid",
     "pie title \"Распределение\"\n    \"A\" : 40\n    \"B\" : 25\n    \"C\" : 20\n    \"D\" : 15\n",
     "pie title \"Distribution\"\n    \"A\" : 40\n    \"B\" : 25\n    \"C\" : 20\n    \"D\" : 15\n"},
};

} // namespace


#if defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "diag_model.hpp"
#include "dialogs.hpp"
#include "document.hpp"
#include "editor.hpp"
#include "fontbook.hpp"
#include "history.hpp"
#include "layout.hpp"
#include "markdown_io.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"
#include "render.hpp"
#include "ui.hpp"

#include <filesystem>

namespace {

std::string find_pandoc_path() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        std::string dir(base);
        candidates.push_back(dir + "tools/pandoc/linux-x86_64/pandoc");
        candidates.push_back(dir + "tools/pandoc/windows-x86_64/pandoc.exe");
        candidates.push_back(dir + "pandoc");
        candidates.push_back(dir + "pandoc.exe");
        candidates.push_back(dir + "../tools/pandoc/linux-x86_64/pandoc");
        candidates.push_back(dir + "../tools/pandoc/windows-x86_64/pandoc.exe");
    }
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string exe(buf);
        size_t slash = exe.find_last_of('/');
        if (slash != std::string::npos) {
            std::string dir = exe.substr(0, slash);
            candidates.push_back(dir + "/tools/pandoc/linux-x86_64/pandoc");
            candidates.push_back(dir + "/tools/pandoc/windows-x86_64/pandoc.exe");
            candidates.push_back(dir + "/pandoc");
            candidates.push_back(dir + "/pandoc.exe");
        }
    }
#endif
    candidates.push_back("tools/pandoc/linux-x86_64/pandoc");
    candidates.push_back("tools/pandoc/windows-x86_64/pandoc.exe");
    candidates.push_back("../tools/pandoc/linux-x86_64/pandoc");
    candidates.push_back("../tools/pandoc/windows-x86_64/pandoc.exe");
    candidates.push_back("pandoc");
    candidates.push_back("pandoc.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (c != "pandoc" && c != "pandoc.exe" && !std::filesystem::exists(c, ec)) {
            continue;
        }
#if defined(_WIN32)
        std::string test = "\"" + c + "\" --version > nul 2>&1";
#else
        std::string test = "\"" + c + "\" --version > /dev/null 2>&1";
#endif
        if (std::system(test.c_str()) == 0) {
            return c;
        }
    }
    return "";
}

std::string find_typst_path() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        std::string dir(base);
        candidates.push_back(dir + "tools/typst/linux-x86_64/typst");
        candidates.push_back(dir + "tools/typst/windows-x86_64/typst.exe");
        candidates.push_back(dir + "typst");
        candidates.push_back(dir + "typst.exe");
        candidates.push_back(dir + "../tools/typst/linux-x86_64/typst");
        candidates.push_back(dir + "../tools/typst/windows-x86_64/typst.exe");
    }
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string exe(buf);
        size_t slash = exe.find_last_of('/');
        if (slash != std::string::npos) {
            std::string dir = exe.substr(0, slash);
            candidates.push_back(dir + "/tools/typst/linux-x86_64/typst");
            candidates.push_back(dir + "/tools/typst/windows-x86_64/typst.exe");
            candidates.push_back(dir + "/typst");
            candidates.push_back(dir + "/typst.exe");
        }
    }
#endif
    candidates.push_back("tools/typst/linux-x86_64/typst");
    candidates.push_back("tools/typst/windows-x86_64/typst.exe");
    candidates.push_back("../tools/typst/linux-x86_64/typst");
    candidates.push_back("../tools/typst/windows-x86_64/typst.exe");
    candidates.push_back("typst");
    candidates.push_back("typst.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (c != "typst" && c != "typst.exe" && !std::filesystem::exists(std::filesystem::u8path(c), ec)) {
            continue;
        }
#if defined(_WIN32)
        std::string test = "\"" + c + "\" --version > nul 2>&1";
#else
        std::string test = "\"" + c + "\" --version > /dev/null 2>&1";
#endif
        if (std::system(test.c_str()) == 0) {
            return c;
        }
    }
    return "";
}

std::string find_fonts_dir() {
    std::vector<std::string> dirs;
    if (const char* base = SDL_GetBasePath()) {
        std::string b(base);
        dirs.push_back(b + "fonts");
        dirs.push_back(b + "../fonts");
        dirs.push_back(b + "../share/marked-text-down/fonts");
    }
    dirs.push_back("fonts");
    dirs.push_back("../fonts");
    dirs.push_back("/usr/share/marked-text-down/fonts");
    dirs.push_back("/usr/local/share/marked-text-down/fonts");
    dirs.push_back(std::string(PROJECT_SOURCE_DIR) + "/fonts");
    for (const auto& d : dirs) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::u8path(d + "/FiraSans-Regular.ttf"), ec)) {
            return d;
        }
    }
    return std::string(PROJECT_SOURCE_DIR) + "/fonts";
}

const std::vector<std::string> kDialects = {
    "CommonMark",
    "GitHub Flavored Markdown",
};

const std::vector<std::string> kFileItemsRu = {
    "Новый   Ctrl+N",
    "Открыть...   Ctrl+O",
    "Сохранить   Ctrl+S",
    "Сохранить как...   Ctrl+Shift+S",
    "О редакторе и возможностях",
};

const std::vector<std::string> kFileItemsEn = {
    "New   Ctrl+N",
    "Open...   Ctrl+O",
    "Save   Ctrl+S",
    "Save As...   Ctrl+Shift+S",
    "Welcome & Features",
};

std::string settings_file_path() {
    char* pref = SDL_GetPrefPath("MarkedTextDown", "Editor");
    if (pref) {
        std::string p = std::string(pref) + "settings.cfg";
        SDL_free(pref);
        return p;
    }
    return "marked_text_down_settings.cfg";
}

// Cuts `text` with an ellipsis so it fits in `max_w`, never splitting a UTF-8 character.
std::string fit_text(TTF_Font* font, std::string text, float max_w) {
    if (ui::text_width(font, text) <= max_w) return text;
    while (!text.empty() && ui::text_width(font, text + "...") > max_w) {
        size_t start = text.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
            --start;
        }
        text.erase(start);
    }
    return text.empty() ? std::string() : text + "...";
}

struct App {
    static constexpr float kTitleH = 36.0f;
    static constexpr float kTabH = 28.0f;
    int ctx_menu_block = -1;
    std::string ctx_menu_lang;
    float ctx_menu_x = 0, ctx_menu_y = 0;
    bool show_ctx_menu = false;
    bool formula_editor_open = false;
    bool formula_just_opened = false;              // swallow the click that opened it
    int formula_block = -1;                        // block being edited, -1 = new
    md::MathNode formula_edit = md::math_seq({});  // the formula tree
    std::vector<int> formula_path;                 // path to the current Text slot
    int formula_off = 0;                           // byte caret offset inside that Text
    std::vector<md::mathdraw::Slot> formula_slots; // hit-test rects from the last draw
    bool diag_editor_open = false;
    bool diag_just_opened = false;              // swallow the click that opened it
    int diag_block = -1;                        // block being edited, -1 = new
    md::DiagModel diag_edit;                    // the diagram being edited
    int diag_sel = -1;                          // selected node (or edge) index
    bool diag_sel_edge = false;                 // the index counts edges when set
    int diag_off = 0;                           // byte caret offset inside its label
    std::vector<md::diagdraw::Slot> diag_slots; // hit-test rects from the last draw
    bool chart_editor_open = false;
    bool chart_just_opened = false;             // swallow the click that opened it
    int chart_block = -1;                       // block being edited, -1 = new
    md::ChartModel chart_edit;
    int chart_sel = -1;                         // selected data row
    bool chart_sel_value = false;               // true when the number cell is selected
    int chart_off = 0;
    std::vector<md::chartdraw::Slot> chart_slots;
    bool pandoc_help_open = false;
    bool welcome_open = false;
    bool show_welcome_on_startup = true;
    int ui_lang = 0; // 0 = RU, 1 = EN

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    ui::Fonts fonts;
    ui::Context ui;
    md::FontBook fontbook;
    md::Document doc;
    md::Editor editor{doc};
    md::Layout layout;
    md::History history;
    float toolbar_h = 52.0f;
    ui::Rect file_rect{};
    ui::Rect dialect_rect{};
    ui::Rect theme_rect{};
    ui::Rect lang_rect{};
    ui::Rect help_rect{};
    ui::Rect caption_rects[3]{}; // minimize, maximize, close
    int tools_tab = 0;           // 0 = Главная, 1 = Таблица, 2 = Дополнения
    ui::Rect tab_rects[4]{};
    ui::Rect doc_viewport{};
    ui::Rect scrollbar_track{};
    ui::Rect scrollbar_thumb{};
    bool dragging = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    bool doc_dirty = true;
    bool light_theme = false;
    float scroll_y = 0;
    float laid_out_width = -1;
    float max_scroll = 0;

    std::string file_path; // empty until the document is opened or saved
    bool modified = false; // unsaved changes

    int dialect = 0;
    std::string status = "Готово";
    int width = 0;
    int height = 0;
    bool has_pandoc = false;
    std::string pandoc_path;
    std::string typst_path;
    std::string fonts_dir;
    std::vector<std::string> pending_drops;
    bool drop_gesture_active = false;

    bool gfm() const { return dialect == 1; }
    bool en() const { return ui_lang == 1; }
    const char* tr(const char* ru, const char* en_str) const { return en() ? en_str : ru; }

    void save_settings() {
        std::string content;
        content += "show_welcome=" + std::string(show_welcome_on_startup ? "1" : "0") + "\n";
        content += "lang=" + std::string(ui_lang == 1 ? "en" : "ru") + "\n";
        content += "theme=" + std::string(light_theme ? "light" : "dark") + "\n";
        md::write_text_file(settings_file_path(), content);
    }

    void load_settings() {
        std::string content;
        if (!md::read_text_file(settings_file_path(), content)) return;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (line == "show_welcome=0") show_welcome_on_startup = false;
            else if (line == "show_welcome=1") show_welcome_on_startup = true;
            else if (line == "lang=en") ui_lang = 1;
            else if (line == "lang=ru") ui_lang = 0;
            else if (line == "theme=light") light_theme = true;
            else if (line == "theme=dark") light_theme = false;
        }
    }

    std::string document_name() const {
        return file_path.empty() ? tr("Без названия", "Untitled")
                                 : std::filesystem::u8path(file_path).filename().u8string();
    }

    void toggle_theme() {
        light_theme = !light_theme;
        ui.palette = light_theme ? ui::light_palette() : ui::dark_palette();
        status = light_theme ? tr("Тема: Светлая", "Theme: Light")
                             : tr("Тема: Тёмная", "Theme: Dark");
        save_settings();
    }

    void toggle_lang() {
        ui_lang = ui_lang == 0 ? 1 : 0;
        status = en() ? "Language: English" : "Язык: Русский";
        update_title();
        save_settings();
    }

    void update_title() {
        SDL_SetWindowTitle(window, ((modified ? "* " : "") + document_name()
                                    + " — Marked-text-down").c_str());
    }

    void mark_edited() {
        doc_dirty = true;
        if (!modified) {
            modified = true;
            update_title();
        }
        editor.blink_tick = SDL_GetTicks();
    }

    void ensure_caret_visible() {
        if (layout.lines.empty() || doc_viewport.h <= 0.0f) return;
        float cx = 0.0f;
        const int li = md::locate_caret(doc, layout, fontbook, editor.caret(), &cx);
        if (li < 0 || li >= static_cast<int>(layout.lines.size())) return;
        const auto& ln = layout.lines[li];
        const float line_top = ln.y;
        const float line_h = ln.height > 0.0f ? ln.height : 18.0f;
        const float line_bottom = line_top + line_h;
        const float view_top = scroll_y;
        const float view_bottom = scroll_y + doc_viewport.h;
        if (line_top < view_top + 6.0f) {
            scroll_y = std::max(0.0f, line_top - 12.0f);
        } else if (line_bottom > view_bottom - 6.0f) {
            scroll_y = std::min(max_scroll, line_bottom - doc_viewport.h + 16.0f);
        }
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
    }

    void new_document() {
        if (!confirm_discard()) return;
        md::Document fresh;
        fresh.blocks.push_back(md::Block{});
        fresh.blocks[0].inlines.push_back(md::Inline{});
        doc = std::move(fresh);
        editor.set_caret(md::Pos{0, 0, 0});
        scroll_y = 0;
        file_path.clear();
        modified = false;
        doc_dirty = true;
        history.reset(doc, editor.caret(), editor.anchor());
        status = tr("Новый документ", "New document");
        update_title();
    }

    // Saves to file_path, asking for a name first when there is none.
    bool save() {
        std::string path = file_path;
        if (path.empty()) {
            path = dialogs::save_markdown("untitled.md");
            if (path.empty()) return false;
            if (std::filesystem::u8path(path).extension().empty()) path += ".md";
        }
        if (!md::write_text_file(path, md::export_markdown(doc, gfm()))) {
            dialogs::show_error(std::string(tr("Не удалось записать файл:\n", "Could not write file:\n")) + path);
            return false;
        }
        file_path = path;
        modified = false;
        status = std::string(tr("Сохранено: ", "Saved: ")) + document_name();
        update_title();
        return true;
    }

    bool save_as() {
        const std::string previous = file_path;
        file_path.clear();
        if (!save()) {
            file_path = previous;
            return false;
        }
        return true;
    }

    // Returns true when it is safe to replace or close the document.
    bool confirm_discard() {
        if (!modified) return true;
        const int answer = dialogs::ask_save_changes(document_name());
        if (answer == 0) return false;
        if (answer == 1) return save();
        return true;
    }

    enum class OpenResult { Opened, Cancelled, Error };

    static bool has_supported_markdown_ext(const std::string& path) {
        std::string ext = std::filesystem::u8path(path).extension().u8string();
        for (char& c : ext) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return ext == ".md" || ext == ".markdown" || ext == ".txt";
    }

    OpenResult open_from_path(const std::string& path, bool from_external, bool show_dialogs = true) {
        if (path.empty()) return OpenResult::Cancelled;

        std::error_code ec;
        const std::filesystem::path fspath = std::filesystem::u8path(path);
        if (!std::filesystem::exists(fspath, ec) || std::filesystem::is_directory(fspath, ec)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка открытия файла", "Error opening file");
            return OpenResult::Error;
        }

        // First verify the file is readable before asking to discard/save current work.
        std::string preview_text;
        if (!md::read_text_file(path, preview_text)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка чтения файла", "Error reading file");
            return OpenResult::Error;
        }

        if (!confirm_discard()) {
            return OpenResult::Cancelled;
        }

        // Re-read after confirm_discard() in case the user chose to save into the same file.
        std::string text;
        if (!md::read_text_file(path, text)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка чтения файла", "Error reading file");
            return OpenResult::Error;
        }

        const int detected = md::looks_like_gfm(text) ? 1 : 0;
        const bool dialect_changed = (detected != dialect);
        md::Document loaded;
        md::import_markdown(text, detected == 1, loaded);

        dialect = detected;
        if (dialect_changed && show_dialogs && !from_external) {
            dialogs::show_info(tr("Диалект изменён", "Dialect changed"),
                               en() ? ("File uses dialect \"" + kDialects[dialect] + "\".\nDialect switched automatically.")
                                    : ("Файл записан в диалекте «" + kDialects[dialect] + "».\nДиалект переключён автоматически."));
        }

        doc = std::move(loaded);
        editor.set_caret(md::Pos{0, 0, 0});
        scroll_y = 0;
        file_path = path;
        modified = false;
        doc_dirty = true;
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = false;
        pandoc_help_open = false;
        ui.open_popup.clear();
        if (from_external) {
            welcome_open = false;
        }
        history.reset(doc, editor.caret(), editor.anchor());
        status = std::string(tr("Открыто: ", "Opened: ")) + document_name() + " (" + kDialects[dialect] + ")";
        update_title();
        return OpenResult::Opened;
    }

    void open() {
        const std::string path = dialogs::open_markdown();
        if (path.empty()) return;
        open_from_path(path, false, true);
    }

    bool export_with_pandoc(const std::string& to_fmt, const std::string& out_path) {
        if (!has_pandoc || pandoc_path.empty()) return false;
        std::string temp_dir;
        if (char* pref = SDL_GetPrefPath("MarkedTextDown", "Editor")) {
            temp_dir = pref;
            SDL_free(pref);
        }
        const std::string tmp_in = temp_dir + "export_input_tmp.md";
        const std::string md_text = md::export_markdown(doc, true);
        if (!md::write_text_file(tmp_in, md_text)) {
            return false;
        }

        std::string title = document_name();
        if (title.empty()) title = "Document";
        // Strip quotes from title to keep CLI clean
        title.erase(std::remove(title.begin(), title.end(), '"'), title.end());

        std::string cmd = "\"" + pandoc_path + "\" \"" + tmp_in + "\" -f gfm -t " + to_fmt
                        + " --standalone --metadata title=\"" + title + "\"";
        if (to_fmt == "pdf") {
            if (!typst_path.empty()) {
                cmd += " --pdf-engine=\"" + typst_path + "\""
                     + " -V mainfont=\"Fira Sans\" -V monofont=\"Fira Mono\"";
                if (!fonts_dir.empty()) {
                    cmd += " --pdf-engine-opt=\"--font-path=" + fonts_dir + "\"";
                }
            }
        }
        cmd += " -o \"" + out_path + "\"";
#if defined(_WIN32)
        // Convert UTF-8 command to UTF-16 for _wsystem so Cyrillic paths and arguments work on Windows.
        std::wstring wcmd;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            wcmd.resize(static_cast<size_t>(wlen - 1));
            MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);
        }
        // _wsystem invokes cmd.exe /c which strips outer quotes if first and last chars are quotes.
        std::wstring full_wcmd = L"\"" + wcmd + L"\"";
        int rc = _wsystem(full_wcmd.c_str());
#else
        int rc = std::system(cmd.c_str());
#endif
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path(tmp_in), ec);
        return rc == 0 && std::filesystem::exists(std::filesystem::u8path(out_path), ec);
    }

    bool init() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetAppMetadata("Marked-text-down", "0.1.0", "com.marked.textdown");
        if (!TTF_Init()) {
            std::fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
            return false;
        }
        window = SDL_CreateWindow("Marked-text-down", 1100, 720,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window) {
            std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
            return false;
        }
        // The window draws its own frame and title bar; see draw_titlebar().
        SDL_SetWindowBordered(window, false);
        SDL_SetWindowMinimumSize(window, 560, 400);
        SDL_SetWindowHitTest(window, hit_test, this);
        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetRenderVSync(renderer, 1);
        {
            SDL_DisplayID did = SDL_GetDisplayForWindow(window);
            if (did != 0) {
                float sc = SDL_GetDisplayContentScale(did);
                if (sc > 0.0f && sc != 1.0f) {
                    // Some backends need texture filter set; also try render scale carefully
                }
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_StartTextInput(window);
        update_title();

        fonts_dir = find_fonts_dir();
        if (!fontbook.init(fonts_dir)) {
            std::fprintf(stderr, "Font load failed: %s\n", SDL_GetError());
            return false;
        }
        if (!TTF_FontHasGlyph(fontbook.get(md::Family::Sans, false, false, 17.0f), 0x042F)
            || !TTF_FontHasGlyph(fontbook.get(md::Family::Mono, false, false, 15.0f), 0x042F)) {
            std::fprintf(stderr, "Font has no Cyrillic glyphs\n");
            return false;
        }
        fonts.regular = fontbook.get(md::Family::Sans, false, false, 15.0f);
        fonts.bold = fontbook.get(md::Family::Sans, true, false, 15.0f);
        fonts.italic = fontbook.get(md::Family::Sans, false, true, 15.0f);
        fonts.bold_italic = fontbook.get(md::Family::Sans, true, true, 15.0f);
        fonts.strike = fontbook.get(md::Family::Sans, false, false, 15.0f, true);
        fonts.mono = fontbook.get(md::Family::Mono, false, false, 14.0f);
        doc = md::make_demo_document();
        pandoc_path = find_pandoc_path();
        if (!pandoc_path.empty()) {
            has_pandoc = true;
        }
        typst_path = find_typst_path();
        history.reset(doc, editor.caret(), editor.anchor());
        editor.blink_tick = SDL_GetTicks();

        ui.renderer = renderer;
        ui.fonts = &fonts;
        load_settings();
        ui.palette = light_theme ? ui::light_palette() : ui::dark_palette();
        welcome_open = show_welcome_on_startup;
        status = tr("Готово", "Ready");
        update_title();
        return true;
    }

    void shutdown() {
        fontbook.clear();
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }

    // Lets SDL move and resize the borderless window. Controls in the title row
    // stay clickable; the rest of the row drags the window.
    static SDL_HitTestResult hit_test(SDL_Window* win, const SDL_Point* point, void* data) {
        const App& app = *static_cast<const App*>(data);
        const float x = static_cast<float>(point->x);
        const float y = static_cast<float>(point->y);
        const float w = static_cast<float>(app.width);
        const float h = static_cast<float>(app.height);

        if (y < kTitleH) {
            const ui::Rect controls[] = {app.file_rect, app.dialect_rect, app.theme_rect,
                                         app.lang_rect, app.help_rect,
                                         app.caption_rects[0], app.caption_rects[1],
                                         app.caption_rects[2]};
            for (const ui::Rect& r : controls) {
                if (ui::contains(r, x, y)) return SDL_HITTEST_NORMAL;
            }
        }

        if (!(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)) {
            const float grip = 6.0f;
            const bool left = x < grip;
            const bool right = x >= w - grip;
            const bool top = y < grip;
            const bool bottom = y >= h - grip;
            if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
            if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
            if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
            if (left) return SDL_HITTEST_RESIZE_LEFT;
            if (right) return SDL_HITTEST_RESIZE_RIGHT;
            if (top) return SDL_HITTEST_RESIZE_TOP;
            if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
        }

        return y < kTitleH ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
    }

    void request_quit() {
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }

    // Tab strip under the title row. The table tab is locked unless the dialect
    // supports tables.
    void draw_tabs() {
        using namespace ui;
        const float h = 24.0f;
        const float y = kTitleH + (kTabH - h) / 2.0f;
        const bool tables_ok = gfm();
        if (!tables_ok && tools_tab == 1) tools_tab = 0;

        const char* labels[3] = {
            tr("Главная", "Home"),
            tr("Таблица", "Table"),
            tr("Дополнения", "Extras")
        };
        float x = 8.0f;
        for (int i = 0; i < 3; ++i) {
            bool enabled = true;
            if (i == 1) enabled = tables_ok;
            const float w = text_width(fonts.regular, labels[i]) + 36.0f;
            if (static_cast<size_t>(i) < sizeof(tab_rects) / sizeof(tab_rects[0])) {
                tab_rects[i] = Rect{x, y, w, h};
            }
            const char* ids[3] = {"tab_main", "tab_table", "tab_extra"};
            const bool clicked = tab_button(ui, ids[i], Rect{x, y, w, h},
                                            labels[i], tools_tab == i, enabled);
            if (clicked) tools_tab = i;
            x += w + 4.0f;
        }

        if (!tables_ok && width - x > 120.0f) {
            const std::string hint = fit_text(fonts.regular,
                tr("Таблицы доступны в диалекте GitHub Flavored Markdown",
                   "Tables are available in GitHub Flavored Markdown dialect"),
                static_cast<float>(width) - x - 24.0f);
            ui::draw_text(ui, fonts.regular, hint,
                      x + 12.0f, y + (h - text_height(fonts.regular)) / 2.0f, ui.palette.text_dim);
        }
    }

    // Title row: File menu on the left, document name in the middle, dialect and
    // window controls on the right. The two corner groups never share space.
    // Items flow left to right and wrap onto new rows when the window is too
    // narrow. The first pass only counts rows, so the bar height is known before
    // it is painted; the second pass draws and handles clicks.
    void draw_toolbar() {
        using namespace ui;
        const float pad = 10.0f;
        const float btn_h = 32.0f;
        const float row_gap = 6.0f;
        const float right = static_cast<float>(width) - pad;

        const float top = kTitleH + kTabH + pad;

        bool measuring = true;
        int row = 0;
        float x = pad;
        float y = top;

        auto place = [&](float w) {
            if (x > pad && x + w > right) {
                ++row;
                x = pad;
                y = top + row * (btn_h + row_gap);
            }
            const Rect r{x, y, w, btn_h};
            x += w + 6.0f;
            return r;
        };
        auto place_label = [&](const char* label, TTF_Font* font = nullptr) {
            TTF_Font* f = font ? font : fonts.regular;
            return place(std::max(34.0f, text_width(f, label) + 28.0f));
        };
        auto sep = [&]() {
            if (!measuring) ui::separator(ui, {x + 2.0f, y + 4.0f, 1.0f, btn_h - 8.0f});
            x += 12.0f;
        };

        auto body = [&]() {
            row = 0;
            x = pad;
            y = top;

            auto widget_button = [&](const char* id, const char* label, bool enabled = true) {
                const Rect r = place_label(label);
                return !measuring && ui::button(ui, id, r, label, enabled);
            };
            auto widget_toggle = [&](const char* id, const char* label, bool active,
                                     bool enabled = true, TTF_Font* font = nullptr) {
                const Rect r = place_label(label, font);
                return !measuring && ui::toggle_button(ui, id, r, label, active, enabled, font);
            };

            if (tools_tab == 1) {
                const bool in_table = editor.in_table();
                if (widget_button("tbl_insert", tr("Вставить таблицу", "Insert Table"))) {
                    editor.insert_table(3, 2);
                    mark_edited();
                }
                sep();
                if (widget_button("tbl_row_add", tr("+ строка", "+ Row"), in_table)) {
                    editor.table_add_row();
                    mark_edited();
                }
                if (widget_button("tbl_row_del", tr("- строка", "- Row"), in_table)) {
                    editor.table_remove_row();
                    mark_edited();
                }
                if (widget_button("tbl_col_add", tr("+ столбец", "+ Column"), in_table)) {
                    editor.table_add_column();
                    mark_edited();
                }
                if (widget_button("tbl_col_del", tr("- столбец", "- Column"), in_table)) {
                    editor.table_remove_column();
                    mark_edited();
                }
                sep();

                const md::TableAlign align = editor.table_align();
                auto align_button = [&](const char* id, const char* label, md::TableAlign a) {
                    if (widget_toggle(id, label, in_table && align == a, in_table)) {
                        editor.table_set_align(a);
                        mark_edited();
                    }
                };
                align_button("tbl_al_left", tr("Слева", "Left"), md::TableAlign::Left);
                align_button("tbl_al_center", tr("По центру", "Center"), md::TableAlign::Center);
                align_button("tbl_al_right", tr("Справа", "Right"), md::TableAlign::Right);
                sep();

                if (widget_button("tbl_remove", tr("Удалить таблицу", "Delete Table"), in_table)) {
                    editor.table_remove();
                    mark_edited();
                }
                return;
            }

            if (tools_tab == 2) {
                if (widget_button("vis_formula", tr("Новая формула", "New Formula"))) {
                    formula_open_new();
                }
                if (widget_button("vis_diag", tr("Новая диаграмма", "New Diagram"))) {
                    diag_open_new();
                }
                if (widget_button("vis_chart", tr("Новый график", "New Chart"))) {
                    chart_open_new();
                }
                sep();

                auto insert_template = [&](const PTemplate& t) {
                    md::Block b;
                    b.type = md::BlockType::CodeBlock;
                    b.lang = t.lang;
                    b.inlines.clear();
                    b.inlines.push_back(md::Inline{std::string(en() ? t.text_en : t.text_ru), 0, ""});
                    doc.blocks.insert(doc.blocks.begin() + editor.caret().block + 1, std::move(b));
                    doc_dirty = true;
                    mark_edited();
                    editor.set_caret(md::Pos{editor.caret().block + 1, 0, 0, -1});
                    history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                };
                {
                    std::vector<std::string> items;
                    items.reserve(sizeof(kMathTemplates) / sizeof(kMathTemplates[0]));
                    for (const auto& t : kMathTemplates) items.push_back(en() ? t.label_en : t.label_ru);
                    const char* lbl = tr("Шаблоны формул", "Formula Templates");
                    const Rect r = place_label(lbl);
                    const int ch = !measuring ? ui::menu(ui, "pandoc_math_menu", r, lbl, items, 260.0f) : -1;
                    if (ch >= 0) insert_template(kMathTemplates[ch]);
                }
                {
                    std::vector<std::string> items;
                    items.reserve(sizeof(kDiagramTemplates) / sizeof(kDiagramTemplates[0]));
                    for (const auto& t : kDiagramTemplates) items.push_back(en() ? t.label_en : t.label_ru);
                    const char* lbl = tr("Шаблоны диаграмм", "Diagram Templates");
                    const Rect r = place_label(lbl);
                    const int ch = !measuring ? ui::menu(ui, "pandoc_diag_menu", r, lbl, items, 280.0f) : -1;
                    if (ch >= 0) insert_template(kDiagramTemplates[ch]);
                }
                {
                    std::vector<std::string> items;
                    items.reserve(sizeof(kChartTemplates) / sizeof(kChartTemplates[0]));
                    for (const auto& t : kChartTemplates) items.push_back(en() ? t.label_en : t.label_ru);
                    const char* lbl = tr("Шаблоны графиков", "Chart Templates");
                    const Rect r = place_label(lbl);
                    const int ch = !measuring ? ui::menu(ui, "pandoc_chart_menu", r, lbl, items, 240.0f) : -1;
                    if (ch >= 0) insert_template(kChartTemplates[ch]);
                }
                sep();
                {
                    const char* lbl = tr("Справка о дополнениях", "Extensions Help");
                    const Rect r = place_label(lbl);
                    if (!measuring && ui::button(ui, "pandoc_help_btn", r, lbl)) {
                        pandoc_help_open = true;
                    }
                }
                sep();
                {
                    struct Fmt { const char* label; const char* ext; const char* filter; const char* to; };
                    static const Fmt fmts[] = {
                        {"DOCX", "document.docx", "*.docx", "docx"},
                        {"PDF",  "document.pdf",  "*.pdf",  "pdf"},
                        {"HTML", "document.html", "*.html", "html5"},
                        {"ODT",  "document.odt",  "*.odt",  "odt"},
                        {"EPUB", "document.epub", "*.epub", "epub"},
                    };
                    std::vector<std::string> items;
                    for (const auto& f : fmts) items.push_back(f.label);
                    const char* lbl = tr("Экспорт...", "Export...");
                    const Rect r = place_label(lbl);
                    const int ch = !measuring ? ui::menu(ui, "pandoc_export_menu", r, lbl, items, 160.0f) : -1;
                    if (ch >= 0 && ch < (int)(sizeof(fmts)/sizeof(fmts[0]))) {
                        if (has_pandoc && !pandoc_path.empty()) {
                            std::string out = dialogs::save_export(fmts[ch].ext, fmts[ch].filter, fmts[ch].label);
                            if (!out.empty()) {
                                if (export_with_pandoc(fmts[ch].to, out)) {
                                    dialogs::show_info(tr("Экспорт", "Export"), tr("Файл экспортирован", "File exported"));
                                    status = std::string(tr("Экспортировано: ", "Exported: ")) + fmts[ch].label;
                                } else {
                                    dialogs::show_error(tr("Ошибка экспорта через Pandoc", "Pandoc export failed"));
                                }
                            }
                        } else {
                            dialogs::show_error(tr("Pandoc не найден", "Pandoc not found"));
                        }
                    }
                }
                return;
            }

            // Главная / Home
            if (widget_button("undo", tr("Отменить", "Undo"))) apply_history(false);
            if (widget_button("redo", tr("Повторить", "Redo"))) apply_history(true);
            sep();

            const unsigned style = editor.active_style();
            if (widget_toggle("bold", tr("Ж", "B"), style & md::StyleBold, true, fonts.bold)) {
                editor.toggle_style(md::StyleBold);
                mark_edited();
            }
            if (widget_toggle("italic", tr("К", "I"), style & md::StyleItalic, true, fonts.italic)) {
                editor.toggle_style(md::StyleItalic);
                mark_edited();
            }
            if (widget_toggle("strike", tr("Зач.", "S"), style & md::StyleStrike, true, fonts.strike)) {
                editor.toggle_style(md::StyleStrike);
                mark_edited();
            }
            if (widget_toggle("code", tr("Код", "Code"), style & md::StyleCode, true, fonts.mono)) {
                editor.toggle_style(md::StyleCode);
                mark_edited();
            }
            sep();

            const md::Block& cb = doc.blocks[editor.caret().block];
            const int cb_level = cb.level;
            auto block_button = [&](const char* id, const char* label, bool active,
                                    md::BlockType t, int level, bool ordered) {
                if (widget_toggle(id, label, active)) {
                    if (active) {
                        editor.set_block(md::BlockType::Paragraph, 0, false);
                    } else {
                        editor.set_block(t, level, ordered);
                    }
                    mark_edited();
                    ensure_caret_visible();
                }
            };
            block_button("h1", tr("Заг. 1", "H1"), cb.type == md::BlockType::Heading && cb_level == 1,
                         md::BlockType::Heading, 1, false);
            block_button("h2", tr("Заг. 2", "H2"), cb.type == md::BlockType::Heading && cb_level == 2,
                         md::BlockType::Heading, 2, false);
            block_button("h3", tr("Заг. 3", "H3"), cb.type == md::BlockType::Heading && cb_level == 3,
                         md::BlockType::Heading, 3, false);
            sep();

            block_button("list", tr("• Список", "• List"), cb.type == md::BlockType::ListItem && !cb.ordered,
                         md::BlockType::ListItem, 0, false);
            block_button("olist", tr("1. Список", "1. List"), cb.type == md::BlockType::ListItem && cb.ordered,
                         md::BlockType::ListItem, 0, true);
            block_button("quote", tr("Цитата", "Quote"), cb.type == md::BlockType::Quote,
                         md::BlockType::Quote, 0, false);
            block_button("codeblock", tr("Блок кода", "Code Block"), cb.type == md::BlockType::CodeBlock,
                         md::BlockType::CodeBlock, 0, false);
            if (widget_button("rule", tr("Линия", "Rule"))) {
                editor.insert_rule();
                mark_edited();
            }
        };

        body();
        measuring = false;
        const int rows = row + 1;
        toolbar_h = top + rows * (btn_h + row_gap) - row_gap + pad;

        ui::fill_rect(ui, {0, 0, static_cast<float>(width), toolbar_h}, ui.palette.panel_bg);
        ui::fill_rect(ui, {0, kTitleH - 1.0f, static_cast<float>(width), 1.0f}, ui.palette.panel_border);
        ui::fill_rect(ui, {0, kTitleH + kTabH - 1.0f, static_cast<float>(width), 1.0f},
                      ui.palette.panel_border);
        ui::fill_rect(ui, {0, toolbar_h - 1.0f, static_cast<float>(width), 1.0f}, ui.palette.panel_border);
        draw_titlebar();
        draw_tabs();
        body();
    }


    void draw_titlebar() {
        using namespace ui;
        const float pad = 8.0f;
        const float h = 28.0f;
        const float y = (kTitleH - h) / 2.0f;
        const float w_px = static_cast<float>(width);
        const float cap_w = 46.0f;

        caption_rects[0] = Rect{w_px - 3.0f * cap_w, 0.0f, cap_w, kTitleH};
        caption_rects[1] = Rect{w_px - 2.0f * cap_w, 0.0f, cap_w, kTitleH};
        caption_rects[2] = Rect{w_px - cap_w, 0.0f, cap_w, kTitleH};

        const float help_w = 34.0f;
        help_rect = Rect{caption_rects[0].x - pad - help_w, y, help_w, h};

        const char* lang_label = en() ? "EN" : "RU";
        const float lang_w = 42.0f;
        lang_rect = Rect{help_rect.x - 6.0f - lang_w, y, lang_w, h};

        const char* theme_label = light_theme ? tr("Светлая", "Light") : tr("Тёмная", "Dark");
        const float theme_w = std::max(text_width(fonts.regular, tr("Светлая", "Light")),
                                       text_width(fonts.regular, tr("Тёмная", "Dark"))) + 26.0f;
        theme_rect = Rect{lang_rect.x - 6.0f - theme_w, y, theme_w, h};

        float dialect_w = 0.0f;
        for (const std::string& name : kDialects) {
            dialect_w = std::max(dialect_w, text_width(fonts.regular, name));
        }
        dialect_w += 28.0f;
        dialect_rect = Rect{theme_rect.x - 6.0f - dialect_w, y, dialect_w, h};
        const char* file_lbl = tr("Файл", "File");
        file_rect = Rect{pad, y, std::max(34.0f, text_width(fonts.regular, file_lbl) + 28.0f), h};

        const int choice = menu(ui, "file", file_rect, file_lbl,
                                en() ? kFileItemsEn : kFileItemsRu, 270.0f);
        if (choice == 0) new_document();
        if (choice == 1) open();
        if (choice == 2) save();
        if (choice == 3) save_as();
        if (choice == 4) welcome_open = true;

        if (button(ui, "theme_btn", theme_rect, theme_label)) {
            toggle_theme();
        }
        if (button(ui, "lang_btn", lang_rect, lang_label)) {
            toggle_lang();
        }
        if (button(ui, "welcome_btn", help_rect, "?")) {
            welcome_open = !welcome_open;
        }

        const int selected = dropdown(ui, "dialect", dialect_rect, kDialects, dialect);
        if (selected >= 0 && selected != dialect) {
            const int old = dialect;
            dialect = selected;
            status = std::string(tr("Диалект: ", "Dialect: ")) + kDialects[dialect];
            // When switching to CommonMark, convert tables to raw pipe paragraphs so user sees
            // they don't work. When switching to GFM, try to convert pipe paragraphs back to tables.
            if (dialect == 0 && old == 1) {
                md::tables_to_paragraphs(doc, true);
                doc_dirty = true;
                mark_edited();
                editor.set_caret(editor.caret());
                history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            } else if (dialect == 1 && old == 0) {
                if (md::paragraphs_to_tables(doc)) {
                    doc_dirty = true;
                    mark_edited();
                    editor.set_caret(md::Pos{0,0,0});
                    history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                }
            }
            // rebuild layout next frame
        }

        const float left_end = file_rect.x + file_rect.w + 12.0f;
        const float right_start = dialect_rect.x - 12.0f;
        const float avail = right_start - left_end;
        if (avail > 40.0f) {
            const std::string title = fit_text(fonts.regular,
                                               (modified ? "* " : "") + document_name(), avail);
            ui::draw_text(ui, fonts.regular, title,
                      left_end + (avail - text_width(fonts.regular, title)) / 2.0f,
                      y + (h - text_height(fonts.regular)) / 2.0f, ui.palette.text_dim);
        }

        if (button(ui, "win_min", caption_rects[0], "")) SDL_MinimizeWindow(window);
        const bool maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
        if (button(ui, "win_max", caption_rects[1], "")) {
            if (maximized) {
                SDL_RestoreWindow(window);
            } else {
                SDL_MaximizeWindow(window);
            }
        }
        if (button(ui, "win_close", caption_rects[2], "")) request_quit();

        // Icons are drawn, not typeset, so they look the same with any font.
        const float cy = kTitleH / 2.0f;
        const float min_cx = caption_rects[0].x + cap_w / 2.0f;
        const float max_cx = caption_rects[1].x + cap_w / 2.0f;
        const float close_cx = caption_rects[2].x + cap_w / 2.0f;
        ui::fill_rect(ui, {min_cx - 5.0f, cy, 10.0f, 1.0f}, ui.palette.text);
        ui::stroke_rect(ui, {max_cx - 5.0f, cy - 5.0f, 10.0f, 10.0f}, ui.palette.text);
        const float x_w = text_width(fonts.regular, "×");
        ui::draw_text(ui, fonts.regular, "×", close_cx - x_w / 2.0f,
                  cy - text_height(fonts.regular) / 2.0f, ui.palette.text);
    }

    // Items flow left to right and wrap onto new rows when the window is too
    

    bool dispatch(const SDL_Event& e) {
        md::EditKind kind = md::EditKind::Other;
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            kind = md::EditKind::Typing;
        } else if (e.type == SDL_EVENT_KEY_DOWN
                   && (e.key.key == SDLK_BACKSPACE || e.key.key == SDLK_DELETE)) {
            kind = md::EditKind::Deleting;
        }
        const bool running = handle_event(e);
        // Any change to the text marks the document as modified, including ones
        // that have no dedicated handler, such as adding a table row with Tab.
        if (history.commit(doc, editor.caret(), editor.anchor(), kind)) {
            mark_edited();
        }
        return running;
    }
void diag_select(int index, bool edge) {
        diag_sel = index;
        diag_sel_edge = edge;
        std::string* t = diag_sel_text();
        diag_off = t != nullptr ? static_cast<int>(t->size()) : 0;
    }

    void diag_open_new() {
        formula_editor_open = false;
        chart_editor_open = false;
        diag_editor_open = true;
        diag_block = -1;
        diag_edit = md::diag_new();
        diag_select(0, false);
        status = tr("Редактор диаграмм: выберите элемент и вводите текст",
                    "Diagram Editor: select an element and type text");
    }

    // Open the diagram editor for an existing block. Returns false when the
    // code has no visual form: that block is left untouched, so nothing the
    // user cannot see is ever lost.
    bool diag_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) {
            return false;
        }
        const md::Block& b = doc.blocks[block_index];
        std::string code;
        for (const md::Inline& inl : b.inlines) code += inl.text;
        // Charts have their own editor.
        if (md::is_chart_text(code)) return false;
        bool ok = true;
        md::DiagModel model = md::diag_from_text(code, &ok);
        if (!ok) {
            status = tr("Диаграмма не поддерживается визуальным редактором",
                        "Diagram is not supported by visual editor");
            return false;
        }
        formula_editor_open = false;
        chart_editor_open = false;
        diag_editor_open = true;
        diag_block = block_index;
        diag_edit = std::move(model);
        if (diag_edit.nodes.empty() && diag_edit.edges.empty()) {
            diag_edit = md::diag_new();
        }
        diag_select(0, false);
        status = tr("Редактор диаграмм: клик по элементу — выбор, ввод — текст",
                    "Diagram Editor: click element to select, type to edit label");
        return true;
    }

    void diag_add_node() {
        md::DiagNode n;
        n.id = md::diag_new_id(diag_edit);
        diag_edit.nodes.push_back(std::move(n));
        diag_select(static_cast<int>(diag_edit.nodes.size()) - 1, false);
    }

    // A new node connected to the selected (or last) one: one click builds
    // the next step of the diagram.
    void diag_add_edge() {
        if (diag_edit.nodes.empty()) diag_add_node();
        int from = -1;
        if (!diag_sel_edge && diag_sel >= 0
            && diag_sel < static_cast<int>(diag_edit.nodes.size())) {
            from = diag_sel;
        } else {
            from = static_cast<int>(diag_edit.nodes.size()) - 1;
        }
        md::DiagNode n;
        n.id = md::diag_new_id(diag_edit);
        diag_edit.nodes.push_back(std::move(n));
        const int to = static_cast<int>(diag_edit.nodes.size()) - 1;
        md::DiagEdge e;
        e.from = diag_edit.nodes[static_cast<size_t>(from)].id;
        e.to = diag_edit.nodes[static_cast<size_t>(to)].id;
        diag_edit.edges.push_back(std::move(e));
        diag_select(to, false);
    }

    // Removes the selected node (with its edges) or the selected edge.
    void diag_delete_element() {
        if (diag_sel_edge) {
            if (diag_sel >= 0 && diag_sel < static_cast<int>(diag_edit.edges.size())) {
                diag_edit.edges.erase(diag_edit.edges.begin() + diag_sel);
            }
            if (diag_sel < static_cast<int>(diag_edit.edges.size())) {
                diag_select(diag_sel, true); // the next edge slid into place
            } else if (!diag_edit.nodes.empty()) {
                diag_select(static_cast<int>(diag_edit.nodes.size()) - 1, false);
            } else {
                diag_select(-1, false);
            }
        } else {
            if (diag_sel >= 0 && diag_sel < static_cast<int>(diag_edit.nodes.size())) {
                const std::string id = diag_edit.nodes[static_cast<size_t>(diag_sel)].id;
                diag_edit.nodes.erase(diag_edit.nodes.begin() + diag_sel);
                diag_edit.edges.erase(
                    std::remove_if(diag_edit.edges.begin(), diag_edit.edges.end(),
                                   [&](const md::DiagEdge& e2) {
                                       return e2.from == id || e2.to == id;
                                   }),
                    diag_edit.edges.end());
            }
            if (diag_edit.nodes.empty() && diag_edit.edges.empty()) {
                diag_select(-1, false);
            } else if (!diag_edit.nodes.empty()) {
                diag_select(std::min(diag_sel, static_cast<int>(diag_edit.nodes.size()) - 1),
                            false);
            } else {
                diag_select(0, true);
            }
        }
        status = tr("Элемент удалён", "Element deleted");
    }

    void diag_set_shape(md::DiagShape shape) {
        if (diag_sel_edge || diag_sel < 0
            || diag_sel >= static_cast<int>(diag_edit.nodes.size())) {
            return;
        }
        diag_edit.nodes[static_cast<size_t>(diag_sel)].shape = shape;
    }

    // Typing goes straight into the selected element's label.
    void diag_insert_text(const char* sym) {
        std::string* t = diag_sel_text();
        if (t == nullptr) {
            diag_add_node();
            t = diag_sel_text();
            if (t == nullptr) return;
        }
        t->insert(static_cast<size_t>(diag_off), sym);
        diag_off += static_cast<int>(std::string(sym).size());
    }

    // Backspace deletes one utf-8 code point of the selected label.
    void diag_backspace() {
        std::string* t = diag_sel_text();
        if (t == nullptr || diag_off <= 0) return;
        size_t p = static_cast<size_t>(diag_off);
        --p;
        while (p > 0 && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) --p;
        t->erase(p, static_cast<size_t>(diag_off) - p);
        diag_off = static_cast<int>(p);
    }

    // Tab walks the elements in drawing order.
    void diag_move_slot(int dir) {
        if (diag_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < diag_slots.size(); ++i) {
            if (diag_slots[i].index == diag_sel && diag_slots[i].edge == diag_sel_edge) {
                cur = static_cast<int>(i);
                break;
            }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(diag_slots.size()) - 1;
        if (next >= static_cast<int>(diag_slots.size())) next = 0;
        const md::diagdraw::Slot& s = diag_slots[static_cast<size_t>(next)];
        diag_select(s.index, s.edge);
    }

    // Writes the picture back to the block as mermaid code — the only place
    // the code exists besides the file itself.
    void diag_apply() {
        const std::string code = md::diag_to_mermaid(diag_edit);
        if (diag_block >= 0 && diag_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[static_cast<size_t>(diag_block)];
            b.lang = "mermaid";
            b.inlines.clear();
            b.inlines.push_back(md::Inline{code, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Диаграмма обновлена", "Diagram updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "mermaid";
            b.inlines.push_back(md::Inline{code, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Диаграмма вставлена", "Diagram inserted");
        }
        doc_dirty = true;
        modified = true;
        diag_editor_open = false;
        tools_tab = 0;
    }

    void draw_diag_editor() {
        if (!diag_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(430.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  diag_block >= 0 ? tr("Редактор диаграммы", "Edit Diagram")
                                  : tr("Новая диаграмма", "New Diagram"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        const bool flow = diag_edit.kind == md::DiagKind::Flowchart;
        const bool node_sel = !diag_sel_edge && diag_sel >= 0
            && diag_sel < static_cast<int>(diag_edit.nodes.size());

        // --- element buttons (wrapping rows like the formula editor) ---
        float tx = ox + 18.0f;
        float ty = oy + 40.0f;
        auto row_button = [&](const char* id, const char* label, bool enabled,
                              const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (button(ui, id, {tx, ty, bw, 30.0f}, label, enabled)) act();
            tx += bw + 6.0f;
        };
        auto row_toggle = [&](const char* id, const char* label, bool active,
                              bool enabled, const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (toggle_button(ui, id, {tx, ty, bw, 30.0f}, label, active, enabled)) act();
            tx += bw + 6.0f;
        };

        row_button("d_node", tr("Узел", "Node"), true, [&]() { diag_add_node(); });
        row_button("d_edge", tr("Связь", "Edge"), true, [&]() { diag_add_edge(); });
        const bool shapes_on = node_sel && flow;
        row_button("d_shape_box", tr("Прямоугольник", "Rectangle"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Box); });
        row_button("d_shape_round", tr("Скруглённый", "Rounded"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Rounded); });
        row_button("d_shape_diamond", tr("Ромб", "Diamond"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Diamond); });
        row_button("d_shape_ellipse", tr("Эллипс", "Ellipse"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Ellipse); });
        row_button("d_shape_stadium", tr("Стадион", "Stadium"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Stadium); });
        ty += 34.0f;

        // --- kind and direction ---
        tx = ox + 18.0f;
        row_toggle("d_flow", tr("Схема потока", "Flowchart"), flow, true,
                   [&]() { diag_edit.kind = md::DiagKind::Flowchart; });
        row_toggle("d_seq", tr("Последовательность", "Sequence"), !flow, true,
                   [&]() { diag_edit.kind = md::DiagKind::Sequence; });
        row_toggle("d_dir",
                   flow ? (diag_edit.lr ? tr("Направление: вправо →", "Direction: Right →")
                                        : tr("Направление: вниз ↓", "Direction: Down ↓"))
                        : tr("Направление: вправо →", "Direction: Right →"),
                   diag_edit.lr, flow, [&]() { diag_edit.lr = !diag_edit.lr; });
        ty += 34.0f;

        // --- diagram canvas ---
        const float canvas_h = std::max(64.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        // Click on an element (checked against the previous frame's rects).
        if (ui.input.released && contains(canvas, ui.input.mouse_x, ui.input.mouse_y)) {
            for (const auto& slot : diag_slots) {
                if (contains(slot.rect, ui.input.mouse_x, ui.input.mouse_y)) {
                    diag_select(slot.index, slot.edge);
                    break;
                }
            }
        }

        // Render the picture; collect clickable elements.
        diag_slots.clear();
        md::diagdraw::Selection sel;
        sel.index = diag_sel;
        sel.edge = diag_sel_edge;
        md::diagdraw::render(&ui, fontbook, diag_edit,
                             canvas.x + 14.0f, canvas.y + 10.0f, canvas.w - 28.0f,
                             diag_sel >= 0 ? &sel : nullptr, &diag_slots);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по элементу — выбор, ввод — текст, Tab — следующий, Delete — удалить элемент, Esc — закрыть",
                     "Click element to select, type to edit, Tab — next, Delete — remove element, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        // --- apply / delete / cancel ---
        const float by = oy + oh - 40.0f;
        if (button(ui, "d_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   diag_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            diag_apply();
        }
        if (button(ui, "d_delete", {ox + 156.0f, by, 150.0f, 30.0f},
                   tr("Удалить элемент", "Delete Element"), diag_sel >= 0)) {
            diag_delete_element();
        }
        if (button(ui, "d_cancel", {ox + 314.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            diag_editor_open = false;
            status = tr("Редактор диаграмм закрыт", "Diagram editor closed");
        }
    }
void handle_key(const SDL_KeyboardEvent& key) {
        const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
        const bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
        using Move = md::Editor::Move;
        switch (key.key) {
            case SDLK_LEFT: editor.move(layout, fontbook, Move::Left, shift); ensure_caret_visible(); break;
            case SDLK_RIGHT: editor.move(layout, fontbook, Move::Right, shift); ensure_caret_visible(); break;
            case SDLK_UP: editor.move(layout, fontbook, Move::Up, shift); ensure_caret_visible(); break;
            case SDLK_DOWN: editor.move(layout, fontbook, Move::Down, shift); ensure_caret_visible(); break;
            case SDLK_HOME:
                editor.move(layout, fontbook, ctrl ? Move::DocStart : Move::LineStart, shift);
                ensure_caret_visible();
                break;
            case SDLK_END:
                editor.move(layout, fontbook, ctrl ? Move::DocEnd : Move::LineEnd, shift);
                ensure_caret_visible();
                break;
            case SDLK_Z:
                if (ctrl) apply_history(shift);
                break;
            case SDLK_Y:
                if (ctrl) apply_history(true);
                break;
            case SDLK_N:
                if (ctrl) new_document();
                break;
            case SDLK_S:
                if (ctrl && shift) save_as();
                else if (ctrl) save();
                break;
            case SDLK_BACKSPACE: editor.backspace(); mark_edited(); ensure_caret_visible(); break;
            case SDLK_DELETE: editor.delete_forward(); mark_edited(); ensure_caret_visible(); break;
            case SDLK_RETURN: { editor.newline(); mark_edited(); ensure_caret_visible(); break; }
            case SDLK_KP_ENTER: { editor.newline(); mark_edited(); ensure_caret_visible(); break; }
            case SDLK_A: if (ctrl) editor.select_all(); break;
            case SDLK_C: if (ctrl) editor.copy_selection(); break;
            case SDLK_X: if (ctrl) { editor.cut_selection(); mark_edited(); } break;
            case SDLK_V: if (ctrl) { editor.paste(); mark_edited(); ensure_caret_visible(); } break;
            case SDLK_B: if (ctrl) { editor.toggle_style(md::StyleBold); mark_edited(); } break;
            case SDLK_I: if (ctrl) { editor.toggle_style(md::StyleItalic); mark_edited(); } break;
            case SDLK_E:
                if (ctrl && shift) {
                    editor.set_block(md::BlockType::CodeBlock);
                    mark_edited();
                    ensure_caret_visible();
                } else if (ctrl) {
                    editor.toggle_style(md::StyleCode);
                    mark_edited();
                }
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
                if (ctrl) {
                    editor.set_block(md::BlockType::Heading,
                                     static_cast<int>(key.key - SDLK_0));
                    mark_edited();
                }
                break;
            case SDLK_L:
                if (ctrl && shift) { editor.set_block(md::BlockType::ListItem); mark_edited(); }
                break;
            case SDLK_O:
                if (ctrl && shift) {
                    editor.set_block(md::BlockType::ListItem, 0, true);
                    mark_edited();
                } else if (ctrl) {
                    open();
                }
                break;
            case SDLK_Q:
                if (ctrl && shift) { editor.set_block(md::BlockType::Quote); mark_edited(); }
                break;
            case SDLK_R:
                if (ctrl && shift) { editor.insert_rule(); mark_edited(); }
                break;
            case SDLK_TAB:
                if (editor.in_table()) {
                    editor.table_step(!shift);
                } else if (editor.indent(shift ? -1 : 1)) {
                    mark_edited();
                }
                break;
            case SDLK_M:
                if (ctrl && shift) {
                    if (!editor.in_table()) {
                        editor.insert_text("$...$");
                        md::Pos p = editor.caret();
                        if (p.offset >= 4) p.offset -= 4;
                        editor.set_caret(p);
                        history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                    }
                }
                break;
            default: break;
        }
    }

    void editor_pointer(float x, float y, bool extend) {
        editor.click(layout, fontbook, x - doc_viewport.x,
                     y - doc_viewport.y + scroll_y, extend);
        ensure_caret_visible();
    }

    void apply_history(bool redo) {
        md::Pos caret, anchor;
        const bool done = redo ? history.redo(doc, caret, anchor)
                               : history.undo(doc, caret, anchor);
        if (!done) {
            status = redo ? tr("Нечего повторять", "Nothing to redo")
                          : tr("Нечего отменять", "Nothing to undo");
            return;
        }
        editor.set_caret(anchor);
        editor.set_caret(caret, true);
        mark_edited();
        status = redo ? tr("Повторено", "Redone") : tr("Отменено", "Undone");
    }

    // Runs one event, then records what it changed for undo.


    void draw_workspace() {
        using namespace ui;
        const Rect area{0, toolbar_h, static_cast<float>(width),
                        static_cast<float>(height) - toolbar_h - 28.0f};
        ui::fill_rect(ui, area, ui.palette.window_bg);

        const float content_w = std::min(760.0f, area.w - 64.0f);
        const float content_x = area.x + (area.w - content_w) / 2.0f;
        const Rect viewport{content_x, area.y + 24.0f, content_w, area.h - 48.0f};

        if (content_w != laid_out_width || doc_dirty) {
            md::layout_document(doc, fontbook, content_w, layout);
            laid_out_width = content_w;
            doc_dirty = false;
        }
        max_scroll = std::max(0.0f, layout.content_height - viewport.h + 24.0f);
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
        doc_viewport = viewport;

        md::EditView view;
        if (editor.has_selection()) {
            const auto [s, e] = editor.selection();
            view.selecting = true;
            view.sel_start = s;
            view.sel_end = e;
        }
        view.caret_visible = (SDL_GetTicks() - editor.blink_tick) % 1060 < 530;
        view.caret = editor.caret();
        md::render_document(ui, doc, layout, fontbook, viewport, scroll_y, &view);

        if (max_scroll > 0.0f) {
            const float thumb_h = std::max(30.0f,
                viewport.h * viewport.h / (layout.content_height + 24.0f));
            const float thumb_y = viewport.y
                + (viewport.h - thumb_h) * (scroll_y / max_scroll);
            scrollbar_track = Rect{area.x + area.w - 16.0f, viewport.y, 14.0f, viewport.h};
            scrollbar_thumb = Rect{area.x + area.w - 11.0f, thumb_y, 6.0f, thumb_h};
            const bool sb_hover = scrollbar_dragging
                || contains(scrollbar_track, ui.input.mouse_x, ui.input.mouse_y);
            ui::fill_rounded_rect(ui, scrollbar_thumb, 3.0f,
                                  sb_hover ? ui.palette.accent : ui.palette.panel_border);
        } else {
            scrollbar_track = Rect{};
            scrollbar_thumb = Rect{};
        }
    }

    void draw_statusbar() {
        using namespace ui;
        const float y = static_cast<float>(height) - 28.0f;
        ui::fill_rect(ui, {0, y, static_cast<float>(width), 28.0f}, ui.palette.panel_bg);
        ui::fill_rect(ui, {0, y, static_cast<float>(width), 1.0f}, ui.palette.panel_border);
        const std::string right = kDialects[dialect] + "  ·  UTF-8";
        const float right_w = text_width(fonts.regular, right);
        const float max_w = static_cast<float>(width) - right_w - 48.0f;

        ui::draw_text(ui, fonts.regular, fit_text(fonts.regular, status, max_w), 12.0f, y + 6.0f,
                  ui.palette.text_dim);

        ui::draw_text(ui, fonts.regular, right,
                  static_cast<float>(width) - 12.0f - right_w,
                  y + 6.0f, ui.palette.text_dim);
    }

    // ---------- formula editor (tree-based) ----------

    // Open the formula editor for a new formula at the caret.
    void formula_open_new() {
        diag_editor_open = false;
        chart_editor_open = false;
        formula_editor_open = true;
        formula_block = -1;
        formula_edit = md::math_seq({});
        formula_path.clear();
        md::first_text_path(formula_edit, {}, formula_path);
        formula_off = 0;
        status = tr("Редактор формул: выберите структуру и заполняйте поля",
                    "Formula Editor: choose a structure and fill the slots");
    }

    // Open the formula editor for an existing math block.
    void formula_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) return;
        const md::Block& b = doc.blocks[block_index];
        std::string tex;
        for (const md::Inline& inl : b.inlines) tex += inl.text;
        diag_editor_open = false;
        chart_editor_open = false;
        formula_editor_open = true;
        formula_block = block_index;
        formula_edit = md::math_from_latex(tex);
        formula_path.clear();
        md::first_text_path(formula_edit, {}, formula_path);
        const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        formula_off = leaf && leaf->type == md::MathNode::Text
                          ? static_cast<int>(leaf->text.size()) : 0;
        status = tr("Редактор формул: выберите поле и вводите символы",
                    "Formula Editor: click a slot and type symbols");
    }

    static md::MathNode formula_new_structure(const std::string& key) {
        using N = md::MathNode;
        auto slot = []() { return md::math_seq({}); };
        if (key == "frac")   return md::math_frac(slot(), slot());
        if (key == "sqrt")   return md::math_sqrt(slot());
        if (key == "root")   return md::math_root(slot(), slot());
        if (key == "sup")    return md::math_sup(slot(), slot());
        if (key == "sub")    return md::math_sub(slot(), slot());
        if (key == "sum")    return md::math_sum();
        if (key == "prod")   return N{N::Prod, "", {slot(), slot(), slot()}};
        if (key == "int")    return md::math_int();
        if (key == "parens") return md::math_parens(slot());
        if (key == "brackets") return N{N::Brackets, "", {slot()}};
        if (key == "braces") return N{N::Braces, "", {slot()}};
        if (key == "abs")    return N{N::Abs, "", {slot()}};
        if (key == "matrix") return md::math_matrix(2, 2);
        if (key == "cases")  return N{N::Piecewise, "", {md::math_seq({md::math_text("")})}};
        return md::math_seq({});
    }

    // Insert a structure at the caret, splitting the current text if needed.
    void formula_insert_node(const std::string& key) {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        md::MathNode node = formula_new_structure(key);
        if (!leaf || leaf->type != md::MathNode::Text) {
            formula_edit.slots.push_back(std::move(node));
            const int idx = static_cast<int>(formula_edit.slots.size()) - 1;
            formula_path.clear();
            md::first_text_path(formula_edit.slots[static_cast<size_t>(idx)],
                                {idx}, formula_path);
            if (formula_path.empty()) formula_path = {idx};
            formula_off = 0;
            return;
        }
        const std::string before = leaf->text.substr(0, static_cast<size_t>(formula_off));
        const std::string after = leaf->text.substr(static_cast<size_t>(formula_off));

        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int slot_index = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent) return;
        if (parent->type == md::MathNode::Seq) {
            // splice: [before-text] [structure] [after-text] in place of the leaf
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            repl.push_back(std::move(node));
            if (!after.empty()) repl.push_back(md::math_text(after));
            std::vector<md::MathNode> out;
            int target_idx = slot_index;
            for (size_t i = 0; i < parent->slots.size(); ++i) {
                if (static_cast<int>(i) == slot_index) {
                    target_idx = static_cast<int>(out.size());
                    for (auto& r : repl) out.push_back(std::move(r));
                } else {
                    out.push_back(std::move(parent->slots[i]));
                }
            }
            parent->slots = std::move(out);
            std::vector<int> base = parent_path;
            base.push_back(target_idx);
            formula_path.clear();
            md::first_text_path(parent->slots[static_cast<size_t>(target_idx)], base,
                                formula_path);
            if (formula_path.empty()) formula_path = base;
        } else {
            // Current slot is a direct child of a non-Seq structure (e.g. Matrix cell or Root):
            // wrap the existing text and the new structure into a Seq so nothing is overwritten!
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            const int node_idx = static_cast<int>(repl.size());
            repl.push_back(std::move(node));
            if (!after.empty()) repl.push_back(md::math_text(after));
            parent->slots[static_cast<size_t>(slot_index)] = md::math_seq(std::move(repl));
            std::vector<int> base = parent_path;
            base.push_back(slot_index);
            base.push_back(node_idx);
            formula_path.clear();
            md::first_text_path(
                parent->slots[static_cast<size_t>(slot_index)].slots[static_cast<size_t>(node_idx)],
                base, formula_path);
            if (formula_path.empty()) formula_path = base;
        }
        formula_off = 0;
    }

    // Insert a binary/relational operator as its own separated element and open a fresh slot after it.
    void formula_insert_operator(const char* op) {
        std::string op_str = std::string(" ") + op + " ";
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (leaf && leaf->type == md::MathNode::Seq && leaf->slots.empty()) {
            leaf->slots.push_back(md::math_text(op_str));
            leaf->slots.push_back(md::math_text(""));
            formula_path.push_back(1);
            formula_off = 0;
            return;
        }
        if (!leaf || leaf->type != md::MathNode::Text || formula_path.empty()) {
            formula_edit.slots.push_back(md::math_text(op_str));
            formula_edit.slots.push_back(md::math_text(""));
            formula_path = {static_cast<int>(formula_edit.slots.size()) - 1};
            formula_off = 0;
            return;
        }
        const std::string before = leaf->text.substr(0, static_cast<size_t>(formula_off));
        const std::string after = leaf->text.substr(static_cast<size_t>(formula_off));
        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int slot_index = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent) return;
        if (parent->type == md::MathNode::Seq) {
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            repl.push_back(md::math_text(op_str));
            const int after_idx = static_cast<int>(repl.size());
            repl.push_back(md::math_text(after));
            std::vector<md::MathNode> out;
            int target_idx = slot_index;
            for (size_t i = 0; i < parent->slots.size(); ++i) {
                if (static_cast<int>(i) == slot_index) {
                    target_idx = static_cast<int>(out.size()) + after_idx;
                    for (auto& r : repl) out.push_back(std::move(r));
                } else {
                    out.push_back(std::move(parent->slots[i]));
                }
            }
            parent->slots = std::move(out);
            formula_path = parent_path;
            formula_path.push_back(target_idx);
            formula_off = 0;
        } else {
            leaf->text.insert(static_cast<size_t>(formula_off), op_str);
            formula_off += static_cast<int>(op_str.size());
        }
    }

    // Add a new empty block at the end of the current sequence (or root) so the user can continue the formula.
    void formula_add_slot_after() {
        if (formula_edit.type != md::MathNode::Seq) {
            formula_edit = md::math_seq({std::move(formula_edit)});
        }
        formula_edit.slots.push_back(md::math_text(""));
        formula_path = {static_cast<int>(formula_edit.slots.size()) - 1};
        formula_off = 0;
    }

    // Insert a single unicode symbol at the caret.
    void formula_insert_symbol(const char* sym) {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (leaf && leaf->type == md::MathNode::Seq && leaf->slots.empty()) {
            if (leaf == &formula_edit) {
                // The root stays a Seq (structures depend on it): add a text child.
                formula_edit.slots.push_back(md::math_text(sym));
                formula_path = {0};
                formula_off = static_cast<int>(std::string(sym).size());
                return;
            }
            // An empty slot row becomes a text run when the user types.
            leaf->type = md::MathNode::Text;
            leaf->text = sym;
            formula_off = static_cast<int>(std::string(sym).size());
            return;
        }
        if (!leaf || leaf->type != md::MathNode::Text) {
            formula_edit.slots.push_back(md::math_text(sym));
            const int idx = static_cast<int>(formula_edit.slots.size()) - 1;
            formula_path = {idx};
            formula_off = static_cast<int>(std::string(sym).size());
            return;
        }
        leaf->text.insert(static_cast<size_t>(formula_off), sym);
        formula_off += static_cast<int>(std::string(sym).size());
    }

    // Backspace: delete one utf-8 code point; empty structures delete themselves.
    void formula_backspace() {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (!leaf || leaf->type != md::MathNode::Text) return;
        if (formula_off > 0) {
            size_t p = static_cast<size_t>(formula_off);
            --p;
            while (p > 0 && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) --p;
            leaf->text.erase(p, static_cast<size_t>(formula_off) - p);
            formula_off = static_cast<int>(p);
            return;
        }
        if (formula_path.empty()) return;
        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int idx = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent || idx <= 0) return;
        md::MathNode& prev = parent->slots[static_cast<size_t>(idx) - 1];
        if (prev.type != md::MathNode::Text && prev.type != md::MathNode::Seq
            && md::math_is_empty(prev)) {
            parent->slots.erase(parent->slots.begin() + (idx - 1));
            return;
        }
        std::vector<int> prev_path = parent_path;
        prev_path.push_back(idx - 1);
        std::vector<int> tp;
        md::first_text_path(prev, prev_path, tp);
        if (!tp.empty()) {
            const md::MathNode* t = md::node_at_path(formula_edit, tp);
            formula_path = tp;
            formula_off = t && t->type == md::MathNode::Text
                              ? static_cast<int>(t->text.size()) : 0;
        }
    }

    // Delete the immediate element (structure or symbol run) the caret sits in,
    // never wiping out the entire formula when editing inside a nested structure.
    void formula_delete_element() {
        if (formula_edit.type != md::MathNode::Seq) {
            formula_edit = md::math_seq({});
            formula_path = {};
            formula_off = 0;
            return;
        }
        if (formula_path.empty()) return;

        // Find the innermost enclosing Seq (or structure) to remove just one block.
        int seq_depth = -1;
        int struct_depth = -1;
        for (int d = static_cast<int>(formula_path.size()) - 1; d >= 0; --d) {
            std::vector<int> sub(formula_path.begin(), formula_path.begin() + d);
            const md::MathNode* n = md::node_at_path(formula_edit, sub);
            if (!n) continue;
            if (n->type == md::MathNode::Seq) {
                if (n->slots.size() > 1 || d == 0) {
                    seq_depth = d;
                    break;
                }
            } else if (struct_depth < 0) {
                struct_depth = d;
            }
        }

        // If the caret is inside an single-element slot of a structure (like numerator of a fraction),
        // and that slot has text, clear its text first; if already empty, remove the structure itself from its parent Seq.
        if (seq_depth > 0) {
            std::vector<int> seq_path(formula_path.begin(), formula_path.begin() + seq_depth);
            const int child_idx = formula_path[static_cast<size_t>(seq_depth)];
            md::MathNode* seq_node = md::node_at_path(formula_edit, seq_path);
            if (seq_node && child_idx >= 0 && static_cast<size_t>(child_idx) < seq_node->slots.size()) {
                seq_node->slots.erase(seq_node->slots.begin() + child_idx);
                if (seq_node->slots.empty()) {
                    formula_path = seq_path;
                    formula_off = 0;
                } else {
                    const int land = std::min(child_idx, static_cast<int>(seq_node->slots.size()) - 1);
                    std::vector<int> base = seq_path;
                    base.push_back(land);
                    formula_path.clear();
                    md::first_text_path(seq_node->slots[static_cast<size_t>(land)], base, formula_path);
                    if (formula_path.empty()) formula_path = base;
                    const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
                    formula_off = leaf && leaf->type == md::MathNode::Text
                                      ? static_cast<int>(leaf->text.size()) : 0;
                }
                return;
            }
        }

        // If we are inside a structure slot that has only 1 element:
        // if that leaf has text, clear it; if it's already empty, remove the enclosing structure from its parent Seq.
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (formula_path.size() > 1 && leaf && leaf->type == md::MathNode::Text && !leaf->text.empty()) {
            leaf->text.clear();
            formula_off = 0;
            return;
        }

        // Remove top-level or enclosing structure from its parent Seq.
        int target_seq_depth = 0;
        for (int d = static_cast<int>(formula_path.size()) - 2; d >= 0; --d) {
            std::vector<int> sub(formula_path.begin(), formula_path.begin() + d);
            const md::MathNode* n = md::node_at_path(formula_edit, sub);
            if (n && n->type == md::MathNode::Seq) {
                target_seq_depth = d;
                break;
            }
        }
        std::vector<int> ppath(formula_path.begin(), formula_path.begin() + target_seq_depth);
        const int idx = formula_path[static_cast<size_t>(target_seq_depth)];
        md::MathNode* pseq = md::node_at_path(formula_edit, ppath);
        if (!pseq || idx < 0 || static_cast<size_t>(idx) >= pseq->slots.size()) return;
        pseq->slots.erase(pseq->slots.begin() + idx);
        if (pseq->slots.empty()) {
            formula_path = ppath;
            formula_off = 0;
        } else {
            const int land = std::min(idx, static_cast<int>(pseq->slots.size()) - 1);
            std::vector<int> base = ppath;
            base.push_back(land);
            formula_path.clear();
            md::first_text_path(pseq->slots[static_cast<size_t>(land)], base, formula_path);
            if (formula_path.empty()) formula_path = base;
            const md::MathNode* l = md::node_at_path(formula_edit, formula_path);
            formula_off = l && l->type == md::MathNode::Text
                              ? static_cast<int>(l->text.size()) : 0;
        }
    }

    // Move caret to next/previous slot in drawing order.
    void formula_move_slot(int dir) {
        if (formula_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < formula_slots.size(); ++i) {
            if (formula_slots[i].path == formula_path) { cur = static_cast<int>(i); break; }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(formula_slots.size()) - 1;
        if (next >= static_cast<int>(formula_slots.size())) next = 0;
        formula_path = formula_slots[static_cast<size_t>(next)].path;
        const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        formula_off = leaf && leaf->type == md::MathNode::Text
                          ? static_cast<int>(leaf->text.size()) : 0;
    }

    void formula_apply() {
        std::string inner = md::math_to_latex(formula_edit); // "$$...$$"
        if (inner.size() >= 4) inner = inner.substr(2, inner.size() - 4);
        if (inner.empty() || inner == "\\,") inner = " ";
        if (formula_block >= 0 && formula_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[formula_block];
            b.inlines.clear();
            b.inlines.push_back(md::Inline{inner, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Формула обновлена", "Formula updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "math";
            b.inlines.push_back(md::Inline{inner, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Формула вставлена", "Formula inserted");
        }
        doc_dirty = true;
        modified = true;
        formula_editor_open = false;
        tools_tab = 0;
    }

    void draw_formula_editor() {
        if (!formula_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(460.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  formula_block >= 0 ? tr("Редактор формулы", "Edit Formula")
                                     : tr("Новая формула", "New Formula"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        // --- structure buttons ---
        struct SBtn { const char* id; const char* lbl_ru; const char* lbl_en; };
        static const SBtn structs[] = {
            {"frac", "Дробь", "Frac"}, {"sqrt", "√", "√"}, {"root", "ⁿ√", "ⁿ√"}, {"sup", "xʸ", "xʸ"},
            {"sub", "xᵢ", "xᵢ"}, {"sum", "∑", "∑"}, {"prod", "∏", "∏"}, {"int", "∫", "∫"},
            {"parens", "( )", "( )"}, {"brackets", "[ ]", "[ ]"}, {"braces", "{ }", "{ }"},
            {"abs", "| |", "| |"}, {"matrix", "Матрица", "Matrix"}, {"cases", "Случ. ф.", "Cases"},
        };
        float tx = ox + 18.0f;
        float ty = oy + 38.0f;
        for (const SBtn& s : structs) {
            const char* lbl = en() ? s.lbl_en : s.lbl_ru;
            const float bw = text_width(fonts.regular, lbl) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 32.0f; }
            if (button(ui, ("fbtn_" + std::string(s.id)).c_str(),
                       {tx, ty, bw, 28.0f}, lbl)) {
                formula_insert_node(s.id);
            }
            tx += bw + 5.0f;
        }
        {
            const char* add_lbl = tr("+ блок", "+ slot");
            const float bw = text_width(fonts.regular, add_lbl) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 32.0f; }
            if (button(ui, "fbtn_add_slot", {tx, ty, bw, 28.0f}, add_lbl)) {
                formula_add_slot_after();
            }
        }
        ty += 34.0f;

        // --- operator buttons (split/create new blocks around operator) ---
        static const char* ops[] = {
            "+", "-", "=", "±", "×", "÷", "≠", "≤", "≥", "≈", "→",
        };
        tx = ox + 18.0f;
        for (const char* op : ops) {
            const float bw = text_width(fonts.regular, op) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 30.0f; }
            if (button(ui, ("fop_" + std::string(op)).c_str(),
                       {tx, ty, bw, 26.0f}, op)) {
                formula_insert_operator(op);
            }
            tx += bw + 5.0f;
        }
        ui::separator(ui, {tx + 2.0f, ty + 3.0f, 1.0f, 20.0f});
        tx += 10.0f;

        // --- symbol buttons ---
        static const char* syms[] = {
            "α","β","γ","δ","θ","λ","μ","π","σ","φ","ω","∞","…",
        };
        for (const char* sym : syms) {
            const float bw = text_width(fonts.regular, sym) + 16.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 30.0f; }
            if (button(ui, ("fsym_" + std::string(sym)).c_str(),
                       {tx, ty, bw, 26.0f}, sym)) {
                formula_insert_symbol(sym);
            }
            tx += bw + 5.0f;
        }
        ty += 32.0f;

        // --- formula canvas ---
        const float canvas_h = std::max(64.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        // Click on a slot (checked against the previous frame's rects).
        if (ui.input.released &&
            canvas.x <= ui.input.mouse_x && ui.input.mouse_x <= canvas.x + canvas.w &&
            canvas.y <= ui.input.mouse_y && ui.input.mouse_y <= canvas.y + canvas.h) {
            for (const auto& slot : formula_slots) {
                if (slot.rect.x <= ui.input.mouse_x && ui.input.mouse_x <= slot.rect.x + slot.rect.w &&
                    slot.rect.y <= ui.input.mouse_y && ui.input.mouse_y <= slot.rect.y + slot.rect.h) {
                    formula_path = slot.path;
                    const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
                    formula_off = leaf && leaf->type == md::MathNode::Text
                                      ? static_cast<int>(leaf->text.size()) : 0;
                    break;
                }
            }
        }

        // render the tree; collect clickable slots
        formula_slots.clear();
        md::mathdraw::render(&ui, fontbook, formula_edit,
                         canvas.x + 14.0f, canvas.y + 10.0f, 20.0f,
                         &formula_path, &formula_slots);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по полю — выбор, Tab — следующее, Delete — удалить элемент, Esc — закрыть",
                     "Click slot to select, Tab — next, Delete — remove element, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        // --- apply / delete / cancel ---
        const float by = oy + oh - 40.0f;
        if (button(ui, "f_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   formula_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            formula_apply();
        }
        if (button(ui, "f_delete", {ox + 156.0f, by, 150.0f, 30.0f}, tr("Удалить элемент", "Delete Element"))) {
            formula_delete_element();
        }
        if (button(ui, "f_cancel", {ox + 314.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            formula_editor_open = false;
            status = tr("Редактор формул закрыт", "Formula editor closed");
        }
    }

    // ---------- chart editor ----------

    std::string* chart_sel_text() {
        if (chart_sel < 0) return nullptr;
        if (chart_sel_value) {
            return chart_sel < static_cast<int>(chart_edit.values.size())
                ? &chart_edit.values[static_cast<size_t>(chart_sel)]
                : nullptr;
        }
        return chart_sel < static_cast<int>(chart_edit.labels.size())
            ? &chart_edit.labels[static_cast<size_t>(chart_sel)]
            : nullptr;
    }

    void chart_select(int index, bool value) {
        chart_sel = index;
        chart_sel_value = value;
        std::string* t = chart_sel_text();
        chart_off = t != nullptr ? static_cast<int>(t->size()) : 0;
    }

    void chart_open_new() {
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = true;
        chart_block = -1;
        chart_edit = md::chart_new();
        chart_select(0, false);
        status = tr("Редактор графика: выберите ячейку и вводите текст",
                    "Chart Editor: select a cell and type text");
    }

    bool chart_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) {
            return false;
        }
        const md::Block& b = doc.blocks[block_index];
        std::string code;
        for (const md::Inline& inl : b.inlines) code += inl.text;
        if (!md::is_chart_text(code)) return false;
        bool ok = true;
        md::ChartModel model = md::chart_from_text(code, &ok);
        if (!ok) {
            status = tr("График не поддерживается визуальным редактором",
                        "Chart is not supported by visual editor");
            return false;
        }
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = true;
        chart_block = block_index;
        chart_edit = std::move(model);
        if (chart_edit.labels.empty()) chart_edit = md::chart_new();
        chart_select(0, false);
        status = tr("Редактор графика: клик по элементу — выбор, ввод — текст",
                    "Chart Editor: click element to select, type to edit");
        return true;
    }

    static constexpr size_t kMaxChartRows = 12;

    void chart_add_row() {
        if (chart_edit.labels.size() >= kMaxChartRows) {
            status = tr("Достигнуто максимальное число рядов (12)",
                        "Maximum number of rows reached (12)");
            return;
        }
        chart_edit.labels.push_back("");
        chart_edit.values.push_back("");
        chart_select(static_cast<int>(chart_edit.labels.size()) - 1, false);
    }

    void chart_delete_row() {
        if (chart_edit.labels.empty()) return;
        if (chart_sel < 0 || chart_sel >= static_cast<int>(chart_edit.labels.size())) {
            chart_select(0, false);
        }
        if (chart_edit.labels.size() == 1) {
            chart_edit.labels[0].clear();
            chart_edit.values[0].clear();
            chart_select(0, false);
            status = tr("Ряд очищен", "Row cleared");
            return;
        }
        const int idx = std::clamp(chart_sel, 0, static_cast<int>(chart_edit.labels.size()) - 1);
        chart_edit.labels.erase(chart_edit.labels.begin() + idx);
        if (idx < static_cast<int>(chart_edit.values.size())) {
            chart_edit.values.erase(chart_edit.values.begin() + idx);
        }
        if (chart_edit.labels.empty()) {
            chart_edit.labels.push_back("");
            chart_edit.values.push_back("");
        }
        chart_select(std::min(idx, static_cast<int>(chart_edit.labels.size()) - 1), false);
        status = tr("Ряд удалён", "Row deleted");
    }

    void chart_insert_text(const char* sym) {
        std::string* t = chart_sel_text();
        if (t == nullptr) {
            if (chart_edit.labels.empty()) chart_add_row();
            chart_select(0, false);
            t = chart_sel_text();
            if (t == nullptr) return;
        }
        t->insert(static_cast<size_t>(chart_off), sym);
        chart_off += static_cast<int>(std::string(sym).size());
    }

    void chart_backspace() {
        std::string* t = chart_sel_text();
        if (t == nullptr) {
            chart_delete_row();
            return;
        }
        if (t->empty()) {
            chart_delete_row();
            return;
        }
        if (chart_off <= 0) {
            chart_delete_row();
            return;
        }
        size_t p = static_cast<size_t>(chart_off);
        --p;
        while (p > 0 && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) --p;
        t->erase(p, static_cast<size_t>(chart_off) - p);
        chart_off = static_cast<int>(p);
    }

    void chart_move_slot(int dir) {
        if (chart_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < chart_slots.size(); ++i) {
            if (chart_slots[i].index == chart_sel && chart_slots[i].value == chart_sel_value) {
                cur = static_cast<int>(i);
                break;
            }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(chart_slots.size()) - 1;
        if (next >= static_cast<int>(chart_slots.size())) next = 0;
        const md::chartdraw::Slot& s = chart_slots[static_cast<size_t>(next)];
        chart_select(s.index, s.value);
    }

    void chart_apply() {
        const std::string code = md::chart_to_mermaid(chart_edit);
        if (chart_block >= 0 && chart_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[static_cast<size_t>(chart_block)];
            b.lang = "mermaid";
            b.inlines.clear();
            b.inlines.push_back(md::Inline{code, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("График обновлён", "Chart updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "mermaid";
            b.inlines.push_back(md::Inline{code, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("График вставлен", "Chart inserted");
        }
        doc_dirty = true;
        modified = true;
        chart_editor_open = false;
        tools_tab = 0;
    }

    void draw_chart_editor() {
        if (!chart_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(470.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  chart_block >= 0 ? tr("Редактор графика", "Edit Chart")
                                   : tr("Новый график", "New Chart"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        const bool pie = chart_edit.kind == md::ChartKind::Pie;

        float tx = ox + 18.0f;
        float ty = oy + 40.0f;
        auto row_button = [&](const char* id, const char* label, bool enabled,
                              const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (button(ui, id, {tx, ty, bw, 30.0f}, label, enabled)) act();
            tx += bw + 6.0f;
        };
        auto row_toggle = [&](const char* id, const char* label, bool active,
                              bool enabled, const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (toggle_button(ui, id, {tx, ty, bw, 30.0f}, label, active, enabled)) act();
            tx += bw + 6.0f;
        };

        row_toggle("ch_bar", tr("Столбчатый", "Bar"), chart_edit.kind == md::ChartKind::Bar, true,
                   [&]() { chart_edit.kind = md::ChartKind::Bar; });
        row_toggle("ch_line", tr("Линейный", "Line"), chart_edit.kind == md::ChartKind::Line, true,
                   [&]() { chart_edit.kind = md::ChartKind::Line; });
        row_toggle("ch_pie", tr("Круговой", "Pie"), pie, true,
                   [&]() { chart_edit.kind = md::ChartKind::Pie; });
        ty += 34.0f;

        tx = ox + 18.0f;
        row_button("ch_add", tr("Добавить ряд", "Add Row"), true, [&]() { chart_add_row(); });
        row_button("ch_del", tr("Удалить ряд", "Delete Row"), true, [&]() { chart_delete_row(); });
        ty += 34.0f;

        // --- chart canvas (clipped so long labels/legends never bleed over buttons) ---
        const float canvas_h = std::max(80.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        if (ui.input.released && contains(canvas, ui.input.mouse_x, ui.input.mouse_y)) {
            for (const auto& slot : chart_slots) {
                if (contains(slot.rect, ui.input.mouse_x, ui.input.mouse_y)) {
                    chart_select(slot.index, slot.value);
                    break;
                }
            }
        }

        const SDL_Rect clip_rect{static_cast<int>(canvas.x + 1), static_cast<int>(canvas.y + 1),
                                 static_cast<int>(canvas.w - 2), static_cast<int>(canvas.h - 20)};
        SDL_SetRenderClipRect(ui.renderer, &clip_rect);
        chart_slots.clear();
        md::chartdraw::Selection sel;
        sel.index = chart_sel;
        sel.value = chart_sel_value;
        md::chartdraw::render(&ui, fontbook, chart_edit,
                              canvas.x + 14.0f, canvas.y + 6.0f, canvas.w - 28.0f,
                              chart_sel >= 0 ? &sel : nullptr, &chart_slots);
        SDL_SetRenderClipRect(ui.renderer, nullptr);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по категории/значению — выбор, ввод — текст, Tab — следующий, Delete — удалить/очистить ряд, Esc — закрыть",
                     "Click category/value to select, type to edit, Tab — next, Delete — remove/clear row, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        const float by = oy + oh - 40.0f;
        if (button(ui, "ch_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   chart_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            chart_apply();
        }
        if (button(ui, "ch_cancel", {ox + 156.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            chart_editor_open = false;
            status = tr("Редактор графика закрыт", "Chart editor closed");
        }
    }

    // ---------- diagram editor ----------

    // Label of the selected element; nullptr when nothing is selected.
    std::string* diag_sel_text() {
        if (diag_sel < 0) return nullptr;
        if (diag_sel_edge) {
            return diag_sel < static_cast<int>(diag_edit.edges.size())
                ? &diag_edit.edges[static_cast<size_t>(diag_sel)].label
                : nullptr;
        }
        return diag_sel < static_cast<int>(diag_edit.nodes.size())
            ? &diag_edit.nodes[static_cast<size_t>(diag_sel)].label
            : nullptr;
    }



    void frame() {
        SDL_GetRenderOutputSize(renderer, &width, &height);
        SDL_SetRenderDrawColor(renderer, ui.palette.window_bg.r, ui.palette.window_bg.g,
                               ui.palette.window_bg.b, ui.palette.window_bg.a);
        SDL_RenderClear(renderer);

        ui::begin_frame(ui);
        draw_toolbar();
        draw_workspace();
        draw_statusbar();
        draw_formula_editor();
        draw_diag_editor();
        draw_chart_editor();
        auto wrap_lines = [&](TTF_Font* fnt, const std::string& s, float maxw) {
            std::vector<std::string> out;
            std::string cur;
            std::string word;
            for (char c : s) {
                if (c == ' ' || c == '\t') {
                    if (!word.empty()) {
                        std::string cand = cur.empty() ? word : cur + " " + word;
                        if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                            out.push_back(cur);
                            cur = word;
                        } else {
                            cur = cand;
                        }
                        word.clear();
                    }
                    continue;
                }
                if (c == '\n') {
                    if (!word.empty()) {
                        std::string cand = cur.empty() ? word : cur + " " + word;
                        if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                            out.push_back(cur);
                            cur = word;
                        } else {
                            cur = cand;
                        }
                        word.clear();
                    }
                    if (!cur.empty()) out.push_back(cur);
                    cur.clear();
                    continue;
                }
                word += c;
            }
            if (!word.empty()) {
                std::string cand = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                    out.push_back(cur);
                    cur = word;
                } else {
                    cur = cand;
                }
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        };
        if (pandoc_help_open) {
            using namespace ui;
            const float w = 580.0f;
            const float h = 380.0f;
            const Rect bg{(static_cast<float>(width)-w)/2.0f, (static_cast<float>(height)-h)/2.0f, w, h};
            ui::fill_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                          SDL_Color{0, 0, 0, 140});
            ui::fill_rounded_rect(ui, bg, 8.0f, ui.palette.panel_bg);
            ui::stroke_rect(ui, bg, ui.palette.panel_border);
            ui::draw_text(ui, fonts.bold,
                          tr("Дополнения Pandoc", "Pandoc Extensions"),
                          bg.x+16.0f, bg.y+10.0f, ui.palette.text);
            if (ui::button(ui, "ph_close", Rect{bg.x+w-96.0f, bg.y+8.0f, 80.0f, 26.0f},
                           tr("Закрыть", "Close"))) {
                pandoc_help_open = false;
            }
            float y = bg.y + 46.0f;
            {
                std::string txt = tr(
                    "Эти меню добавляют блоки (формулы, диаграммы, графики) для вставки. При экспорте через Pandoc поддержка зависит от целевого формата и движка. Разные сайты и программы могут поддерживать разный набор расширений.",
                    "These menus insert visual blocks (formulas, diagrams, charts). When exporting via Pandoc, support depends on the target format and engine. Different websites and apps support different Markdown extensions.");
                auto lines = wrap_lines(fonts.regular, txt, w - 32.0f);
                for (auto& l : lines) {
                    ui::draw_text(ui, fonts.regular, l, bg.x+16.0f, y, ui.palette.text);
                    y += 18.0f;
                }
                y += 6.0f;
            }
            const char* head[] = {tr("Дополнение", "Extension"), "GitHub", "GitLab", "Notion", "Obsidian", "Reddit"};
            float xh = bg.x+16.0f;
            for (int i=0;i<6;++i) {
                ui::draw_text(ui, fonts.regular, head[i], xh, y, ui.palette.text_dim);
                xh += (i==0?200.0f:72.0f);
            }
            y += 20.0f;
            const char* rows[][6] = {
                {"LaTeX/Math", "+", "+", "+", "+", "-"},
                {tr("Mermaid (диаграммы)", "Mermaid (diagrams)"), "+", "+", "+", "+", "-"},
                {tr("XYChart/Pie (графики)", "XYChart/Pie (charts)"), "-", "-", "-", "+/-", "-"},
                {tr("GFM-таблицы", "GFM tables"), "+", "+", "+", "+", "+"},
            };
            for (int r=0;r<4;++r) {
                float xx = bg.x+16.0f;
                for (int c=0;c<6;++c) {
                    ui::draw_text(ui, fonts.regular, rows[r][c], xx, y, ui.palette.text);
                    xx += (c==0?200.0f:72.0f);
                }
                y += 18.0f;
            }
            y += 10.0f;
            {
                std::string note = tr(
                    "+ — обычно поддерживается, +/- — зависит от сайта/приложения, - — не поддерживается. Поддержка в браузерных MD-рендерах отличается от экспорта через Pandoc.",
                    "+ — widely supported, +/- — depends on site/plugins, - — unsupported. Browser Markdown renderers differ from Pandoc export.");
                auto lines = wrap_lines(fonts.regular, note, w - 32.0f);
                for (auto& l : lines) {
                    ui::draw_text(ui, fonts.regular, l, bg.x+16.0f, y, ui.palette.text_dim);
                    y += 16.0f;
                }
            }
        }
        if (welcome_open) {
            using namespace ui;
            const float w = std::min(680.0f, static_cast<float>(width) - 32.0f);
            const float h = std::min(510.0f, static_cast<float>(height) - 32.0f);
            const Rect bg{(static_cast<float>(width) - w) / 2.0f,
                          (static_cast<float>(height) - h) / 2.0f, w, h};
            ui::fill_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                          SDL_Color{0, 0, 0, 145});
            ui::fill_rounded_rect(ui, bg, 10.0f, ui.palette.panel_bg);
            ui::stroke_rect(ui, bg, ui.palette.panel_border);

            ui::draw_text(ui, fonts.bold,
                          tr("Добро пожаловать в Marked-text-down",
                             "Welcome to Marked-text-down"),
                          bg.x + 20.0f, bg.y + 14.0f, ui.palette.text);
            if (ui::button(ui, "wel_lang", Rect{bg.x + w - 150.0f, bg.y + 10.0f, 48.0f, 28.0f},
                           en() ? "RU" : "EN")) {
                toggle_lang();
            }
            if (ui::button(ui, "wel_close_top", Rect{bg.x + w - 94.0f, bg.y + 10.0f, 76.0f, 28.0f},
                           tr("Закрыть", "Close"))) {
                welcome_open = false;
            }

            float y = bg.y + 50.0f;
            {
                std::string intro = tr(
                    "Лёгкий визуальный (WYSIWYG) редактор Markdown. Вы работаете с живым отформатированным документом без служебной разметки на экране, а на диске сохраняется чистый .md-файл.",
                    "A lightweight visual (WYSIWYG) Markdown editor. You work directly with formatted text and visual objects without raw code on screen, while saving clean standard .md files.");
                for (const auto& l : wrap_lines(fonts.regular, intro, w - 40.0f)) {
                    ui::draw_text(ui, fonts.regular, l, bg.x + 20.0f, y, ui.palette.text);
                    y += 19.0f;
                }
                y += 8.0f;
            }

            struct Feature {
                const char* title_ru;
                const char* title_en;
                const char* desc_ru;
                const char* desc_en;
            };
            static const Feature feats[] = {
                {"Живое форматирование и стили",
                 "Live Formatting & Styles",
                 "Жирный (Ctrl+B), курсив (Ctrl+I), зачёркнутый и моноширинный код (Ctrl+E), заголовки H1–H3 (Ctrl+1..3), списки, цитаты и разделители.",
                 "Bold (Ctrl+B), italic (Ctrl+I), strikethrough, inline code (Ctrl+E), headings H1–H3 (Ctrl+1..3), bullet/numbered lists, quotes, and rules."},
                {"Визуальные формулы, диаграммы и графики",
                 "Visual Formulas, Diagrams & Charts",
                 "Во вкладке «Дополнения» создавайте математические формулы, блок-схемы, диаграммы последовательности и графики (столбчатые, линейные, круговые). Клик по объекту открывает визуальный конструктор.",
                 "In the 'Extras' tab, build math formulas, flowcharts, sequence diagrams, and charts (bar, line, pie). Click any object in the document to reopen its visual editor."},
                {"Таблицы GFM и автоопределение диалекта",
                 "GFM Tables & Dialect Detection",
                 "В режиме GitHub Flavored Markdown создавайте таблицы, управляйте строками, столбцами и выравниванием (Tab — переход по ячейкам). При открытии файла диалект определяется автоматически.",
                 "In GitHub Flavored Markdown mode, insert tables, add/remove rows and columns, and align cells (Tab moves between cells). Dialect is detected automatically on open."},
                {"Экспорт через Pandoc и темы оформления",
                 "Pandoc Export & Themes",
                 "Экспортируйте документ в DOCX, PDF, HTML, ODT и EPUB в один клик. Переключайте тёмную/светлую тему и язык интерфейса (RU/EN) прямо в верхней панели.",
                 "Export documents to DOCX, PDF, HTML, ODT, and EPUB in one click. Switch between Dark/Light themes and RU/EN interface language right from the top bar."},
            };

            for (const auto& f : feats) {
                if (y + 52.0f > bg.y + h - 54.0f) break;
                const Rect card{bg.x + 18.0f, y, w - 36.0f, 66.0f};
                ui::fill_rounded_rect(ui, card, 6.0f, ui.palette.window_bg);
                ui::stroke_rect(ui, card, ui.palette.panel_border);
                ui::draw_text(ui, fonts.bold, en() ? f.title_en : f.title_ru,
                              card.x + 12.0f, card.y + 6.0f, ui.palette.accent);
                float fy = card.y + 26.0f;
                for (const auto& l : wrap_lines(fonts.regular, en() ? f.desc_en : f.desc_ru, card.w - 24.0f)) {
                    if (fy + 16.0f > card.y + card.h - 2.0f) break;
                    ui::draw_text(ui, fonts.regular, l, card.x + 12.0f, fy, ui.palette.text);
                    fy += 17.0f;
                }
                y += 72.0f;
            }

            const float by = bg.y + h - 42.0f;
            const std::string chk_label = std::string(show_welcome_on_startup ? "[ ] " : "[✓] ")
                + tr("Не показывать при следующем запуске", "Don't show again on startup");
            const float chk_w = text_width(fonts.regular, chk_label) + 24.0f;
            if (ui::toggle_button(ui, "wel_dont_show", {bg.x + 18.0f, by, chk_w, 30.0f},
                                  chk_label.c_str(), !show_welcome_on_startup)) {
                show_welcome_on_startup = !show_welcome_on_startup;
                save_settings();
            }

            const char* start_lbl = tr("Начать работу", "Start Writing");
            const float start_w = std::max(140.0f, text_width(fonts.regular, start_lbl) + 32.0f);
            if (ui::button(ui, "wel_start", {bg.x + w - 18.0f - start_w, by, start_w, 30.0f}, start_lbl)) {
                welcome_open = false;
            }
        }

        ui::stroke_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                        ui.palette.panel_border);
        ui::end_frame(ui);
        SDL_RenderPresent(renderer);

        ui.input.pressed = false;
        ui.input.released = false;
    }

    


    bool handle_event(const SDL_Event& e) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                return !confirm_discard();
            case SDL_EVENT_MOUSE_MOTION:
                ui.input.mouse_x = e.motion.x;
                ui.input.mouse_y = e.motion.y;
                if (scrollbar_dragging && max_scroll > 0.0f) {
                    const float avail_h = scrollbar_track.h - scrollbar_thumb.h;
                    if (avail_h > 1.0f) {
                        const float ratio = (e.motion.y - scrollbar_track.y - scrollbar_drag_offset) / avail_h;
                        scroll_y = std::clamp(ratio * max_scroll, 0.0f, max_scroll);
                    }
                } else if (dragging) {
                    editor_pointer(e.motion.x, e.motion.y, true);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui.input.down = true;
                    ui.input.pressed = true;
                    if (welcome_open) {
                        using namespace ui;
                        const float w = std::min(680.0f, static_cast<float>(width) - 32.0f);
                        const float h = std::min(510.0f, static_cast<float>(height) - 32.0f);
                        const Rect bg{(static_cast<float>(width) - w) / 2.0f,
                                      (static_cast<float>(height) - h) / 2.0f, w, h};
                        if (!contains(bg, e.button.x, e.button.y)) {
                            welcome_open = false;
                            break;
                        }
                    }
                    if (pandoc_help_open) {
                        using namespace ui;
                        const float w = 580.0f;
                        const float h = 380.0f;
                        const Rect bg{(static_cast<float>(width)-w)/2.0f, (static_cast<float>(height)-h)/2.0f, w, h};
                        if (contains(bg, e.button.x, e.button.y)) {
                            // click inside modal - let ui handle
                        } else {
                            pandoc_help_open = false;
                            break;
                        }
                    }
                    if (max_scroll > 0.0f && scrollbar_track.w > 0.0f
                        && ui::contains(scrollbar_track, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui) && !formula_editor_open
                        && !diag_editor_open && !chart_editor_open
                        && !welcome_open && !pandoc_help_open) {
                        scrollbar_dragging = true;
                        if (ui::contains(scrollbar_thumb, e.button.x, e.button.y)) {
                            scrollbar_drag_offset = e.button.y - scrollbar_thumb.y;
                        } else {
                            scrollbar_drag_offset = scrollbar_thumb.h / 2.0f;
                            const float avail_h = scrollbar_track.h - scrollbar_thumb.h;
                            if (avail_h > 1.0f) {
                                const float ratio = (e.button.y - scrollbar_track.y - scrollbar_drag_offset) / avail_h;
                                scroll_y = std::clamp(ratio * max_scroll, 0.0f, max_scroll);
                            }
                        }
                        break;
                    }
                    if (ui::contains(doc_viewport, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui) && !formula_editor_open
                        && !diag_editor_open && !chart_editor_open
                        && !welcome_open && !pandoc_help_open) {
                        // A click on a formula/diagram/chart block opens the visual editor.
                        const float cy = e.button.y - doc_viewport.y + scroll_y;
                        int hit = -1;
                        for (size_t i = 0; i < doc.blocks.size() && i < layout.boxes.size(); ++i) {
                            const auto& box = layout.boxes[i];
                            if (cy >= box.y && cy <= box.y + box.h) {
                                hit = static_cast<int>(i);
                                break;
                            }
                        }
                        // Double-click below the last block (or at the bottom when the last block
                        // is a code/formula/diagram/chart/table/rule block) appends a new paragraph
                        // and moves the caret into it so the user is never trapped.
                        if (e.button.clicks >= 2 && !doc.blocks.empty()) {
                            const float doc_bottom = layout.boxes.empty()
                                ? 0.0f
                                : (layout.boxes.back().y + layout.boxes.back().h);
                            const md::Block& last_blk = doc.blocks.back();
                            const bool last_is_non_para = last_blk.type != md::BlockType::Paragraph;
                            if (cy > doc_bottom - 4.0f || (hit == -1 && cy >= doc_bottom - 20.0f)
                                || (last_is_non_para && cy >= doc_bottom - 12.0f)) {
                                md::Block para;
                                para.type = md::BlockType::Paragraph;
                                para.inlines.push_back(md::Inline{"", 0, ""});
                                doc.blocks.push_back(std::move(para));
                                const int new_idx = static_cast<int>(doc.blocks.size()) - 1;
                                editor.set_caret(md::Pos{new_idx, 0, 0, -1});
                                mark_edited();
                                md::layout_document(doc, fontbook, laid_out_width > 0.0f ? laid_out_width : 760.0f, layout);
                                max_scroll = std::max(0.0f, layout.content_height - doc_viewport.h + 24.0f);
                                ensure_caret_visible();
                                dragging = false;
                                break;
                            }
                        }
                        bool opened = false;
                        if (hit >= 0 && hit < static_cast<int>(doc.blocks.size())) {
                            const md::Block& blk = doc.blocks[hit];
                            if (blk.type == md::BlockType::CodeBlock
                                && (blk.lang == "math" || blk.lang == "latex" || blk.lang == "tex")) {
                                formula_open_block(hit);
                                opened = true;
                                // The panel appears under the cursor mid-click;
                                // swallow this click so it can't hit a panel button.
                                formula_just_opened = true;
                                ui.input.pressed = false;
                                ui.active_id.clear();
                            } else if (blk.type == md::BlockType::CodeBlock
                                       && md::is_diagram_lang(blk.lang)) {
                                std::string code;
                                for (const md::Inline& inl : blk.inlines) code += inl.text;
                                if (md::is_chart_text(code)) {
                                    if (chart_open_block(hit)) {
                                        opened = true;
                                        chart_just_opened = true;
                                        ui.input.pressed = false;
                                        ui.active_id.clear();
                                    }
                                } else if (diag_open_block(hit)) {
                                    // A click on a diagram block opens its editor,
                                    // swallowed the same way.
                                    opened = true;
                                    diag_just_opened = true;
                                    ui.input.pressed = false;
                                    ui.active_id.clear();
                                }
                            }
                        }
                        if (!opened) {
                            const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                            editor_pointer(e.button.x, e.button.y, shift);
                            dragging = true;
                        }
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (ui::contains(doc_viewport, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui)) {
                        const float dx = e.button.x;
                        const float dy = e.button.y;
                        // find block under cursor
                        const float cy = dy - doc_viewport.y + scroll_y;
                        int hit_block = -1;
                        for (size_t i = 0; i < doc.blocks.size() && i < layout.boxes.size(); ++i) {
                            const auto& box = layout.boxes[i];
                            if (cy >= box.y && cy <= box.y + box.h) {
                                hit_block = (int)i;
                                break;
                            }
                        }
                        if (hit_block >= 0 && hit_block < (int)doc.blocks.size()) {
                            const auto& blk = doc.blocks[hit_block];
                            if (blk.type == md::BlockType::CodeBlock) {
                                const std::string& l = blk.lang;
                                if (l == "math" || l == "latex" || l == "tex" || l == "mermaid"
                                    || l == "plantuml" || l == "puml" || l == "dot" || l == "graphviz") {
                                    std::vector<std::string> items;
                                    if (l == "mermaid" || l == "plantuml" || l == "puml" || l == "dot" || l == "graphviz") {
                                        for (const auto& t : kDiagramTemplates) items.push_back(std::string("Шаблон: ") + t.label_ru);
                                    }
                                    if (l == "math" || l == "latex" || l == "tex") {
                                        for (const auto& t : kMathTemplates) items.push_back(std::string("Шаблон: ") + t.label_ru);
                                    }
                                    items.push_back("Очистить блок");
                                    items.push_back("Вставить $...$ inline");
                                    // store context
                                    struct Ctx { int block; std::string lang; };
                                    static Ctx ctxb = {-1, ""};
                                    ctxb.block = hit_block; ctxb.lang = l;
                                    // use a simple approach: show via dropdown? easier: just handle by pushing action
                                    // but we don't have ad-hoc menu; trigger by setting a flag - quick hack
                                    // open a small popup by reusing dropdown logic not directly possible; instead just apply first action? no
                                    // better: remember and handle in frame - store in app
                                    // (we'll add members)
                                    extern App* g_app;
                                    if (g_app) {
                                        g_app->ctx_menu_block = hit_block;
                                        g_app->ctx_menu_lang = l;
                                        g_app->ctx_menu_x = dx; g_app->ctx_menu_y = dy;
                                        g_app->show_ctx_menu = true;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui.input.down = false;
                    // Swallow the release of the click that opened an editor.
                    if (formula_just_opened || diag_just_opened || chart_just_opened) {
                        formula_just_opened = false;
                        diag_just_opened = false;
                        chart_just_opened = false;
                        ui.input.released = false;
                    } else {
                        ui.input.released = true;
                    }
                    dragging = false;
                    scrollbar_dragging = false;
                }
                break;
            case SDL_EVENT_DROP_BEGIN:
                pending_drops.clear();
                drop_gesture_active = true;
                break;
            case SDL_EVENT_DROP_FILE:
                if (e.drop.data != nullptr) {
                    if (!drop_gesture_active) {
                        // In case a backend emits DROP_FILE without DROP_BEGIN, process immediately
                        std::string dropped(e.drop.data);
                        if (!has_supported_markdown_ext(dropped)) {
                            dialogs::show_error(tr("Поддерживаются только файлы .md, .markdown и .txt",
                                                   "Only .md, .markdown, and .txt files are supported"));
                        } else {
                            open_from_path(dropped, true, true);
                        }
                    } else {
                        pending_drops.emplace_back(e.drop.data);
                    }
                }
                break;
            case SDL_EVENT_DROP_COMPLETE:
                if (drop_gesture_active) {
                    drop_gesture_active = false;
                    std::vector<std::string> files = std::move(pending_drops);
                    pending_drops.clear();
                    if (files.size() > 1) {
                        dialogs::show_error(tr("Можно перетащить только один файл за раз",
                                               "Please drop only one file at a time"));
                        status = tr("Отклонено: несколько файлов", "Rejected: multiple files");
                    } else if (files.size() == 1) {
                        std::error_code ec;
                        const std::filesystem::path fsp = std::filesystem::u8path(files[0]);
                        if (std::filesystem::is_directory(fsp, ec) || !has_supported_markdown_ext(files[0])) {
                            dialogs::show_error(tr("Поддерживаются только файлы .md, .markdown и .txt",
                                                   "Only .md, .markdown, and .txt files are supported"));
                            status = tr("Неподдерживаемый тип файла", "Unsupported file type");
                        } else {
                            open_from_path(files[0], true, true);
                        }
                    }
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (welcome_open || pandoc_help_open) {
                    break;
                }
                if (formula_editor_open) {
                    formula_insert_symbol(e.text.text);
                    break;
                }
                if (diag_editor_open) {
                    diag_insert_text(e.text.text);
                    break;
                }
                if (chart_editor_open) {
                    chart_insert_text(e.text.text);
                    break;
                }
                editor.insert_text(e.text.text);
                mark_edited();
                ensure_caret_visible();
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_F1) {
                    welcome_open = !welcome_open;
                    break;
                }
                if (e.key.key == SDLK_ESCAPE) {
                    if (welcome_open) {
                        welcome_open = false;
                        break;
                    }
                    if (pandoc_help_open) {
                        pandoc_help_open = false;
                        break;
                    }
                    if (formula_editor_open) {
                        formula_editor_open = false;
                        status = tr("Редактор формул закрыт", "Formula editor closed");
                        break;
                    }
                    if (diag_editor_open) {
                        diag_editor_open = false;
                        status = tr("Редактор диаграмм закрыт", "Diagram editor closed");
                        break;
                    }
                    if (chart_editor_open) {
                        chart_editor_open = false;
                        status = tr("Редактор графика закрыт", "Chart editor closed");
                        break;
                    }
                    ui.open_popup.clear();
                    break;
                }
                if (welcome_open || pandoc_help_open) {
                    if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                        welcome_open = false;
                        pandoc_help_open = false;
                    }
                    break;
                }
                if (formula_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: formula_backspace(); break;
                        case SDLK_DELETE: formula_delete_element(); break;
                        case SDLK_TAB: formula_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (formula_off > 0) {
                                const md::MathNode* leaf =
                                    md::node_at_path(formula_edit, formula_path);
                                if (leaf && leaf->type == md::MathNode::Text) {
                                    size_t p = static_cast<size_t>(formula_off);
                                    --p;
                                    while (p > 0 && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) --p;
                                    formula_off = static_cast<int>(p);
                                }
                            } else {
                                formula_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            const md::MathNode* leaf =
                                md::node_at_path(formula_edit, formula_path);
                            if (leaf && leaf->type == md::MathNode::Text
                                && formula_off < static_cast<int>(leaf->text.size())) {
                                size_t p = static_cast<size_t>(formula_off) + 1;
                                while (p < leaf->text.size()
                                       && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) ++p;
                                formula_off = static_cast<int>(p);
                            } else {
                                formula_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            formula_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                if (diag_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: diag_backspace(); break;
                        case SDLK_DELETE: diag_delete_element(); break;
                        case SDLK_TAB: diag_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (diag_off > 0) {
                                std::string* t = diag_sel_text();
                                if (t != nullptr) {
                                    size_t p = static_cast<size_t>(diag_off);
                                    --p;
                                    while (p > 0
                                           && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                        --p;
                                    }
                                    diag_off = static_cast<int>(p);
                                }
                            } else {
                                diag_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            std::string* t = diag_sel_text();
                            if (t != nullptr && diag_off < static_cast<int>(t->size())) {
                                size_t p = static_cast<size_t>(diag_off) + 1;
                                while (p < t->size()
                                       && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                    ++p;
                                }
                                diag_off = static_cast<int>(p);
                            } else {
                                diag_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            diag_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                if (chart_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: chart_backspace(); break;
                        case SDLK_DELETE:
                            {
                                std::string* t = chart_sel_text();
                                if (t != nullptr && !t->empty() && chart_off < static_cast<int>(t->size())) {
                                    size_t p = static_cast<size_t>(chart_off);
                                    while (p < t->size() && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) ++p;
                                    size_t q = p + 1;
                                    while (q < t->size() && (static_cast<unsigned char>((*t)[q]) & 0xC0) == 0x80) ++q;
                                    t->erase(p, q - p);
                                    chart_off = static_cast<int>(p);
                                } else {
                                    chart_delete_row();
                                }
                            }
                            break;
                        case SDLK_TAB: chart_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (chart_off > 0) {
                                std::string* t = chart_sel_text();
                                if (t != nullptr) {
                                    size_t p = static_cast<size_t>(chart_off);
                                    --p;
                                    while (p > 0
                                           && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                        --p;
                                    }
                                    chart_off = static_cast<int>(p);
                                }
                            } else {
                                chart_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            std::string* t = chart_sel_text();
                            if (t != nullptr && chart_off < static_cast<int>(t->size())) {
                                size_t p = static_cast<size_t>(chart_off) + 1;
                                while (p < t->size()
                                       && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                    ++p;
                                }
                                chart_off = static_cast<int>(p);
                            } else {
                                chart_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            chart_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                handle_key(e.key);
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float dy = e.wheel.y;
                if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
                scroll_y = std::clamp(scroll_y - dy * 48.0f, 0.0f, max_scroll);
                break;
            }
            default:
                break;
        }
        return true;
    }


    

    // it is painted; the second pass draws and handles clicks.





};



App* g_app = nullptr;

static void push_mouse_event(SDL_EventType type, float x, float y) {
    SDL_Event e{};
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.type = type;
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.type = type;
        e.button.x = x;
        e.button.y = y;
        e.button.button = SDL_BUTTON_LEFT;
    }
    SDL_PushEvent(&e);
}

static void push_wheel_event(float dy) {
    SDL_Event e{};
    e.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = dy;
    SDL_PushEvent(&e);
}

static void push_key_event(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.mod = mod;
    SDL_PushEvent(&e);
}

} // namespace


int main(int argc, char** argv) {
    const bool selftest = argc > 1 && std::string(argv[1]) == "--selftest";
    const std::string screenshot = argc > 2 ? argv[2] : "selftest.bmp";

    App app;
    g_app = &app;
    if (!app.init()) {
        app.shutdown();
        return 1;
    }
    std::printf("init ok: SDL %s, fonts + Cyrillic ok\n", SDL_GetRevision());

    if (selftest) {
        auto pump = [&]() {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                app.dispatch(e);
            }
        };
        auto save_frame = [&](const std::string& path) {
            app.frame();
            SDL_Surface* pixels = SDL_RenderReadPixels(app.renderer, nullptr);
            if (!pixels) {
                std::fprintf(stderr, "read pixels: %s\n", SDL_GetError());
                return false;
            }
            const bool saved = SDL_SaveBMP(pixels, path.c_str());
            SDL_DestroySurface(pixels);
            if (!saved) std::fprintf(stderr, "save bmp: %s\n", SDL_GetError());
            return saved;
        };

        app.ui_lang = 0;
        app.light_theme = false;
        app.ui.palette = ui::dark_palette();
        app.welcome_open = true;
        for (int i = 0; i < 5; ++i) {
            pump();
            app.frame();
        }
        const auto wdot = screenshot.find_last_of('.');
        const std::string welcome_path = wdot == std::string::npos
            ? screenshot + "_welcome"
            : screenshot.substr(0, wdot) + "_welcome" + screenshot.substr(wdot);
        if (!save_frame(welcome_path)) {
            app.shutdown();
            return 1;
        }
        // Close welcome modal via Escape and verify it closes
        push_key_event(SDLK_ESCAPE);
        pump();
        app.frame();
        if (app.welcome_open) {
            std::fprintf(stderr, "selftest: welcome modal did not close on Escape\n");
            app.shutdown();
            return 1;
        }
        if (!save_frame(screenshot)) {
            app.shutdown();
            return 1;
        }
        std::printf("selftest frame ok: %dx%d, screenshot %s\n", app.width, app.height,
                    screenshot.c_str());

        // Synthetic click on the dialect dropdown: press, release, then let the
        // deferred popup render.
        const float dx = app.dialect_rect.x + app.dialect_rect.w - 25.0f;
        const float dy = app.dialect_rect.y + app.dialect_rect.h / 2.0f;
        push_mouse_event(SDL_EVENT_MOUSE_MOTION, dx, dy);
        push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, dx, dy);
        pump();
        app.frame();
        push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, dx, dy);
        pump();
        app.frame();
        if (app.ui.open_popup != "dialect") {
            std::fprintf(stderr, "selftest: dropdown did not open\n");
            app.shutdown();
            return 1;
        }
        const auto dot = screenshot.find_last_of('.');
        const std::string popup_path = dot == std::string::npos
            ? screenshot + "_popup"
            : screenshot.substr(0, dot) + "_popup" + screenshot.substr(dot);
        if (!save_frame(popup_path)) {
            app.shutdown();
            return 1;
        }
        std::printf("selftest dropdown ok: screenshot %s\n", popup_path.c_str());

        // Click the second item ("GitHub Flavored Markdown").
        const float item_x = app.dialect_rect.x + app.dialect_rect.w / 2.0f;
        const float item_y = app.dialect_rect.y + app.dialect_rect.h + 4.0f + 32.0f + 16.0f;
        push_mouse_event(SDL_EVENT_MOUSE_MOTION, item_x, item_y);
        push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, item_x, item_y);
        pump();
        app.frame();
        push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, item_x, item_y);
        pump();
        app.frame();
        if (app.dialect != 1 || !app.ui.open_popup.empty()) {
            std::fprintf(stderr, "selftest: item selection failed (dialect=%d)\n", app.dialect);
            app.shutdown();
            return 1;
        }
        std::printf("selftest selection ok: %s\n", kDialects[app.dialect].c_str());

        // Selection + caret overlay screenshot.
        app.editor.set_caret(md::Pos{2, 0, 0});
        app.editor.set_caret(md::Pos{4, 1, 5}, true);
        app.editor.blink_tick = SDL_GetTicks();
        const std::string sel_path = dot == std::string::npos
            ? screenshot + "_select"
            : screenshot.substr(0, dot) + "_select" + screenshot.substr(dot);
        if (!save_frame(sel_path)) {
            app.shutdown();
            return 1;
        }

        // Wheel scroll moves the document viewport.
        push_wheel_event(-1.0f);
        pump();
        app.frame();
        if (app.scroll_y <= 0.0f) {
            std::fprintf(stderr, "selftest: scroll failed (height=%.0f)\n",
                         app.layout.content_height);
            app.shutdown();
            return 1;
        }
        std::printf("selftest scroll ok: offset=%.0f, lines=%zu, height=%.0f\n",
                    app.scroll_y, app.layout.lines.size(), app.layout.content_height);

        // Editing through the public editor API.
        auto& ed = app.editor;
        ed.click(app.layout, app.fontbook, 5.0f, 5.0f, false);
        ed.move(app.layout, app.fontbook, md::Editor::Move::DocStart, false);
        ed.insert_text("Тест ");
        if (app.doc.blocks[0].inlines[0].text.rfind("Тест ", 0) != 0) {
            std::fprintf(stderr, "selftest: insert failed\n");
            app.shutdown();
            return 1;
        }
        const size_t blocks_before = app.doc.blocks.size();
        ed.newline();
        if (app.doc.blocks.size() != blocks_before + 1) {
            std::fprintf(stderr, "selftest: block split failed\n");
            app.shutdown();
            return 1;
        }
        ed.backspace();
        if (app.doc.blocks.size() != blocks_before) {
            std::fprintf(stderr, "selftest: block merge failed\n");
            app.shutdown();
            return 1;
        }
        // Consecutive typing must not leave a selection behind.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.insert_text("a");
        ed.insert_text("b");
        ed.insert_text("c");
        if (ed.has_selection() || app.doc.blocks[1].inlines[0].text.rfind("abc", 0) != 0) {
            std::fprintf(stderr, "selftest: consecutive typing failed (%s)\n",
                         app.doc.blocks[1].inlines[0].text.c_str());
            app.shutdown();
            return 1;
        }

        // Backspace/Delete across inline (style) boundaries.
        // Block 1: plain("Лёгкий ") + bold("WYSIWYG") + plain(...).
        ed.set_caret(md::Pos{1, 2, 0});
        ed.backspace();
        if (app.doc.blocks[1].inlines[1].text != "WYSIWY") {
            std::fprintf(stderr, "selftest: backspace at inline start failed (%s)\n",
                         app.doc.blocks[1].inlines[1].text.c_str());
            app.shutdown();
            return 1;
        }
        ed.set_caret(md::Pos{1, 0,
                     static_cast<int>(app.doc.blocks[1].inlines[0].text.size())});
        ed.delete_forward();
        if (app.doc.blocks[1].inlines[1].text != "YSIWY") {
            std::fprintf(stderr, "selftest: delete at inline end failed (%s)\n",
                         app.doc.blocks[1].inlines[1].text.c_str());
            app.shutdown();
            return 1;
        }
        std::printf("selftest inline boundaries ok\n");

        // Bold on a selection, then removed again: runs split and merge back.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.set_caret(md::Pos{1, 0, 3}, true);
        ed.toggle_style(md::StyleBold);
        {
            const auto& il = app.doc.blocks[1].inlines;
            if (il[0].text != "abc" || !(il[0].style & md::StyleBold)
                || il[1].text.rfind("Лёгкий", 0) != 0 || (il[1].style & md::StyleBold)) {
                std::fprintf(stderr, "selftest: bold on selection failed\n");
                app.shutdown();
                return 1;
            }
        }
        if (!(ed.active_style() & md::StyleBold)) {
            std::fprintf(stderr, "selftest: active style on selection failed\n");
            app.shutdown();
            return 1;
        }
        ed.toggle_style(md::StyleBold);
        if (app.doc.blocks[1].inlines[0].style != 0
            || app.doc.blocks[1].inlines[0].text.rfind("abcЛёгкий", 0) != 0) {
            std::fprintf(stderr, "selftest: style removal/merge failed\n");
            app.shutdown();
            return 1;
        }

        // Toggle without a selection styles the next typed text only.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.toggle_style(md::StyleItalic);
        if (!(ed.active_style() & md::StyleItalic)) {
            std::fprintf(stderr, "selftest: pending style not reported\n");
            app.shutdown();
            return 1;
        }
        ed.insert_text("Z");
        ed.insert_text("Y");
        {
            const auto& il = app.doc.blocks[1].inlines;
            if (il[0].text != "ZY" || !(il[0].style & md::StyleItalic)
                || il[1].text.rfind("abc", 0) != 0 || (il[1].style & md::StyleItalic)
                || ed.has_selection()) {
                std::fprintf(stderr, "selftest: pending style on typing failed\n");
                app.shutdown();
                return 1;
            }
        }
        std::printf("selftest inline styles ok\n");

        // Italic over a whole paragraph keeps its bold runs.
        {
            const auto& blk = app.doc.blocks[3];
            ed.set_caret(md::Pos{3, 0, 0});
            ed.set_caret(md::Pos{3, static_cast<int>(blk.inlines.size()) - 1,
                                 static_cast<int>(blk.inlines.back().text.size())}, true);
            ed.toggle_style(md::StyleItalic);
            app.doc_dirty = true;
            ed.set_caret(md::Pos{0, 0, 0});
            const std::string styles_path = dot == std::string::npos
                ? screenshot + "_styles"
                : screenshot.substr(0, dot) + "_styles" + screenshot.substr(dot);
            if (!save_frame(styles_path)) {
                app.shutdown();
                return 1;
            }
        }

        // Block operations.
        {
            auto fail = [&](const char* what) {
                std::fprintf(stderr, "selftest: %s\n", what);
                app.shutdown();
                return 1;
            };
            auto& d = app.doc;

            ed.set_caret(md::Pos{1, 0, 0});
            ed.set_block(md::BlockType::Heading, 2);
            if (d.blocks[1].type != md::BlockType::Heading || d.blocks[1].level != 2) {
                return fail("heading on paragraph failed");
            }
            ed.set_block(md::BlockType::Heading, 2);
            if (d.blocks[1].type != md::BlockType::Paragraph) {
                return fail("heading toggle off failed");
            }

            // Ordered list over blocks 1..2, then Enter continues it.
            ed.set_caret(md::Pos{1, 0, 0});
            ed.set_caret(md::Pos{2, 0, 0}, true);
            ed.set_block(md::BlockType::ListItem, 0, true);
            md::layout_document(d, app.fontbook, 760.0f, app.layout);
            if (app.layout.markers[1] != "1." || app.layout.markers[2] != "2.") {
                return fail("ordered list numbering failed");
            }
            ed.set_caret(md::Pos{2, 0, static_cast<int>(d.blocks[2].inlines[0].text.size())});
            ed.newline();
            if (d.blocks[3].type != md::BlockType::ListItem || !d.blocks[3].ordered) {
                return fail("list continuation on Enter failed");
            }
            // Enter on the empty item leaves the list.
            ed.newline();
            if (d.blocks[3].type != md::BlockType::Paragraph) {
                return fail("empty list item exit failed");
            }

            // Nesting with Tab, then numbering restarts at depth 1.
            ed.set_caret(md::Pos{2, 0, 0});
            if (!ed.indent(1) || d.blocks[2].depth != 1) {
                return fail("indent failed");
            }
            md::layout_document(d, app.fontbook, 760.0f, app.layout);
            if (app.layout.markers[2] != "1.") {
                return fail("nested numbering failed");
            }
            ed.indent(-1);

            // Multi-digit numbering: the marker must never be clipped at the
            // left edge of the content column (indent follows marker width).
            {
                md::Document wide;
                for (int i = 0; i < 15; ++i) {
                    md::Block b;
                    b.type = md::BlockType::ListItem;
                    b.ordered = true;
                    b.inlines.push_back(md::Inline{"пункт", 0, ""});
                    wide.blocks.push_back(std::move(b));
                }
                md::Layout wl;
                md::layout_document(wide, app.fontbook, 760.0f, wl);
                bool wide_ok = true;
                for (const md::Line& ln : wl.lines) {
                    if (ln.block < 0 || static_cast<size_t>(ln.block) >= wl.markers.size()) continue;
                    const std::string& mk = wl.markers[static_cast<size_t>(ln.block)];
                    if (mk.empty()) continue;
                    TTF_Font* mf = app.fontbook.get(md::Family::Sans, false, false, 17.0f);
                    int mw = 0, mh = 0;
                    TTF_GetStringSize(mf, mk.c_str(), mk.size(), &mw, &mh);
                    if (ln.x0 - 10.0f - static_cast<float>(mw) < 0.0f) {
                        wide_ok = false;
                        std::fprintf(stderr, "selftest: list marker '%s' clipped (x0=%.1f mw=%d)\n",
                                     mk.c_str(), ln.x0, mw);
                    }
                }
                if (!wide_ok || wl.markers.back() != "15.") {
                    return fail("multi-digit list marker indent failed");
                }
            }
            if (d.blocks[2].depth != 0) {
                return fail("outdent failed");
            }

            // Rule on the blank paragraph 3, paragraph after it.
            ed.set_caret(md::Pos{3, 0, 0});
            ed.insert_rule();
            if (d.blocks[3].type != md::BlockType::Rule
                || d.blocks[4].type != md::BlockType::Paragraph
                || !(ed.caret() == md::Pos{4, 0, 0})) {
                return fail("insert_rule failed");
            }

            // Code block toggle on paragraph 4 and back.
            ed.set_block(md::BlockType::CodeBlock);
            if (d.blocks[4].type != md::BlockType::CodeBlock) {
                return fail("code block failed");
            }
            ed.set_block(md::BlockType::CodeBlock);

            // Backspace at the start of a list item turns it into a paragraph.
            ed.set_caret(md::Pos{1, 0, 0});
            ed.backspace();
            if (d.blocks[1].type != md::BlockType::Paragraph) {
                return fail("backspace unformat failed");
            }

            app.doc_dirty = true;
            const std::string blocks_path = dot == std::string::npos
                ? screenshot + "_blocks"
                : screenshot.substr(0, dot) + "_blocks" + screenshot.substr(dot);
            ed.set_caret(md::Pos{0, 0, 0});
            if (!save_frame(blocks_path)) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest block operations ok\n");
        }

        ed.select_all();
        if (ed.selected_text().empty()) {
            std::fprintf(stderr, "selftest: select all failed\n");
            app.shutdown();
            return 1;
        }
        ed.delete_selection();
        if (app.doc.blocks.size() != 1) {
            std::fprintf(stderr, "selftest: delete selection failed\n");
            app.shutdown();
            return 1;
        }

        // hit_test and locate_caret agree at the document start.
        md::layout_document(app.doc, app.fontbook, 760.0f, app.layout);
        ed.move(app.layout, app.fontbook, md::Editor::Move::DocStart, false);
        float cx = 0;
        const int li = md::locate_caret(app.doc, app.layout, app.fontbook, ed.caret(), &cx);
        if (li < 0) {
            std::fprintf(stderr, "selftest: locate_caret failed\n");
            app.shutdown();
            return 1;
        }
        const md::Pos p = md::hit_test(app.doc, app.layout, app.fontbook, cx,
                                       app.layout.lines[li].y + 2.0f);
        if (!(p == ed.caret())) {
            std::fprintf(stderr, "selftest: hit/locate roundtrip failed\n");
            app.shutdown();
            return 1;
        }
        // Key-driven rule editing, a frame after every key like the real loop.
        const SDL_Keymod ctrl_shift = static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
        auto press = [&](SDL_Keycode key, SDL_Keymod mod) {
            push_key_event(key, mod);
            pump();
            app.frame();
        };
        auto has_rule = [&]() {
            for (const md::Block& b : app.doc.blocks) {
                if (b.type == md::BlockType::Rule) return true;
            }
            return false;
        };

        ed.insert_text("alpha");
        press(SDLK_RETURN, SDL_KMOD_NONE);
        press(SDLK_R, ctrl_shift);
        if (!has_rule()) {
            std::fprintf(stderr, "selftest: rule insert by keys failed\n");
            app.shutdown();
            return 1;
        }
        press(SDLK_BACKSPACE, SDL_KMOD_NONE);
        if (has_rule() || app.doc.blocks.size() != 2) {
            std::fprintf(stderr, "selftest: backspace did not remove rule\n");
            app.shutdown();
            return 1;
        }
        press(SDLK_R, ctrl_shift);
        ed.set_caret(md::Pos{1, 0, 0});
        press(SDLK_DELETE, SDL_KMOD_NONE);
        if (has_rule() || app.doc.blocks.size() != 2) {
            std::fprintf(stderr, "selftest: delete did not remove rule\n");
            app.shutdown();
            return 1;
        }
        std::printf("selftest rule keys ok: blocks=%zu\n", app.doc.blocks.size());

        // Markdown import/export. Every fixture must reach a fixed point after one
        // round trip, and the structure of the basic fixture must match the source.
        {
            const std::string fixtures_dir = std::string(PROJECT_SOURCE_DIR) + "/tests/fixtures/";
            struct Fixture { const char* name; bool gfm; };
            const Fixture fixtures[] = {{"basic.md", false}, {"escapes.md", false},
                                        {"gfm.md", true}, {"tables.md", true}};

            for (const Fixture& fx : fixtures) {
                std::string src;
                if (!md::read_text_file(fixtures_dir + fx.name, src)) {
                    std::fprintf(stderr, "selftest: fixture %s missing\n", fx.name);
                    app.shutdown();
                    return 1;
                }
                md::Document d1, d2;
                md::import_markdown(src, fx.gfm, d1);
                const std::string first = md::export_markdown(d1, fx.gfm);
                md::import_markdown(first, fx.gfm, d2);
                const std::string second = md::export_markdown(d2, fx.gfm);
                if (first != second) {
                    std::fprintf(stderr, "selftest: %s is not stable:\n--- first\n%s\n--- second\n%s\n",
                                 fx.name, first.c_str(), second.c_str());
                    app.shutdown();
                    return 1;
                }
                md::write_text_file(std::string(PROJECT_SOURCE_DIR) + "/build/export_" + fx.name,
                                    first);
            }

            md::Document basic;
            std::string basic_src;
            md::read_text_file(fixtures_dir + "basic.md", basic_src);
            md::import_markdown(basic_src, false, basic);

            auto has_block = [&](auto pred) {
                for (const md::Block& b : basic.blocks) {
                    if (pred(b)) return true;
                }
                return false;
            };
            auto has_inline = [&](auto pred) {
                for (const md::Block& b : basic.blocks) {
                    for (const md::Inline& inl : b.inlines) {
                        if (pred(inl)) return true;
                    }
                }
                return false;
            };

            const bool ok =
                basic.blocks[0].type == md::BlockType::Heading && basic.blocks[0].level == 1
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::ListItem && b.depth == 1;
                })
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::ListItem && b.ordered;
                })
                && has_block([](const md::Block& b) { return b.type == md::BlockType::Quote; })
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::CodeBlock
                        && b.inlines[0].text.find("return 0;") != std::string::npos;
                })
                && has_block([](const md::Block& b) { return b.type == md::BlockType::Rule; })
                && has_inline([](const md::Inline& i) {
                    return (i.style & md::StyleLink) && i.href == "https://example.com";
                })
                && has_inline([](const md::Inline& i) {
                    return (i.style & md::StyleBold) && (i.style & md::StyleItalic);
                });
            if (!ok) {
                std::fprintf(stderr, "selftest: basic.md structure mismatch\n");
                app.shutdown();
                return 1;
            }

            md::Document esc;
            std::string esc_src;
            md::read_text_file(fixtures_dir + "escapes.md", esc_src);
            md::import_markdown(esc_src, false, esc);
            const bool decoded = esc.blocks.size() >= 3
                && esc.blocks[1].inlines[0].text.find("C:\\temp") != std::string::npos
                && esc.blocks[1].inlines[0].text.find("Два * три") != std::string::npos
                && esc.blocks[4].inlines[0].text.find("Слово & сущность") != std::string::npos
                && esc.blocks[4].inlines[0].text.find("Привет") != std::string::npos;
            if (!decoded) {
                std::fprintf(stderr, "selftest: escapes/entities not decoded\n");
                app.shutdown();
                return 1;
            }
            auto fixture_text = [&](const char* name) {
                std::string text;
                md::read_text_file(fixtures_dir + name, text);
                return text;
            };
            const bool detect_ok = md::looks_like_gfm(fixture_text("gfm.md"))
                && !md::looks_like_gfm(fixture_text("basic.md"))
                && !md::looks_like_gfm(fixture_text("escapes.md"))
                && !md::looks_like_gfm("```\nx ~~y~~\n| a | b |\n|---|---|\n```\n")
                && md::looks_like_gfm("text\n\n| a | b |\n|---|---|\n| 1 | 2 |\n");
            if (!detect_ok) {
                std::fprintf(stderr, "selftest: GFM detection mismatch\n");
                app.shutdown();
                return 1;
            }
            std::printf("selftest markdown io ok: round trips stable, structure matches, dialect detection ok\n");

            // Save to a known path (the dialog itself is not automated).
            app.file_path = std::string(PROJECT_SOURCE_DIR) + "/build/saved_check.md";
            app.doc = std::move(basic);
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.mark_edited();
            app.scroll_y = 0;
            const std::string import_path = dot == std::string::npos
                ? screenshot + "_import"
                : screenshot.substr(0, dot) + "_import" + screenshot.substr(dot);
            if (!save_frame(import_path)) {
                app.shutdown();
                return 1;
            }
            if (!app.modified || !app.save() || app.modified) {
                std::fprintf(stderr, "selftest: save flow failed\n");
                app.shutdown();
                return 1;
            }
            std::string saved;
            md::read_text_file(app.file_path, saved);
            if (saved != md::export_markdown(app.doc, app.gfm())) {
                std::fprintf(stderr, "selftest: saved file differs from export\n");
                app.shutdown();
                return 1;
            }
            std::printf("selftest save ok: %s\n", app.document_name().c_str());
        }

        // Formula editor: insert a fraction structure, type into its slots,
        // apply, and check that a rendered math block appears with LaTeX
        // kept only in the file model.
        {
            app.modified = false;
            app.file_path.clear();
            app.doc = md::Document{};
            app.doc.blocks.push_back(md::Block{});
            app.doc.blocks[0].inlines.push_back(md::Inline{});
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.doc_dirty = true;
            app.frame();

            app.formula_open_new();
            app.frame(); // the click targets come from the previous frame
            app.formula_insert_node("frac"); // empty fraction at caret
            app.frame();
            // numerator "a", denominator "b"
            app.formula_insert_symbol("a");
            app.frame();
            app.formula_move_slot(1);
            app.formula_insert_symbol("b");
            app.frame();
            app.formula_apply();

            const bool has_math = app.doc.blocks.size() >= 2
                && app.doc.blocks[1].type == md::BlockType::CodeBlock
                && app.doc.blocks[1].lang == "math"
                && !app.doc.blocks[1].inlines.empty()
                && app.doc.blocks[1].inlines[0].text.find("\\frac") != std::string::npos;
            if (!has_math) {
                const md::Block& bad = app.doc.blocks.size() >= 2 ? app.doc.blocks[1] : app.doc.blocks[0];
                std::fprintf(stderr, "selftest: formula apply failed (block lang='%s' text='%s')\n",
                             bad.lang.c_str(),
                             bad.inlines.empty() ? "" : bad.inlines[0].text.c_str());
                app.shutdown();
                return 1;
            }
            app.doc_dirty = true;
            for (int i = 0; i < 3; ++i) { pump(); app.frame(); }
            const auto fdot = screenshot.find_last_of('.');
            save_frame(fdot == std::string::npos
                ? screenshot + "_formula"
                : screenshot.substr(0, fdot) + "_formula" + screenshot.substr(fdot));

            // Reopen the block in the editor: the structure must survive the
            // LaTeX round trip and the numerator slot must be selectable.
            const int math_index = 1;
            app.formula_open_block(math_index);
            app.frame(); // renders the canvas and collects clickable slots
            const bool reopened = app.formula_editor_open
                && app.formula_edit.type == md::MathNode::Seq
                && !app.formula_slots.empty();
            if (!reopened) {
                std::fprintf(stderr, "selftest: formula reopen failed\n");
                app.shutdown();
                return 1;
            }
            save_frame(fdot == std::string::npos
                ? screenshot + "_editor"
                : screenshot.substr(0, fdot) + "_editor" + screenshot.substr(fdot));
            app.formula_editor_open = false;

            // Delete: build a fraction + a trailing symbol, then delete the
            // element under the caret and confirm it disappears.
            {
                app.formula_open_new();
                app.frame();
                app.formula_insert_node("frac");
                app.frame();
                app.formula_insert_symbol("a");
                app.frame();
                // move out of the fraction to root, then type a stray symbol
                app.formula_path = {};
                app.formula_insert_symbol("z");
                app.frame();
                const size_t before = app.formula_edit.slots.size();
                // caret is on the stray "z" (top-level element) -> delete it
                app.formula_delete_element();
                app.frame();
                const bool removed = app.formula_edit.slots.size() == before - 1;
                app.formula_editor_open = false;
                if (!removed) {
                    std::fprintf(stderr, "selftest: formula delete element failed\n");
                    app.shutdown();
                    return 1;
                }
            }

            // Real click on the math block must OPEN the editor and keep it
            // open (the opening click is swallowed, not fed to a panel button).
            {
                app.doc_dirty = true;
                for (int i = 0; i < 3; ++i) { pump(); app.frame(); }
                // Find the math block's on-screen centre from the layout.
                const int mi = 1;
                const md::BlockBox& mbox = app.layout.boxes[mi];
                const float bx = app.doc_viewport.x + mbox.x + mbox.w / 2.0f;
                const float by = app.doc_viewport.y + mbox.y - app.scroll_y + mbox.h / 2.0f;
                app.formula_editor_open = false;
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, by);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, by);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, by);
                pump();
                app.frame();
                if (!app.formula_editor_open) {
                    std::fprintf(stderr, "selftest: clicking math block did not keep editor open\n");
                    app.shutdown();
                    return 1;
                }
                app.formula_editor_open = false;
            }

            std::printf("selftest formula ok: block inserted, rendered, reopened, click-open, delete\n");
        }

        // Diagram editor: a template block renders as a picture (never code),
        // opens on click, edits elements, and keeps mermaid only in the model
        // that goes to the file.
        {
            app.modified = false;
            app.file_path.clear();
            app.doc = md::Document{};
            app.doc.blocks.push_back(md::Block{});
            app.doc.blocks[0].inlines.push_back(md::Inline{});
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.scroll_y = 0;
            app.doc_dirty = true;
            app.frame();

            // Insert the "Ветвление" template the way the Insert tab does.
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = kDiagramTemplates[1].lang;
            b.inlines.push_back(md::Inline{std::string(kDiagramTemplates[1].text_ru), 0, ""});
            app.doc.blocks.push_back(std::move(b));
            app.editor.set_caret(md::Pos{1, 0, 0, -1});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.doc_dirty = true;
            app.frame();

            auto diag_model_of = [&](int idx, bool* ok_out) {
                std::string code;
                for (const md::Inline& inl : app.doc.blocks[idx].inlines) code += inl.text;
                return md::diag_from_text(code, ok_out);
            };

            // The code parses into a visual model with elements.
            {
                bool ok = true;
                const md::DiagModel m = diag_model_of(1, &ok);
                if (!ok || m.nodes.size() < 4 || m.edges.size() < 3) {
                    std::fprintf(stderr,
                                 "selftest: diagram template did not parse "
                                 "(ok=%d nodes=%zu edges=%zu)\n",
                                 ok ? 1 : 0, m.nodes.size(), m.edges.size());
                    app.shutdown();
                    return 1;
                }
            }
            // The layout reserves the height of the picture, not code lines.
            if (app.layout.boxes.size() < 2 || app.layout.boxes[1].h < 60.0f) {
                std::fprintf(stderr, "selftest: diagram block has no picture height (%.1f)\n",
                             app.layout.boxes.size() >= 2 ? app.layout.boxes[1].h : -1.0f);
                app.shutdown();
                return 1;
            }
            const auto ddot = screenshot.find_last_of('.');
            save_frame(ddot == std::string::npos
                ? screenshot + "_diagram"
                : screenshot.substr(0, ddot) + "_diagram" + screenshot.substr(ddot));

            // Opening the block shows the editor with elements and click slots.
            if (!app.diag_open_block(1)) {
                std::fprintf(stderr, "selftest: diagram block did not open in the editor\n");
                app.shutdown();
                return 1;
            }
            app.frame(); // renders the canvas and collects clickable slots
            const bool reopened = app.diag_editor_open
                && !app.diag_edit.nodes.empty() && !app.diag_slots.empty();
            if (!reopened) {
                std::fprintf(stderr,
                             "selftest: diagram reopen failed (nodes=%zu slots=%zu)\n",
                             app.diag_edit.nodes.size(), app.diag_slots.size());
                app.shutdown();
                return 1;
            }
            save_frame(ddot == std::string::npos
                ? screenshot + "_diageditor"
                : screenshot.substr(0, ddot) + "_diageditor" + screenshot.substr(ddot));

            // Add an element, type into it and apply: the block text (the only
            // place code lives) grows a matching node.
            {
                const size_t nodes_before = app.diag_edit.nodes.size();
                app.diag_add_node();
                app.frame();
                app.diag_insert_text("Тест");
                app.frame();
                if (app.diag_edit.nodes.size() != nodes_before + 1) {
                    std::fprintf(stderr, "selftest: diagram add node failed\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_apply();
                bool ok = true;
                const md::DiagModel m = diag_model_of(1, &ok);
                bool has_new = false;
                for (const md::DiagNode& n : m.nodes) {
                    if (n.label == "Тест") has_new = true;
                }
                if (!ok || !has_new) {
                    std::fprintf(stderr, "selftest: diagram apply failed (ok=%d)\n", ok ? 1 : 0);
                    app.shutdown();
                    return 1;
                }
            }

            // Delete: select the first edge and remove it.
            {
                if (!app.diag_open_block(1)) {
                    std::fprintf(stderr, "selftest: diagram reopen before delete failed\n");
                    app.shutdown();
                    return 1;
                }
                app.frame();
                const size_t edges_before = app.diag_edit.edges.size();
                app.diag_select(0, true);
                app.diag_delete_element();
                app.frame();
                if (edges_before == 0 || app.diag_edit.edges.size() != edges_before - 1) {
                    std::fprintf(stderr, "selftest: diagram delete element failed (%zu -> %zu)\n",
                                 edges_before, app.diag_edit.edges.size());
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Real click on the diagram block must OPEN the editor and keep
            // it open (the opening click is swallowed, like the formula one).
            {
                app.doc_dirty = true;
                for (int i = 0; i < 3; ++i) {
                    pump();
                    app.frame();
                }
                const md::BlockBox& dbox = app.layout.boxes[1];
                const float bx = app.doc_viewport.x + dbox.x + dbox.w / 2.0f;
                const float by = app.doc_viewport.y + dbox.y - app.scroll_y + dbox.h / 2.0f;
                app.diag_editor_open = false;
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, by);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, by);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, by);
                pump();
                app.frame();
                if (!app.diag_editor_open) {
                    std::fprintf(stderr,
                                 "selftest: clicking the diagram block did not keep the editor open\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Sequence diagram: a different template still parses, reserves
            // picture height and opens in the editor.
            {
                md::Block sq;
                sq.type = md::BlockType::CodeBlock;
                sq.lang = kDiagramTemplates[5].lang;
                sq.inlines.push_back(
                    md::Inline{std::string(kDiagramTemplates[5].text_ru), 0, ""});
                app.doc.blocks.push_back(std::move(sq));
                app.doc_dirty = true;
                app.frame();
                const int sq_idx = static_cast<int>(app.doc.blocks.size()) - 1;
                bool ok = true;
                const md::DiagModel m = diag_model_of(sq_idx, &ok);
                if (!ok || m.kind != md::DiagKind::Sequence || m.nodes.size() < 2
                    || m.edges.size() < 3) {
                    std::fprintf(stderr,
                                 "selftest: sequence template did not parse "
                                 "(ok=%d kind=%d nodes=%zu edges=%zu)\n",
                                 ok ? 1 : 0, static_cast<int>(m.kind),
                                 m.nodes.size(), m.edges.size());
                    app.shutdown();
                    return 1;
                }
                if (app.layout.boxes.size() < 3 || app.layout.boxes[2].h < 80.0f) {
                    std::fprintf(stderr, "selftest: sequence picture has no height\n");
                    app.shutdown();
                    return 1;
                }
                save_frame(ddot == std::string::npos
                    ? screenshot + "_diagseq"
                    : screenshot.substr(0, ddot) + "_diagseq" + screenshot.substr(ddot));
                if (!app.diag_open_block(sq_idx) || !app.diag_editor_open) {
                    std::fprintf(stderr, "selftest: sequence block did not open\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Unsupported code (class diagram): shown as a neutral placeholder
            // and refuses to open, keeping its text intact.
            {
                md::Block cls;
                cls.type = md::BlockType::CodeBlock;
                cls.lang = "mermaid";
                cls.inlines.push_back(md::Inline{
                    std::string("classDiagram\n  Animal <|-- Duck\n  Animal : +int age\n"), 0, ""});
                app.doc.blocks.push_back(std::move(cls));
                app.doc_dirty = true;
                app.frame();
                const int cls_idx = static_cast<int>(app.doc.blocks.size()) - 1;
                if (app.diag_open_block(cls_idx)) {
                    std::fprintf(stderr, "selftest: unsupported diagram opened anyway\n");
                    app.shutdown();
                    return 1;
                }
                const std::string before = app.doc.blocks[cls_idx].inlines[0].text;
                if (before.find("classDiagram") == std::string::npos) {
                    std::fprintf(stderr, "selftest: unsupported diagram text was rewritten\n");
                    app.shutdown();
                    return 1;
                }
                // Scroll the placeholder into view for the screenshot.
                if (cls_idx < static_cast<int>(app.layout.boxes.size())) {
                    app.scroll_y = std::max(
                        0.0f, app.layout.boxes[cls_idx].y
                                  + app.layout.boxes[cls_idx].h
                                        - static_cast<float>(app.height) + 160.0f);
                }
                app.frame();
                // Its placeholder box must reserve space instead of code lines.
                if (cls_idx >= static_cast<int>(app.layout.boxes.size())
                    || app.layout.boxes[cls_idx].h < 30.0f) {
                    std::fprintf(stderr, "selftest: unsupported diagram has no placeholder box\n");
                    app.shutdown();
                    return 1;
                }
                save_frame(ddot == std::string::npos
                    ? screenshot + "_diagunsupported"
                    : screenshot.substr(0, ddot) + "_diagunsupported" + screenshot.substr(ddot));
            }

            std::printf(
                "selftest diagram ok: picture rendered, editor opened, click-open, add/delete, "
                "sequence, unsupported placeholder\n");
        }

        // File menu, clicked for real: "Новый" empties the document. The modified
        // flag is cleared first so no save prompt is shown.
        {
            app.modified = false;
            app.file_path = "/tmp/opencode/not-used.md";
            app.doc = md::make_demo_document();
            app.doc_dirty = true;
            // The demo document replaced the saved one behind history's back;
            // reset it so the first click is not seen as an edit.
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.frame();

            const float fx = app.file_rect.x + app.file_rect.w / 2.0f;
            const float fy = app.file_rect.y + app.file_rect.h / 2.0f;
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, fx, fy);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, fx, fy);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, fx, fy);
            pump();
            app.frame();
            if (app.ui.open_popup != "file") {
                std::fprintf(stderr, "selftest: file menu did not open\n");
                app.shutdown();
                return 1;
            }
            const auto file_dot = screenshot.find_last_of('.');
            save_frame(file_dot == std::string::npos
                ? screenshot + "_filemenu"
                : screenshot.substr(0, file_dot) + "_filemenu" + screenshot.substr(file_dot));

            const float ix = app.file_rect.x + 20.0f;
            const float iy = app.file_rect.y + app.file_rect.h + 4.0f + app.file_rect.h / 2.0f;
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, ix, iy);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, ix, iy);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, ix, iy);
            pump();
            app.frame();

            const bool empty = app.doc.blocks.size() == 1 && app.doc.blocks[0].inlines.size() == 1
                && app.doc.blocks[0].inlines[0].text.empty();
            if (!empty || !app.file_path.empty() || !app.ui.open_popup.empty()) {
                std::fprintf(stderr, "selftest: file menu \"Новый\" failed\n"
                             "  blocks=%zu fpath='%s' popup='%s' modified=%d\n",
                             app.doc.blocks.size(),
                             app.file_path.c_str(), app.ui.open_popup.c_str(),
                             app.modified ? 1 : 0);
                app.shutdown();
                return 1;
            }
            std::printf("selftest file menu ok: new document from menu\n");
        }

        // Narrow window: the toolbar wraps onto more rows and grows taller.
        {
            const float wide_h = app.toolbar_h;
            SDL_SetWindowSize(app.window, 560, 720);
            for (int i = 0; i < 3; ++i) {
                pump();
                app.frame();
            }
            const float narrow_h = app.toolbar_h;
            const auto dot_pos = screenshot.find_last_of('.');
            const std::string narrow_path = dot_pos == std::string::npos
                ? screenshot + "_narrow"
                : screenshot.substr(0, dot_pos) + "_narrow" + screenshot.substr(dot_pos);
            if (!save_frame(narrow_path)) {
                app.shutdown();
                return 1;
            }
            SDL_SetWindowSize(app.window, 1100, 720);
            for (int i = 0; i < 3; ++i) {
                pump();
                app.frame();
            }
            if (narrow_h <= wide_h || app.toolbar_h != wide_h) {
                std::fprintf(stderr, "selftest toolbar: wrap failed (wide %.0f, narrow %.0f, back %.0f)\n",
                             wide_h, narrow_h, app.toolbar_h);
                app.shutdown();
                return 1;
            }
            std::printf("selftest toolbar ok: wide %.0fpx, narrow %.0fpx, screenshot %s\n",
                        wide_h, narrow_h, narrow_path.c_str());
        }

        // Undo/redo. Typing in a row is one step, a delete is its own step, a new
        // edit after undo drops the redo branch.
        {
            md::Document base;
            base.blocks.push_back(md::Block{});
            base.blocks[0].inlines.push_back(md::Inline{"abc"});
            app.doc = base;
            app.editor.set_caret(md::Pos{0, 0, 3});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());

            auto text_of = [&]() {
                std::string s;
                for (const md::Inline& inl : app.doc.blocks[0].inlines) s += inl.text;
                return s;
            };
            auto text_event = [](const char* t) {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = t;
                return e;
            };
            auto key_event = [](SDL_Keycode key, SDL_Keymod mod) {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = key;
                e.key.mod = mod;
                return e;
            };
            const SDL_Keymod none = SDL_KMOD_NONE;
            const SDL_Keymod ctrl = SDL_KMOD_CTRL;

            bool ok = true;
            auto expect = [&](const char* what, const std::string& want) {
                if (text_of() != want) {
                    std::fprintf(stderr, "selftest undo: %s: got \"%s\", want \"%s\"\n",
                                 what, text_of().c_str(), want.c_str());
                    ok = false;
                }
            };

            app.dispatch(text_event("x"));
            app.dispatch(text_event("y"));
            expect("typing", "abcxy");
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo typing run", "abc");
            app.dispatch(key_event(SDLK_Y, ctrl));
            expect("redo", "abcxy");
            app.dispatch(key_event(SDLK_Z, ctrl));
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo to start", "abc");
            app.dispatch(text_event("z"));
            app.dispatch(key_event(SDLK_Y, ctrl));
            expect("new edit drops redo", "abcz");
            app.dispatch(key_event(SDLK_BACKSPACE, none));
            expect("backspace", "abc");
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo backspace", "abcz");
            if (app.editor.caret().offset != 4) {
                std::fprintf(stderr, "selftest undo: caret not restored (%d)\n",
                             app.editor.caret().offset);
                ok = false;
            }
            app.dispatch(key_event(SDLK_Z, ctrl | SDL_KMOD_SHIFT));
            expect("shift+ctrl+z redo", "abc");

            if (!ok) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest undo ok: typing runs, deletes, redo, caret\n");
        }

        // Tables: cells take the caret and typing, Tab and Enter move between them,
        // Tab past the last cell adds a row, row and column commands keep the grid
        // rectangular, pipes are escaped, clicks land in the right cell, and the
        // table tab is locked in CommonMark.
        {
            md::Document base;
            base.blocks.push_back(md::Block{});
            base.blocks[0].inlines.push_back(md::Inline{"x"});
            app.doc = base;
            app.doc_dirty = true;
            app.dialect = 1;
            app.editor.set_caret(md::Pos{0, 0, 1});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.frame();

            auto key_event = [](SDL_Keycode k) {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = k;
                e.key.mod = SDL_KMOD_NONE;
                return e;
            };
            auto text_event = [](const char* t) {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = t;
                return e;
            };
            auto table_index = [&]() {
                for (size_t i = 0; i < app.doc.blocks.size(); ++i) {
                    if (app.doc.blocks[i].type == md::BlockType::Table) return static_cast<int>(i);
                }
                return -1;
            };
            auto table_rows = [&]() { return md::table_rows(app.doc.blocks[table_index()]); };
            auto table_cols = [&]() { return md::table_cols(app.doc.blocks[table_index()]); };
            auto cell_text = [&](int r, int c) {
                const md::Block& t = app.doc.blocks[table_index()];
                std::string s;
                for (const md::Inline& inl : t.cells[r * md::table_cols(t) + c]) s += inl.text;
                return s;
            };
            auto click = [&](float x, float y) {
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, x, y);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, x, y);
                pump();
                app.frame();
            };

            bool ok = true;
            auto expect = [&](const char* what, bool cond) {
                if (!cond) {
                    std::fprintf(stderr, "selftest table: %s failed\n", what);
                    ok = false;
                }
            };

            app.editor.insert_table(2, 2);
            app.dispatch(text_event("a"));
            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(text_event("b"));
            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(text_event("c"));
            app.dispatch(key_event(SDLK_RETURN));
            app.dispatch(text_event("d"));
            expect("typing and Enter", cell_text(0, 0) == "a" && cell_text(0, 1) == "b"
                && cell_text(1, 0) == "c" && cell_text(2, 0) == "d" && table_rows() == 3);

            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(key_event(SDLK_TAB));
            expect("Tab past the last cell adds a row",
                   table_rows() == 4 && app.editor.caret().cell == 6);

            app.dispatch(text_event("e"));
            app.dispatch(key_event(SDLK_BACKSPACE));
            const size_t blocks = app.doc.blocks.size();
            app.dispatch(key_event(SDLK_BACKSPACE));
            expect("backspace at cell start keeps the table",
                   app.doc.blocks.size() == blocks && table_index() >= 0 && cell_text(3, 0).empty());

            app.editor.table_add_column();
            expect("add column", table_cols() == 3 && cell_text(0, 2) == "b");
            app.editor.table_remove_column();
            expect("remove column", table_cols() == 2 && cell_text(0, 1) == "b");
            app.editor.table_remove_row();
            expect("remove row", table_rows() == 3);

            app.editor.set_caret(md::Pos{table_index(), 0, 1, 0});
            app.dispatch(text_event("|"));
            expect("pipe is escaped on export",
                   md::export_markdown(app.doc, true).find("a\\|") != std::string::npos);

            app.scroll_y = 0.0f;
            app.frame();
            const md::TableGrid& grid = app.layout.tables[0];
            click(app.doc_viewport.x + (grid.xs[1] + grid.xs[2]) / 2.0f,
                  app.doc_viewport.y - app.scroll_y + (grid.ys[1] + grid.ys[2]) / 2.0f);
            expect("click lands in cell (1,1)",
                   app.editor.caret().cell == 3 && app.editor.caret().block == table_index());

            const ui::Rect theme_btn = app.theme_rect;
            click(theme_btn.x + theme_btn.w / 2.0f, theme_btn.y + theme_btn.h / 2.0f);
            expect("theme toggles to light", app.light_theme && ui::is_light_palette(app.ui.palette));
            click(theme_btn.x + theme_btn.w / 2.0f, theme_btn.y + theme_btn.h / 2.0f);
            expect("theme toggles back to dark", !app.light_theme && !ui::is_light_palette(app.ui.palette));

            const ui::Rect lang_btn = app.lang_rect;
            click(lang_btn.x + lang_btn.w / 2.0f, lang_btn.y + lang_btn.h / 2.0f);
            expect("lang toggles to EN", app.ui_lang == 1 && app.en());
            click(lang_btn.x + lang_btn.w / 2.0f, lang_btn.y + lang_btn.h / 2.0f);
            expect("lang toggles back to RU", app.ui_lang == 0 && !app.en());

            const ui::Rect help_btn = app.help_rect;
            click(help_btn.x + help_btn.w / 2.0f, help_btn.y + help_btn.h / 2.0f);
            expect("welcome modal opens via ? button", app.welcome_open);
            push_key_event(SDLK_ESCAPE);
            pump();
            app.frame();
            expect("welcome modal closes via Escape", !app.welcome_open);

            const ui::Rect table_tab = app.tab_rects[1];
            click(table_tab.x + table_tab.w / 2.0f, table_tab.y + table_tab.h / 2.0f);
            expect("table tab opens in GFM", app.tools_tab == 1);

            app.dialect = 0;
            app.frame();
            click(table_tab.x + table_tab.w / 2.0f, table_tab.y + table_tab.h / 2.0f);
            expect("table tab is locked in CommonMark", app.tools_tab == 0);
            app.dialect = 1;
            app.tools_tab = 1;
            app.editor.set_caret(md::Pos{table_index(), 0, 0, 1});
            const auto table_dot = screenshot.find_last_of('.');
            save_frame(table_dot == std::string::npos
                ? screenshot + "_table"
                : screenshot.substr(0, table_dot) + "_table" + screenshot.substr(table_dot));

            app.editor.table_remove();
            app.mark_edited();
            app.frame();
            expect("table remove keeps document valid", table_index() == -1 && !app.doc.blocks.empty());

            if (!ok) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest table ok: typing, Tab/Enter, rows, columns, pipes, click, lock\n");
        }

        // Open-from-path, Drag & Drop, Chart editor slots/limits, Formula nested delete,
        // Scrollbar drag, Double-click at document end, and Pandoc PDF/EPUB export.
        {
            const std::string fixtures_dir = std::string(PROJECT_SOURCE_DIR) + "/tests/fixtures/";
            app.modified = false;

            // 1. open_from_path: valid file, missing file, empty file
            if (app.open_from_path(fixtures_dir + "gfm.md", true, false) != App::OpenResult::Opened
                || app.dialect != 1 || app.welcome_open) {
                std::fprintf(stderr, "selftest: open_from_path gfm.md failed\n");
                app.shutdown();
                return 1;
            }
            const std::string prev_path = app.file_path;
            if (app.open_from_path(fixtures_dir + "does_not_exist.md", true, false) != App::OpenResult::Error
                || app.file_path != prev_path || app.dialect != 1) {
                std::fprintf(stderr, "selftest: open_from_path missing file corrupted state\n");
                app.shutdown();
                return 1;
            }
            const std::string empty_md = std::string(PROJECT_SOURCE_DIR) + "/build/empty_check.md";
            md::write_text_file(empty_md, "");
            if (app.open_from_path(empty_md, true, false) != App::OpenResult::Opened
                || app.doc.blocks.empty()) {
                std::fprintf(stderr, "selftest: open_from_path empty file failed\n");
                app.shutdown();
                return 1;
            }

            // 2. Drag & Drop: single valid file opens; multiple files and unsupported
            // types are rejected without replacing the document.
            {
                const std::string basic_md = fixtures_dir + "basic.md";
                SDL_Event eb{}; eb.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb);
                SDL_Event ef{}; ef.type = SDL_EVENT_DROP_FILE; ef.drop.data = basic_md.c_str();
                app.dispatch(ef);
                SDL_Event ec{}; ec.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec);
                if (app.file_path != basic_md || app.welcome_open) {
                    std::fprintf(stderr, "selftest: drag & drop single file failed\n");
                    app.shutdown();
                    return 1;
                }
                // Two files in one gesture: rejected as a whole, document kept.
                const std::string prev_path2 = app.file_path;
                SDL_Event eb2{}; eb2.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb2);
                SDL_Event e1{}; e1.type = SDL_EVENT_DROP_FILE; e1.drop.data = basic_md.c_str();
                app.dispatch(e1);
                const std::string other_md = fixtures_dir + "gfm.md";
                SDL_Event e2{}; e2.type = SDL_EVENT_DROP_FILE; e2.drop.data = other_md.c_str();
                app.dispatch(e2);
                SDL_Event ec2{}; ec2.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec2);
                if (app.file_path != prev_path2) {
                    std::fprintf(stderr, "selftest: multi-file drop replaced document\n");
                    app.shutdown();
                    return 1;
                }
                // Unsupported extension: rejected, document kept.
                SDL_Event eb3{}; eb3.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb3);
                const std::string png = fixtures_dir + "not_a_markdown.png";
                SDL_Event e3{}; e3.type = SDL_EVENT_DROP_FILE; e3.drop.data = png.c_str();
                app.dispatch(e3);
                SDL_Event ec3{}; ec3.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec3);
                if (app.file_path != prev_path2) {
                    std::fprintf(stderr, "selftest: unsupported drop replaced document\n");
                    app.shutdown();
                    return 1;
                }
                // A new gesture after rejections must still open files.
                SDL_Event eb4{}; eb4.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb4);
                SDL_Event e4{}; e4.type = SDL_EVENT_DROP_FILE; e4.drop.data = other_md.c_str();
                app.dispatch(e4);
                SDL_Event ec4{}; ec4.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec4);
                if (app.file_path != other_md) {
                    std::fprintf(stderr, "selftest: drop after rejections failed\n");
                    app.shutdown();
                    return 1;
                }
            }

            // 3. Chart editor: slots selectable when 0/empty, row cap at 12
            {
                app.chart_open_new();
                app.frame();
                for (int i = 0; i < 4; ++i) app.chart_add_row();
                app.chart_select(1, true); // value cell of an empty row
                app.frame();
                if (app.chart_slots.size() < 10 || app.chart_sel < 0 || !app.chart_sel_value) {
                    std::fprintf(stderr, "selftest: chart slots missing (%zu, sel=%d val=%d)\n",
                                 app.chart_slots.size(), app.chart_sel,
                                 app.chart_sel_value ? 1 : 0);
                    app.shutdown();
                    return 1;
                }
                for (int i = 0; i < 20; ++i) app.chart_add_row();
                if (app.chart_edit.labels.size() > App::kMaxChartRows) {
                    std::fprintf(stderr, "selftest: chart row limit not enforced (%zu)\n",
                                 app.chart_edit.labels.size());
                    app.shutdown();
                    return 1;
                }
                app.chart_apply();
                app.frame();
            }

            // 4. Formula editor: operator split & nested delete keeps outer formula intact
            {
                app.formula_open_new();
                app.frame();
                app.formula_insert_symbol("a");
                app.formula_insert_operator("+");
                app.formula_insert_node("frac");
                app.frame();
                app.formula_insert_symbol("x");
                app.frame();
                // First delete clears "x" inside numerator without deleting "a +" or the fraction
                app.formula_delete_element();
                app.frame();
                if (app.formula_edit.slots.size() < 3) {
                    std::fprintf(stderr, "selftest: formula delete wiped whole formula (%zu)\n",
                                 app.formula_edit.slots.size());
                    app.shutdown();
                    return 1;
                }
                // Second delete on empty numerator removes the fraction, keeping "a +"
                app.formula_delete_element();
                app.frame();
                if (app.formula_edit.slots.size() < 2) {
                    std::fprintf(stderr, "selftest: formula structure delete wiped siblings (%zu)\n",
                                 app.formula_edit.slots.size());
                    app.shutdown();
                    return 1;
                }
                app.formula_editor_open = false;
            }

            // 5. Double-click at the bottom when last block is a chart/code block creates a new paragraph
            {
                const size_t before_blocks = app.doc.blocks.size();
                app.scroll_y = app.max_scroll;
                app.frame();
                const float click_x = app.doc_viewport.x + 40.0f;
                const float click_y = app.doc_viewport.y + app.doc_viewport.h - 4.0f;
                SDL_Event md{};
                md.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                md.button.button = SDL_BUTTON_LEFT;
                md.button.x = click_x;
                md.button.y = click_y;
                md.button.clicks = 2;
                app.dispatch(md);
                SDL_Event mu{};
                mu.type = SDL_EVENT_MOUSE_BUTTON_UP;
                mu.button.button = SDL_BUTTON_LEFT;
                mu.button.x = click_x;
                mu.button.y = click_y;
                app.dispatch(mu);
                app.frame();
                if (app.doc.blocks.size() != before_blocks + 1
                    || app.doc.blocks.back().type != md::BlockType::Paragraph
                    || app.editor.caret().block != static_cast<int>(app.doc.blocks.size()) - 1) {
                    std::fprintf(stderr, "selftest: double-click at document end did not create paragraph\n");
                    app.shutdown();
                    return 1;
                }
            }

            // 6. Export EPUB and PDF via Pandoc (+ Typst) if available
            if (app.has_pandoc) {
                const std::string epub_out = std::string(PROJECT_SOURCE_DIR) + "/build/selftest_out.epub";
                if (!app.export_with_pandoc("epub", epub_out)) {
                    std::fprintf(stderr, "selftest: EPUB export failed\n");
                    app.shutdown();
                    return 1;
                }
                if (!app.typst_path.empty()) {
                    const std::string pdf_out = std::string(PROJECT_SOURCE_DIR) + "/build/selftest_out.pdf";
                    if (!app.export_with_pandoc("pdf", pdf_out)) {
                        std::fprintf(stderr, "selftest: PDF export with Typst failed\n");
                        app.shutdown();
                        return 1;
                    }
                }
                std::printf("selftest pandoc export ok: EPUB%s\n",
                            app.typst_path.empty() ? "" : " + PDF (Typst)");
            }

            std::printf("selftest v1.0.1 fixes ok: open_from_path, drop, chart, formula, dblclick\n");
        }

        std::printf("selftest edit ok: insert/split/merge/select/delete/roundtrip\n");
        app.shutdown();
        return 0;
    }

    // Handle CLI arguments for normal launch:
    // Accept at most one positional file path to open on startup.
    if (argc > 1) {
        std::vector<std::string> positional;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg.empty()) continue;
            if (arg.rfind("-", 0) == 0) {
                dialogs::show_error(std::string(app.tr("Неизвестный параметр запуска:\n",
                                                       "Unknown command-line option:\n")) + arg);
                app.shutdown();
                return 2;
            }
            positional.push_back(arg);
        }
        if (positional.size() > 1) {
            dialogs::show_error(app.tr("Укажите только один файл для открытия при запуске.",
                                       "Please specify only one file to open on startup."));
            app.shutdown();
            return 2;
        }
        if (positional.size() == 1) {
            if (app.open_from_path(positional[0], true, true) != App::OpenResult::Opened) {
                app.shutdown();
                return 1;
            }
        }
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            running = app.dispatch(e) && running;
        }
        app.frame();
    }
    app.shutdown();
    return 0;
}

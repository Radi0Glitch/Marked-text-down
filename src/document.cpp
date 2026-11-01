#include "document.hpp"

namespace md {

namespace {
Inline plain(std::string text) { return {std::move(text), 0}; }
Inline bold(std::string text) { return {std::move(text), StyleBold}; }
Inline italic(std::string text) { return {std::move(text), StyleItalic}; }
Inline code(std::string text) { return {std::move(text), StyleCode}; }
} // namespace

Document make_demo_document() {
    Document doc;
    auto add = [&](Block b) { doc.blocks.push_back(std::move(b)); };
    auto para = [&](std::vector<Inline> inls) { add(Block{BlockType::Paragraph, 0, std::move(inls)}); };
    auto heading = [&](int level, std::string text) {
        add(Block{BlockType::Heading, level, {plain(std::move(text))}});
    };
    auto item = [&](std::string text, int depth, bool ordered) {
        Block b{BlockType::ListItem, 0, {plain(std::move(text))}};
        b.depth = depth;
        b.ordered = ordered;
        add(std::move(b));
    };
    auto code_block = [&](std::string lang, std::string text) {
        Block b{BlockType::CodeBlock, 0, {plain(std::move(text))}};
        b.lang = std::move(lang);
        add(std::move(b));
    };

    add(Block{BlockType::Heading, 1, {plain("Marked-text-down")}});
    para({plain("Лёгкий "), bold("WYSIWYG"),
          plain("-редактор Markdown: вы работаете с отформатированным текстом, а на диске сохраняется обычный "),
          code(".md"), plain("-файл. Ниже показано всё, что умеет редактор. Этот текст можно менять или удалить.")});

    heading(2, "Форматирование");
    para({plain("Жирный: "), bold("жирный"), plain(", курсив: "), italic("курсив"),
          plain(", жирный курсив: "), Inline{"жирный курсив", StyleBold | StyleItalic},
          plain(", зачёркнутый: "), Inline{"зачёркнутый", StyleStrike},
          plain(", моноширинный: "), code("код"), plain(".")});
    para({plain("Ссылка: "), Inline{"Marked-text-down на GitHub", StyleLink,
          "https://github.com/Radi0Glitch/Marked-text-down"},
          plain(". Формула в строке: $E = mc^2$.")});
    heading(3, "Заголовок третьего уровня");

    heading(2, "Списки");
    item("Маркированный пункт", 0, false);
    item("Вложенный пункт", 1, false);
    item("Ещё один пункт", 0, false);
    item("Нумерованный пункт", 0, true);
    item("Второй нумерованный", 0, true);

    heading(2, "Цитата");
    add(Block{BlockType::Quote, 0, {italic("Простота — высшая форма изощрённости."),
                                    plain(" — Леонардо да Винчи")}});

    heading(2, "Блок кода");
    code_block("rust", "fn main() {\n    println!(\"Привет, мир!\");\n}");

    heading(2, "Таблица");
    {
        Block t;
        t.type = BlockType::Table;
        t.inlines.push_back(Inline{});
        t.aligns = {TableAlign::Left, TableAlign::Center, TableAlign::Right};
        const char* cells[][3] = {
            {"Формат", "Команда", "Статус"},
            {"PDF", "Файл › Экспорт", "готово"},
            {"DOCX", "Файл › Экспорт", "готово"},
            {"EPUB", "Файл › Экспорт", "готово"},
        };
        for (const auto& row : cells) {
            for (const char* cell : row) t.cells.push_back({plain(cell)});
        }
        add(std::move(t));
    }

    heading(2, "Формула");
    code_block("latex", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}");

    heading(2, "Диаграмма");
    code_block("mermaid", "graph TD\n    A[Черновик .md] --> B[Редактор]\n    B --> C[PDF и DOCX]\n    B --> D[GitHub]\n");

    heading(2, "График");
    code_block("mermaid", "xychart-beta\n    title \"Продажи\"\n    x-axis [\"Янв\", \"Фев\", \"Мар\", \"Апр\"]\n    y-axis 0 --> 40\n    bar [12, 18, 25, 34]\n");

    heading(2, "Линия");
    add(Block{BlockType::Rule, 0, {}});
    para({plain("Линия выше разделяет части документа.")});

    heading(2, "Прочее");
    item("Поиск и замена: Ctrl+F и Ctrl+H.", 0, false);
    item("Оглавление по заголовкам: Ctrl+Shift+T.", 0, false);
    item("Масштаб документа: Ctrl и колесо мыши, от 50% до 200%.", 0, false);
    item("Автосохранение: несохранённый текст копируется и предлагается к восстановлению.", 0, false);
    item("Недавние файлы в меню «Файл», счётчик слов в нижней строке.", 0, false);
    item("Светлая и тёмная темы, интерфейс на русском и английском языках.", 0, false);

    heading(2, "Горячие клавиши");
    para({plain("Ctrl+B — жирный, Ctrl+I — курсив, Ctrl+E — код, Ctrl+1…3 — заголовки, "
                "Ctrl+Shift+L — маркированный список, Ctrl+Shift+O — нумерованный, "
                "Ctrl+Shift+Q — цитата, Ctrl+Shift+E — блок кода, Ctrl+Shift+R — линия, "
                "Ctrl+Shift+M — формула в строке, Ctrl+Z и Ctrl+Y — отмена и повтор, "
                "Ctrl+S — сохранить, F1 — окно приветствия и справка.")});

    heading(2, "Экспорт");
    para({plain("Документ выгружается в DOCX, PDF, HTML, ODT и EPUB через меню «Файл» › «Экспорт». "
                "Для экспорта нужен установленный Pandoc, PDF строится через Typst.")});
    para({plain("Тема, язык и показ окна приветствия сохраняются автоматически.")});

    return doc;
}

} // namespace md

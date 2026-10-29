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

    doc.blocks.push_back(Block{BlockType::Heading, 1, {plain("Marked-text-down")}});

    doc.blocks.push_back(Block{BlockType::Paragraph, 0, {
        plain("Лёгкий "), bold("WYSIWYG"), plain("-редактор Markdown. Нажмите "),
        code("F1"), plain(" или кнопку "), italic("?"),
        plain(" в верхней панели для обзора возможностей."),
    }});

    doc.blocks.push_back(Block{BlockType::Heading, 2, {plain("Быстрый старт")}});

    doc.blocks.push_back(Block{BlockType::Paragraph, 0, {
        plain("Смешанные стили в одной строке: "),
        bold("жирный"), plain(", "), italic("курсив"), plain(", "),
        Inline{"жирный курсив", StyleBold | StyleItalic}, plain(" и "),
        code("моноширинный код"), plain("."),
    }});

    doc.blocks.push_back(Block{BlockType::Quote, 0, {
        italic("Простота — высшая форма изощрённости."),
        plain(" — Леонардо да Винчи"),
    }});

    doc.blocks.push_back(Block{BlockType::Heading, 2, {plain("Визуальные блоки и экспорт")}});

    doc.blocks.push_back(Block{BlockType::Paragraph, 0, {
        plain("Формулы, диаграммы, графики и таблицы GFM создаются и редактируются визуально — "
              "достаточно выбрать нужный инструмент на верхней панели или кликнуть по объекту в тексте. "
              "При сохранении создаётся чистый стандартный Markdown-файл, а через меню «Экспорт...» "
              "документ можно выгрузить в DOCX, PDF, HTML, ODT и EPUB."),
    }});

    doc.blocks.push_back(Block{BlockType::Heading, 3, {plain("Горячие клавиши")}});

    doc.blocks.push_back(Block{BlockType::Paragraph, 0, {
        plain("Основные сочетания: "), code("Ctrl+B"), plain(" — жирный, "),
        code("Ctrl+I"), plain(" — курсив, "), code("Ctrl+E"), plain(" — код, "),
        code("Ctrl+1..3"), plain(" — заголовки, "), code("Ctrl+S"), plain(" — сохранить, "),
        code("F1"), plain(" — окно приветствия и возможностей."),
    }});

    doc.blocks.push_back(Block{BlockType::Paragraph, 0, {
        plain("Переключайте светлую и тёмную тему, а также язык интерфейса (RU / EN) прямо в заголовке окна. "
              "Все настройки и выбор показа стартового окна автоматически сохраняются."),
    }});

    return doc;
}

} // namespace md

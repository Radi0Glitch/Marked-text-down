# Marked-text-down

Лёгкий кроссплатформенный визуальный (WYSIWYG) редактор Markdown. Вы работаете с живым отформатированным текстом и визуальными блоками без служебной разметки на экране, а на диске сохраняется чистый стандартный `.md`-файл.

**Стек:** C++17 · SDL3 · SDL3_ttf · md4c · tinyfiledialogs · собственный векторный мини-UI и движок вёрстки (без ImGui/Nuklear/WebView).

---

## Возможности

- **Живое форматирование (WYSIWYG)**:
  - Строчные стили: **жирный** (`Ctrl+B`), *курсив* (`Ctrl+I`), ~~зачёркнутый~~, `моноширинный код` (`Ctrl+E`) и их комбинации.
  - Блоки: заголовки 1–3 уровней (`Ctrl+1..3`), маркированные и нумерованные списки (с вложенностью через `Tab` / `Shift+Tab`), цитаты, блоки кода и горизонтальные разделители.
- **Визуальные конструкторы (без ввода кода)**:
  - **Математические формулы**: дроби, корни, степени, индексы, суммы, произведения, интегралы, матрицы, системы условий и греческие символы (в файле сохраняются как блоки `math`/`latex`).
  - **Диаграммы**: блок-схемы (`flowchart TD/LR` с разными фигурами узлов и подписями связей) и диаграммы последовательности (`sequenceDiagram`).
  - **Графики**: столбчатые, линейные (`xychart-beta`) и круговые (`pie`) диаграммы с интерактивным редактированием рядов.
  - Клик по любой формуле, диаграмме или графику в тексте открывает визуальный редактор этого блока.
- **Таблицы GitHub Flavored Markdown (GFM)**:
  - Добавление и удаление строк/столбцов, выравнивание содержимого ячеек (слева, по центру, справа), навигация по ячейкам клавишами `Tab` / `Shift+Tab` и `Enter`.
- **Два диалекта Markdown**:
  - Поддержка **CommonMark** и **GitHub Flavored Markdown** с автоматическим распознаванием диалекта при открытии файла.
- **Экспорт через Pandoc**:
  - Встроенная поддержка экспорта документа в **DOCX**, **PDF**, **HTML**, **ODT** и **EPUB** в один клик + справочная таблица совместимости расширений с популярными платформами (GitHub, GitLab, Notion, Obsidian, Reddit).
- **Удобный интерфейс для пользователя**:
  - Переключение **Тёмной** и **Светлой** темы оформления.
  - Переключение языка интерфейса (**Русский / English**).
  - Стартовое окно обзора возможностей (`F1` или кнопка `?`) с запоминанием настроек между запусками.
  - Полноценная история изменений (**Undo / Redo** — `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z`).

---

## Горячие клавиши

| Сочетание | Действие |
|---|---|
| `Ctrl+N` | Новый документ |
| `Ctrl+O` | Открыть `.md`-файл |
| `Ctrl+S` / `Ctrl+Shift+S` | Сохранить / Сохранить как... |
| `Ctrl+Z` / `Ctrl+Y` (`Ctrl+Shift+Z`) | Отменить / Повторить |
| `Ctrl+B` | Жирный шрифт |
| `Ctrl+I` | Курсив |
| `Ctrl+E` | Моноширинный код |
| `Ctrl+Shift+E` | Блок кода |
| `Ctrl+1`, `Ctrl+2`, `Ctrl+3` | Заголовок 1, 2, 3 уровня |
| `Ctrl+Shift+L` / `Ctrl+Shift+O` | Маркированный / Нумерованный список |
| `Ctrl+Shift+Q` | Цитата |
| `Ctrl+Shift+R` | Горизонтальная линия-разделитель |
| `Tab` / `Shift+Tab` | Следующая/предыдущая ячейка таблицы или уровень отступа списка |
| `F1` | Окно приветствия и возможностей |

---

## Сборка под Linux

Зависимости (Arch Linux):

```sh
sudo pacman -S sdl3 sdl3_ttf meson ninja zenity
```

Сборка и запуск:

```sh
meson setup build --buildtype=release
ninja -C build
./build/marked-text-down
```

---

## Кросс-компиляция под Windows и сборка NSIS-установщика

Для сборки Windows-версии (`x86_64`) на Linux используются `mingw-w64-gcc` и `nsis`:

```sh
sudo pacman -S mingw-w64-gcc nsis
```

Сборка исполняемого файла с помощью Meson cross-file:

```sh
meson setup build-win --cross-file mingw-cross.ini --buildtype=release
ninja -C build-win
```

Упаковка установщика `Marked-text-down-Setup-x86_64.exe` через NSIS (из папки дистрибутива Windows с `marked-text-down.exe`, `SDL3.dll`, `SDL3_ttf.dll`, `fonts/` и `tools/pandoc/windows-x86_64/pandoc.exe`):

```sh
makensis installer.nsi
```

Готовые сборки располагаются в каталоге `dist/`:
- `dist/Marked-text-down-Setup-x86_64.exe` — установщик для Windows x86_64 (с поддержкой русского и английского языков, ярлыками и деинсталлятором).
- `dist/Marked-text-down-windows-x86_64/` — portable-версия для Windows.
- `dist/Marked-text-down-linux-x86_64.tar.gz` — архив сборки для Linux x86_64.

---

## Автотестирование без дисплея (Headless Selftest)

Проект включает встроенный набор автотестов (рендер, ввод, стили, списки, таблицы, импорт/экспорт Markdown, визуальные редакторы формул и диаграмм, история изменений, темы и локализация):

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
  ./build/marked-text-down --selftest selftest.bmp
```

---

## Шрифты и лицензии

- **Fira Sans** (Regular, Bold, Italic, BoldItalic) и **Fira Mono** (Regular, Bold) с поддержкой латиницы и кириллицы — SIL Open Font License (`fonts/OFL.txt`, `fonts/FiraMono-OFL.txt`).
- **md4c** — MIT License (`third_party/md4c/LICENSE.md`).
- **tinyfiledialogs** — zlib License (`third_party/tinyfiledialogs/`).

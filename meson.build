project(
  'Marked-text-down',
  ['c', 'cpp'],
  version: '0.1.0',
  default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=2'],
)

sdl3 = dependency('sdl3')
sdl3_ttf = dependency('sdl3-ttf')

sources = files(
  'src/main.cpp',
  'src/ui.cpp',
  'src/fontbook.cpp',
  'src/document.cpp',
  'src/layout.cpp',
  'src/render.cpp',
  'src/editor.cpp',
  'src/history.cpp',
  'src/markdown_io.cpp',
  'src/dialogs.cpp',
  'src/math_model.cpp',
  'src/math_draw.cpp',
  'src/diag_model.cpp',
  'src/diag_draw.cpp',
  'src/chart_model.cpp',
  'src/chart_draw.cpp',
)

vendored = files(
  'third_party/md4c/md4c.c',
  'third_party/tinyfiledialogs/tinyfiledialogs.c',
)

deps = [sdl3, sdl3_ttf]
if host_machine.system() == 'windows'
  cc = meson.get_compiler('c')
  deps += [
    cc.find_library('comdlg32'),
    cc.find_library('ole32'),
  ]
endif

executable(
  'marked-text-down',
  sources,
  vendored,
  dependencies: deps,
  include_directories: include_directories('third_party/md4c', 'third_party/tinyfiledialogs'),
  cpp_args: ['-DPROJECT_SOURCE_DIR="@0@"'.format(meson.project_source_root())],
  win_subsystem: 'windows',
)

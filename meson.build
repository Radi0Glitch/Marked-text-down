project(
  'Marked-text-down',
  ['c', 'cpp'],
  version: '1.2.0',
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
  'src/search.cpp',
  'src/dialogs.cpp',
  'src/window_state.cpp',
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

if host_machine.system() == 'windows'
  # Embed the application icon and version info into the exe.
  # windres runs on the build host, so an absolute source path works for both
  # native Windows builds and mingw cross-builds on Linux.
  windows = import('windows')
  rc_conf = configuration_data()
  rc_conf.set('ICON_PATH', meson.project_source_root() / 'packaging' / 'icons' / 'mtd-logo.ico')
  app_rc = configure_file(input: 'packaging/windows/app.rc.in',
                          output: 'app.rc',
                          configuration: rc_conf)
  windows_resources = windows.compile_resources(app_rc)
else
  windows_resources = []
endif

executable(
  'marked-text-down',
  sources + windows_resources,
  vendored,
  dependencies: deps,
  include_directories: include_directories('third_party/md4c', 'third_party/tinyfiledialogs'),
  cpp_args: ['-DPROJECT_SOURCE_DIR="@0@"'.format(meson.project_source_root())],
  win_subsystem: 'windows',
  install: true,
  link_args: host_machine.system() == 'windows'
    ? ['-static', '-static-libgcc', '-static-libstdc++']
    : [],
)

if host_machine.system() != 'windows'
  install_data('fonts/FiraSans-Regular.ttf', 'fonts/FiraSans-Bold.ttf',
               'fonts/FiraSans-Italic.ttf', 'fonts/FiraSans-BoldItalic.ttf',
               'fonts/FiraMono-Regular.ttf', 'fonts/FiraMono-Bold.ttf',
               'fonts/PT_Serif-Web-Regular.ttf', 'fonts/PT_Serif-Web-Bold.ttf',
               'fonts/PT_Serif-Web-Italic.ttf', 'fonts/PT_Serif-Web-BoldItalic.ttf',
               'fonts/NotoSansMath-Regular.ttf', 'fonts/NotoSans-Regular.ttf',
               install_dir: get_option('datadir') / 'marked-text-down' / 'fonts')
  install_data('marked-text-down.desktop',
               install_dir: get_option('datadir') / 'applications')
  # The .desktop file references Icon=marked-text-down, so install the logo
  # under that name (renamed copy generated in the build directory).
  app_icon = configure_file(input: 'mtd-logo.svg',
                            output: 'marked-text-down.svg',
                            copy: true)
  install_data(app_icon,
               install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps')
endif

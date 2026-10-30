#!/usr/bin/env python3
"""
trace_stroke.py — PNG → SVG с градиентом и свечением.
Зависимости: potrace (pacman -S potrace), Pillow (pip install Pillow)
Использование: python trace_stroke.py input.png [output.svg]
"""
import sys, subprocess, re, shutil
from pathlib import Path

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} input.png [output.svg]")
        sys.exit(1)

    inp = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else inp.with_suffix('.svg')

    if not shutil.which('potrace'):
        print("potrace не найден → sudo pacman -S potrace", file=sys.stderr)
        sys.exit(1)

    try:
        from PIL import Image
    except ImportError:
        print("Pillow не установлен → pip install Pillow", file=sys.stderr)
        sys.exit(1)

    # 1. PNG → ч/б маска (фон чёрный, мазок белый)
    img = Image.open(inp).convert('L')
    # Порог 40: всё что ярче — мазок. Подстрой под свой фон.
    mask = img.point(lambda x: 255 if x > 40 else 0, '1')
    pbm = inp.with_suffix('.pbm')
    mask.save(pbm)

    # 2. potrace → сырой SVG
    raw_svg = inp.with_suffix('.raw.svg')
    subprocess.run(
        ['potrace', str(pbm), '-s', '-o', str(raw_svg),
         '--turdsize', '2', '--alphamax', '1.3', '--opttolerance', '0.2'],
        check=True
    )

    # 3. Оборачиваем: градиент + свечение
    svg = raw_svg.read_text()

    defs = """<defs>
    <linearGradient id="g" x1="0%" y1="100%" x2="100%" y2="0%">
      <stop offset="0%"   stop-color="#00e5ff"/>
      <stop offset="50%"  stop-color="#7c4dff"/>
      <stop offset="100%" stop-color="#ff4081"/>
    </linearGradient>
    <filter id="glow" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur in="SourceGraphic" stdDeviation="4"  result="b1"/>
      <feGaussianBlur in="SourceGraphic" stdDeviation="12" result="b2"/>
      <feMerge>
        <feMergeNode in="b2"/>
        <feMergeNode in="b1"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>"""

    svg = re.sub(r'(<svg[^>]*>)', r'\1\n' + defs, svg, count=1)

    # Заменяем все path: убираем fill, ставим stroke+filter
    def style_path(m):
        attrs = m.group(1)
        # Убираем старые fill/stroke
        attrs = re.sub(r'\s*fill="[^"]*"', '', attrs)
        attrs = re.sub(r'\s*stroke="[^"]*"', '', attrs)
        return (f'<path{attrs} fill="none" '
                f'stroke="url(#g)" stroke-width="24" '
                f'stroke-linecap="round" stroke-linejoin="round" '
                f'filter="url(#glow)"/>')

    svg = re.sub(r'<path([^>]*)/>', style_path, svg)
    # На случай если path с закрывающим тегом
    svg = re.sub(r'<path([^>]*)></path>', style_path, svg)

    out.write_text(svg)

    # Чистка
    pbm.unlink(missing_ok=True)
    raw_svg.unlink(missing_ok=True)

    print(f"→ {out}")

if __name__ == '__main__':
    main()
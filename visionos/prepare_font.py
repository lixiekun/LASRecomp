"""Download the OFL Noto Sans SC font and create a static TrueType fallback.

Run with a Python environment containing fonttools. No Nintendo font is used.
"""
import hashlib
from pathlib import Path
import subprocess
import tempfile

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

FONT_REV = '2894aab31764f10f29c421bdfd2340d3b382d384'
FONT_SHA256 = 'a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'
FONT_URL = f'https://raw.githubusercontent.com/google/fonts/{FONT_REV}/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf'


def main():
    dest = Path(__file__).resolve().parent / 'fonts/LASFallback-Regular.ttf'
    dest.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'NotoSansSC-variable.ttf'
        subprocess.run(['curl', '--fail', '--location', '--max-time', '120', FONT_URL, '-o', str(src)], check=True)
        if hashlib.sha256(src.read_bytes()).hexdigest() != FONT_SHA256:
            raise RuntimeError('Downloaded font does not match the pinned source')
        with TTFont(src) as font:
            # The game's font engine requires TrueType outlines and a static face.
            static = instantiateVariableFont(font, {'wght': 400}, inplace=True)
            static.save(dest)
    print(f'Prepared {dest.name}; license: visionos/fonts/OFL.txt')


if __name__ == '__main__':
    main()

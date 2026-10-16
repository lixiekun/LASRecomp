"""Stage the owner's private resources into a locally built app, before signing.

Usage: python3 visionos/stage_game.py /path/to/LASVision.app
The generated game code is already statically linked by Xcode.
"""
from pathlib import Path
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
app = Path(sys.argv[1]).resolve()
assert app.is_dir() and app.suffix == '.app', 'Build the application first'
source = repo / 'extracted'
font_dir = repo / 'visionos/fonts'
assert (font_dir / 'LASFallback-Regular.ttf').is_file(), 'Run visionos/prepare_font.py with a Python environment containing fonttools first'
assert (source / 'romfs.bin').is_file(), 'Prepare the private RomFS image first'
with (source / 'exefs/main').open('rb') as f:
    f.seek(0x40)
    assert f.read(8).hex() == 'ae16f71e002af8cb', 'Wrong game version'


def copy_file(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() and dst.stat().st_size == src.stat().st_size and dst.stat().st_mtime_ns == src.stat().st_mtime_ns:
        return
    if dst.exists():
        dst.unlink()
    # APFS clones avoid duplicating the 6 GB image. Other filesystems use a regular copy.
    result = subprocess.run(['cp', '-cp', str(src), str(dst)], capture_output=True)
    if result.returncode:
        shutil.copy2(src, dst)


for src in (source / 'exefs').iterdir():
    if src.is_file():
        copy_file(src, app / 'game/exefs' / src.name)
copy_file(source / 'romfs.bin', app / 'game/romfs.bin')
# The game reads most resources from romfs.bin. The host's video decoder and
# cosmetic patcher also read this small set of resources as ordinary files.
for folder in ('region_common/movie', 'region_common/ui'):
    for src in (source / 'romfs' / folder).rglob('*'):
        if src.is_file():
            copy_file(src, app / 'game/romfs' / src.relative_to(source / 'romfs'))
for name in ('ItemBottle.bfres', 'Playerlink.bfres', 'ItemFairy.bfres', 'ItemMagicPowder.bfres'):
    src = source / 'romfs/region_common/actor' / name
    if src.exists():
        copy_file(src, app / 'game/romfs/region_common/actor' / name)
copy_file(font_dir / 'LASFallback-Regular.ttf', app / 'LASFallback-Regular.ttf')
copy_file(font_dir / 'OFL.txt', app / 'NotoSansCJK-OFL.txt')
copy_file(repo / 'third_party/fonts/Baloo2/Baloo2-SemiBold.ttf', app / 'Baloo2-SemiBold.ttf')
copy_file(repo / 'third_party/fonts/Baloo2/OFL.txt', app / 'Baloo2-OFL.txt')
copy_file(repo / 'LICENSE', app / 'LASRecomp-LICENSE.txt')
print('Private game resources staged. Re-sign the app before device installation.')

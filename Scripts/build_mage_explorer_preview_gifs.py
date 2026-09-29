from pathlib import Path
from PIL import Image


ROOT = Path(__file__).resolve().parent.parent / "SourceAssets" / "KayKitMageExplorer"


def build(name: str, output: str, step: int = 2, duration_ms: int = 66) -> None:
    paths = sorted((ROOT / "PreviewFrames" / name).glob("Frame_*.png"))[::step]
    if not paths:
        raise RuntimeError(f"No rendered frames for {name}")
    frames = [Image.open(path).convert("RGB") for path in paths]
    frames[0].save(
        ROOT / output,
        save_all=True,
        append_images=frames[1:],
        duration=duration_ms,
        loop=0,
        optimize=True,
    )
    for frame in frames:
        frame.close()
    print(output, len(paths))


build("ReadBook", "MageExplorer_ReadBook.gif")
build("WandDig", "MageExplorer_WandDig.gif")
build("Run", "MageExplorer_Run.gif")

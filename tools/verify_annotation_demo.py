# Smoke test for the annotation-family demo: (1) launches the app aimed at
# the dimension cluster and dumps a screenshot, (2) saves the demo document
# through GRID_SAVE_DWG, (3) re-imports it through GRID_DWG expecting
# "(0 unsupported)", and (4) dumps a screenshot of the imported picture.
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXECUTABLE = ROOT / "build2022" / "bin" / "Release" / "WINDOW.exe"
OUT = ROOT / "graphify-out" / "annotation-verify"
OUT.mkdir(parents=True, exist_ok=True)

# demoAnchor = (1e7 + 1536, 0, 1e7 - 1024); cluster at demoAnchor + (2304, 0).
CAMERA = "10004190,260,9998976,2100"


def launch(name, env_extra, tokens, timeout_seconds=40):
    env = os.environ.copy()
    env.update({
        "WINDOW_RENDERER": "dx11",
        "GRID_SCREENSHOT": str(OUT / f"{name}.bmp"),
        "GRID_CAMERA_START_TARGET": CAMERA,
    })
    env.update(env_extra)
    print(f"[{name}] launching", flush=True)
    process = subprocess.Popen(
        [str(EXECUTABLE)],
        cwd=str(ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    seen = {}
    # A reader thread sidesteps python's read-ahead buffering on pipes:
    # the app keeps its stdout open for its whole lifetime, so the
    # for-line-in-pipe idiom would stall forever without it.
    def pump():
        for line in process.stdout:
            line = line.strip()
            for token in tokens:
                if token in line and token not in seen:
                    seen[token] = line
                    print(f"  {line}", flush=True)

    reader = threading.Thread(target=pump, daemon=True)
    reader.start()
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        time.sleep(0.5)
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
    reader.join(timeout=5)
    return seen


def main():
    camera_token = "Camera start target"

    # 1) authored demo picture.
    launch("authored", {"GRID_START_STYLE": "3"}, [camera_token])
    ok_authored = (OUT / "authored.bmp").exists()

    # 2) save the demo document as DWG.
    dwg_path = OUT / "annotation_demo.dwg"
    dwg_path.unlink(missing_ok=True)
    seen = launch("save", {"GRID_SAVE_DWG": str(dwg_path)},
                  [camera_token, "GRID_SAVE_DWG"])
    ok_saved = seen.get("GRID_SAVE_DWG", "").startswith("GRID_SAVE_DWG: wrote")
    ok_saved = ok_saved and dwg_path.exists()

    # 3) re-import the saved file: zero unsupported entities expected.
    seen = launch("imported", {"GRID_DWG": str(dwg_path)},
                  [camera_token, "GRID_DWG:"])
    import_line = seen.get("GRID_DWG:", "")
    ok_imported = "(0 unsupported)" in import_line

    print()
    print(f"authored screenshot: {'OK' if ok_authored else 'FAIL'}")
    print(f"save DWG:            {'OK' if ok_saved else 'FAIL'}")
    print(f"re-import 0 skipped: {'OK' if ok_imported else 'FAIL'} "
          f"({import_line})")
    return 0 if (ok_authored and ok_saved and ok_imported) else 1


if __name__ == "__main__":
    sys.exit(main())

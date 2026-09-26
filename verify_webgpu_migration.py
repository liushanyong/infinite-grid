import math
import os
import re
import subprocess
import sys
from pathlib import Path


STATE_PATTERN = re.compile(
    r"Camera Target: \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)\s+"
    r"\[(PERSP|ORTHO)\]\s+near=([0-9.eE+-]+)\s+far=([0-9.eE+-]+)"
)
BACKEND_PATTERN = re.compile(
    r"Renderer backend: (bgfx-webgpu-migration \((Direct3D11|Direct3D12)\)"
    r"|webgpu-native \(WebGPU\))"
)


def main() -> None:
    configuration = sys.argv[1] if len(sys.argv) > 1 else "Release"
    executable = (Path(__file__).parent / "build2022" / "bin" /
                  configuration / "WINDOW.exe")
    if not executable.exists():
        raise RuntimeError(f"missing executable: {executable}")

    env = os.environ.copy()
    env.update({
        "WINDOW_RENDERER": "webgpu",
        "GRID_STRESS_COUNT": "100",
        "GRID_CAMERA_TEST_PAN_X": "80",
        "GRID_CAMERA_TEST_PAN_AT_SECONDS": "2",
    })

    states = []
    active_api = None
    process = subprocess.Popen(
        [str(executable)],
        cwd=str(executable.parent.parent.parent.parent),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    try:
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            match = BACKEND_PATTERN.search(line)
            if match:
                active_api = match.group(2) or "WebGPU"
            state = STATE_PATTERN.search(line)
            if state:
                states.append(state)
                if len(states) >= 2:
                    break
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

    if active_api is None:
        raise RuntimeError("WebGPU migration backend did not initialize")
    if len(states) < 2:
        raise RuntimeError("camera output did not contain before/after states")

    targets = [tuple(float(value) for value in state.group(1, 2, 3))
               for state in states]
    near_values = [float(state.group(5)) for state in states]
    far_values = [float(state.group(6)) for state in states]
    assert all(math.isfinite(value) for target in targets for value in target)
    assert all(math.isfinite(value) and value > 0.0 for value in near_values)
    assert all(math.isfinite(value) for value in far_values)
    assert near_values[0] == near_values[1]
    assert far_values[0] == far_values[1]
    assert targets[0] != targets[1]
    print(f"verify_webgpu_migration: PASS ({active_api} compatibility path)")


if __name__ == "__main__":
    main()

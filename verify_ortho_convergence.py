import math
import os
import queue
import re
import subprocess
import sys
import threading
import time
from pathlib import Path


STATE_PATTERN = re.compile(
    r"Camera Target: \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)\s+"
    r"\[(PERSP|ORTHO)\]\s+near=([0-9.eE+-]+)\s+far=([0-9.eE+-]+)"
)
SCENARIO_MARKER = "Camera test: origin orthographic convergence"


def parse_camera_state(line):
    match = STATE_PATTERN.search(line)
    if not match:
        return None
    values = [float(match.group(index)) for index in (1, 2, 3, 5, 6)]
    return {
        "target": tuple(values[0:3]),
        "projection": match.group(4),
        "near": values[3],
        "far": values[4],
    }


def main():
    configuration = sys.argv[1] if len(sys.argv) > 1 else "Debug"
    executable = (Path(__file__).parent / "build2022" / "bin" /
                  configuration / "WINDOW.exe")
    if not executable.exists():
        raise RuntimeError(f"missing executable: {executable}")

    env = os.environ.copy()
    env.update({
        "WINDOW_RENDERER": "dx11",
        "GRID_STRESS_COUNT": "1",
        "GRID_CAMERA_TEST_ORIGIN_ORTHO_AT_SECONDS": "1",
    })
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

    lines = queue.Queue()
    def read_output():
        assert process.stdout is not None
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()

    scenario_seen = False
    scenario_time = None
    state = None
    deadline = time.monotonic() + 6.0
    try:
        while time.monotonic() < deadline:
            try:
                line = lines.get(timeout=0.1)
            except queue.Empty:
                continue
            if line is None:
                break
            sys.stdout.write(line)
            if SCENARIO_MARKER in line:
                scenario_seen = True
                scenario_time = time.monotonic()
                continue
            parsed = parse_camera_state(line)
            if parsed:
                state = parsed
            if (scenario_time is not None and
                    time.monotonic() - scenario_time >= 2.0):
                break
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

    if not scenario_seen:
        raise RuntimeError("origin orthographic test scenario did not run")
    if state is None:
        raise RuntimeError("scenario did not emit a final camera state")

    assert state["projection"] == "ORTHO", state
    assert all(math.isfinite(value) and abs(value) < 1.0e-4
               for value in state["target"]), state
    assert math.isfinite(state["near"]) and state["near"] > -1.0e5, state
    assert math.isfinite(state["far"]) and state["far"] < 1.0e5, state
    assert state["far"] > state["near"], state
    print("verify_ortho_convergence: PASS")


if __name__ == "__main__":
    main()

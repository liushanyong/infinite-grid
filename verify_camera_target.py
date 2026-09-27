import math
import os
import re
import subprocess
import sys
from pathlib import Path


TARGET_PATTERN = re.compile(
    r"Camera Target: \((-?[0-9.]+), (-?[0-9.]+), (-?[0-9.]+)\)\s+"
    r"\[(PERSP|ORTHO)\]\s+near=([0-9.eE+-]+)\s+far=([0-9.eE+-]+)"
)


def parse_camera_state(line):
    match = TARGET_PATTERN.search(line)
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
    configuration = sys.argv[1] if len(sys.argv) > 1 else "Release"
    stress_count = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    executable = Path(__file__).parent / "build2022" / "bin" / configuration / "WINDOW.exe"
    if not executable.exists():
        raise RuntimeError(f"missing executable: {executable}")

    env = os.environ.copy()
    env.update({
        "WINDOW_RENDERER": "dx11",
        "GRID_STRESS_COUNT": str(stress_count),
        "GRID_CAMERA_TEST_PAN_X": "80",
        "GRID_CAMERA_TEST_PAN_AT_SECONDS": "2",
    })

    states = []
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
            state = parse_camera_state(line)
            if state:
                states.append(state)
                # Depth-slab stabilization can emit a second target log before
                # the scheduled pan.  Stop only after the pan has actually
                # moved the target, otherwise the regression compares two
                # pre-pan states and fails nondeterministically.
                if (len(states) >= 2 and
                        states[-1]["target"] != states[0]["target"]):
                    break
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

    if len(states) < 2:
        print(f"WINDOW exited with code {process.returncode}", file=sys.stderr)
        raise RuntimeError("camera output did not contain before/after pan states")

    before, after = states[0], states[-1]
    for state in (before, after):
        assert all(math.isfinite(value) for value in state["target"])
        assert math.isfinite(state["near"]) and state["near"] > 0.0
        assert math.isfinite(state["far"]) and state["far"] > state["near"]

    assert before["target"] != after["target"], "pan did not move the camera target"
    assert before["near"] == after["near"], "pan changed the near plane"
    assert before["far"] == after["far"], "pan changed the far plane"
    print("verify_camera_target: PASS")


if __name__ == "__main__":
    main()

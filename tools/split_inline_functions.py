"""Move non-template `inline` free functions from a header to a cpp.

Keeps templates, structs/classes and inline variables in the header;
emits declarations in place of the moved definitions.  The function body
opener is the first `{` OUTSIDE the parameter-list parentheses (so
brace-init default arguments like `= {}` are part of the signature, not
the body).  Brace matching ignores // comments and string/char literals.
"""
import io
import re
import sys


def strip_for_counting(line: str) -> str:
    """Remove string/char literals and // comments for brace counting."""
    out = []
    i = 0
    n = len(line)
    in_str = False
    quote = ""
    while i < n:
        ch = line[i]
        if in_str:
            if ch == "\\" and i + 1 < n:
                i += 2
                continue
            if ch == quote:
                in_str = False
            i += 1
            continue
        if ch in "\"'":
            in_str = True
            quote = ch
            i += 1
            continue
        if ch == "/" and i + 1 < n and line[i + 1] == "/":
            break
        out.append(ch)
        i += 1
    return "".join(out)


def split_header(header_path: str, cpp_path: str, include_line: str):
    lines = io.open(header_path, encoding="utf-8").read().split("\n")
    moved = []
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if re.match(r"^inline ", line):
            # Join raw lines until the body opener: the first '{' at paren
            # depth 0 (comment/string aware).
            j = i
            paren_depth = 0
            body_start_line = None
            body_start_col = None
            while j < n:
                stripped = strip_for_counting(lines[j])
                for col, ch in enumerate(stripped):
                    if ch == "(":
                        paren_depth += 1
                    elif ch == ")":
                        paren_depth -= 1
                    elif ch == "{" and paren_depth == 0:
                        body_start_line = j
                        body_start_col = col
                        break
                if body_start_line is not None:
                    break
                j += 1
            assert body_start_line is not None, f"no body for: {line}"

            # Signature = everything before the body opener (raw lines,
            # 'inline' stripped from the first line).  body_start_col is a
            # column within the LAST joined line, so slice that line only.
            sig_lines = lines[i : body_start_line + 1]
            sig_lines[-1] = sig_lines[-1][:body_start_col]
            sig_text = "\n".join(sig_lines).rstrip()
            sig_text = re.sub(r"^inline ", "", sig_text)
            sig_text = re.sub(r"\n +$", "", sig_text)
            sig_text = sig_text.rstrip()

            # Brace-match the body from the opener to its close.
            body_depth = 0
            k = body_start_line
            body_lines = []
            while k < n:
                stripped = strip_for_counting(lines[k])
                start = body_start_col if k == body_start_line else 0
                counted = stripped[start:]
                body_depth += counted.count("{") - counted.count("}")
                body_lines.append(lines[k])
                if body_depth <= 0:
                    break
                k += 1

            # Keep template functions in the header.
            p = i - 1
            while p >= 0 and not lines[p].strip():
                p -= 1
            is_template = p >= 0 and lines[p].strip().startswith("template")
            if is_template:
                out.extend(lines[i : k + 1])
            else:
                moved.append(sig_text + "\n" + "\n".join(body_lines))
                out.append(sig_text + ";")
            i = k + 1
            continue
        out.append(line)
        i += 1

    io.open(header_path, "w", encoding="utf-8", newline="").write("\n".join(out))
    cpp = include_line + "\n\n" + "\n\n".join(moved) + "\n"
    io.open(cpp_path, "w", encoding="utf-8", newline="").write(cpp)
    print(f"{header_path}: moved {len(moved)} functions -> {cpp_path}")


if __name__ == "__main__":
    split_header(sys.argv[1], sys.argv[2], sys.argv[3])

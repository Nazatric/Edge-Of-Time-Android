"""Strip CMake deprecation-warning blocks so real errors are visible in CI reports."""
import sys, pathlib

def main() -> int:
    path = pathlib.Path(sys.argv[1])
    if not path.exists():
        print(f"(missing {path})")
        return 0
    lines = path.read_text(errors="replace").splitlines()
    out, skip = [], 0
    for line in lines:
        if "CMake Deprecation Warning" in line:
            skip = 14            # swallow the warning block + its call stack
            continue
        if skip:
            skip -= 1
            if line.strip() == "":
                skip = 0
            continue
        out.append(line)
    print("\n".join(out))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

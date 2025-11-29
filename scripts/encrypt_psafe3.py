import argparse
import json
from getpass import getpass
from pathlib import Path

from psafe3_tools import write_psafe


def load_entries(path: str):
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    if isinstance(payload, dict) and "entries" in payload:
        entries = payload["entries"]
    else:
        entries = payload
    if not isinstance(entries, list):
        raise SystemExit("Input JSON must be a list of entries or an object with an 'entries' list")
    return entries


def main() -> None:
    parser = argparse.ArgumentParser(description="Encrypt JSON entries into a .psafe3 vault.")
    parser.add_argument("input", help="Path to JSON file (list or {\"entries\": [...]})")
    parser.add_argument("output", help="Path to output .psafe3 file")
    parser.add_argument("-p", "--password", help="Vault password (prompt if omitted)")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing output file")
    args = parser.parse_args()

    entries = load_entries(args.input)

    password = args.password or getpass("Vault password: ")
    if not password:
        raise SystemExit("Password is required")
    if not args.password:
        confirm = getpass("Repeat password: ")
        if password != confirm:
            raise SystemExit("Passwords do not match")

    try:
        write_psafe(entries, password, args.output, overwrite=args.overwrite)
    except FileExistsError as exc:
        raise SystemExit(str(exc))

    print(f"Encrypted vault written to {args.output}")


if __name__ == "__main__":
    main()

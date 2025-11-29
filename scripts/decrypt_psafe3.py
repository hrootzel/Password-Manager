import argparse
import json
from getpass import getpass
from pathlib import Path

from psafe3_tools import PasswordError, load_psafe_entries


def main() -> None:
    parser = argparse.ArgumentParser(description="Decrypt a .psafe3 vault and dump entries as JSON.")
    parser.add_argument("input", help="Path to input .psafe3 file")
    parser.add_argument("-o", "--output", help="Optional path to write JSON (stdout if omitted)")
    parser.add_argument("-p", "--password", help="Vault password (prompt if omitted)")
    args = parser.parse_args()

    password = args.password or getpass("Vault password: ")
    if not password:
        raise SystemExit("Password is required")

    try:
        entries = load_psafe_entries(args.input, password)
    except PasswordError:
        raise SystemExit("Password incorrect or vault corrupted.")

    payload = {"entries": entries}

    if args.output:
        Path(args.output).write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"Decrypted entries written to {args.output}")
    else:
        print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()

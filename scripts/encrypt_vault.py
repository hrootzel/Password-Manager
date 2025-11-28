import argparse
import json
from getpass import getpass

from vault_crypto import encrypt_vault, save_file


def main() -> None:
    parser = argparse.ArgumentParser(description="Encrypt plaintext JSON into a .vault file.")
    parser.add_argument("input", help="Path to input JSON file")
    parser.add_argument("-o", "--output", help="Path to output .vault file")
    parser.add_argument("-p", "--password", help="Master password (will prompt if omitted)")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        plaintext_json = f.read()

    # Validate JSON early to catch obvious issues
    json.loads(plaintext_json)

    password = args.password or getpass("Master password: ")
    if not password:
        raise SystemExit("Password is required")
    if not args.password:
        confirm = getpass("Repeat master password: ")
        if password != confirm:
            raise SystemExit("Passwords do not match.")

    blob = encrypt_vault(plaintext_json, password)
    save_file(args.output, blob)
    print(f"Encrypted vault written to {args.output}")


if __name__ == "__main__":
    main()

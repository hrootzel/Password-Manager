import argparse
import json
from getpass import getpass

from vault_crypto import decrypt_vault, load_file


def main() -> None:
    parser = argparse.ArgumentParser(description="Decrypt a .vault file to JSON.")
    parser.add_argument("input", help="Path to input .vault file")
    parser.add_argument("-o", "--output", help="Optional path to save decrypted JSON (stdout if omitted)")
    parser.add_argument("-p", "--password", help="Master password (will prompt if omitted)")
    args = parser.parse_args()

    blob = load_file(args.input)
    password = args.password or getpass("Master password: ")
    if not password:
        raise SystemExit("Password is required")

    plaintext_json, verified = decrypt_vault(blob, password)
    if not verified:
        raise SystemExit("Checksum mismatch: wrong password or corrupted vault.")

    # Ensure it is valid JSON
    parsed = json.loads(plaintext_json)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            json.dump(parsed, f, indent=2)
        print(f"Decrypted JSON written to {args.output}")
    else:
        print(json.dumps(parsed, indent=2))


if __name__ == "__main__":
    main()

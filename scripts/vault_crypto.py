import hashlib
import os
import secrets
from typing import Tuple

try:
    from Crypto.Cipher import AES  # pycryptodome
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pycryptodome required. Install with: pip install pycryptodome") from exc


SALT_SIZE = 16
CHECKSUM_SIZE = 32
KEY_SIZE = 16  # AES-128
PBKDF2_ITERATIONS = 10000
BLOCK_SIZE = 16


def derive_key(passphrase: str, salt: bytes) -> bytes:
    return hashlib.pbkdf2_hmac("sha256", passphrase.encode("utf-8"), salt, PBKDF2_ITERATIONS, KEY_SIZE)


def pad(data: bytes) -> bytes:
    pad_len = BLOCK_SIZE - (len(data) % BLOCK_SIZE)
    return data + bytes([pad_len]) * pad_len


def unpad(data: bytes) -> bytes:
    if not data:
        raise ValueError("No data to unpad")
    pad_len = data[-1]
    if pad_len == 0 or pad_len > BLOCK_SIZE:
        raise ValueError("Invalid padding")
    return data[:-pad_len]


def checksum(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()[:CHECKSUM_SIZE]


def encrypt_vault(plaintext_json: str, passphrase: str) -> bytes:
    salt = secrets.token_bytes(SALT_SIZE)
    key = derive_key(passphrase, salt)
    cipher = AES.new(key, AES.MODE_ECB)

    payload = plaintext_json.encode("utf-8")
    encrypted = cipher.encrypt(pad(payload))
    return salt + checksum(payload) + encrypted


def decrypt_vault(blob: bytes, passphrase: str) -> Tuple[str, bool]:
    if len(blob) < SALT_SIZE + CHECKSUM_SIZE:
        raise ValueError("Vault file too small")

    salt = blob[:SALT_SIZE]
    expected_checksum = blob[SALT_SIZE : SALT_SIZE + CHECKSUM_SIZE]
    encrypted = blob[SALT_SIZE + CHECKSUM_SIZE :]

    key = derive_key(passphrase, salt)
    cipher = AES.new(key, AES.MODE_ECB)

    decrypted = unpad(cipher.decrypt(encrypted))
    verified = checksum(decrypted) == expected_checksum
    return decrypted.decode("utf-8"), verified


def save_file(path: str, data: bytes) -> None:
    dirpath = os.path.dirname(path)
    if dirpath:
        os.makedirs(dirpath, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def load_file(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()

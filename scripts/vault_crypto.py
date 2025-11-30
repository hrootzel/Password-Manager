import hashlib
import os
import secrets
from dataclasses import dataclass
from typing import Tuple

try:
    from Crypto.Cipher import AES  # pycryptodome
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pycryptodome required. Install with: pip install pycryptodome") from exc

# Binary layout (version 2):
# [MAGIC(4)] [SALT(16)] [NONCE(12)] [TAG(16)] [CIPHERTEXT...]
MAGIC = b"VLT2"
SALT_SIZE = 16
NONCE_SIZE = 12
TAG_SIZE = 16
KEY_SIZE = 32  # AES-256
PBKDF2_ITERATIONS = 10_000


@dataclass
class VaultPayload:
    salt: bytes
    nonce: bytes
    tag: bytes
    ciphertext: bytes

    def to_bytes(self) -> bytes:
        return MAGIC + self.salt + self.nonce + self.tag + self.ciphertext

    @classmethod
    def from_bytes(cls, blob: bytes) -> "VaultPayload":
        header_len = len(MAGIC) + SALT_SIZE + NONCE_SIZE + TAG_SIZE
        if len(blob) < header_len:
            raise ValueError("Vault file too small")
        if blob[: len(MAGIC)] != MAGIC:
            raise ValueError("Invalid vault header")
        salt_offset = len(MAGIC)
        nonce_offset = salt_offset + SALT_SIZE
        tag_offset = nonce_offset + NONCE_SIZE
        salt = blob[salt_offset:nonce_offset]
        nonce = blob[nonce_offset:tag_offset]
        tag = blob[tag_offset : tag_offset + TAG_SIZE]
        ciphertext = blob[tag_offset + TAG_SIZE :]
        return cls(salt=salt, nonce=nonce, tag=tag, ciphertext=ciphertext)


def derive_key(passphrase: str, salt: bytes) -> bytes:
    return hashlib.pbkdf2_hmac(
        "sha256",
        passphrase.encode("utf-8"),
        salt,
        PBKDF2_ITERATIONS,
        KEY_SIZE,
    )


def encrypt_vault(plaintext_json: str, passphrase: str) -> bytes:
    salt = secrets.token_bytes(SALT_SIZE)
    nonce = secrets.token_bytes(NONCE_SIZE)
    key = derive_key(passphrase, salt)

    cipher = AES.new(key, AES.MODE_GCM, nonce=nonce, mac_len=TAG_SIZE)
    ciphertext, tag = cipher.encrypt_and_digest(plaintext_json.encode("utf-8"))

    payload = VaultPayload(salt=salt, nonce=nonce, tag=tag, ciphertext=ciphertext)
    return payload.to_bytes()


def decrypt_vault(blob: bytes, passphrase: str) -> Tuple[str, bool]:
    payload = VaultPayload.from_bytes(blob)
    key = derive_key(passphrase, payload.salt)

    cipher = AES.new(key, AES.MODE_GCM, nonce=payload.nonce, mac_len=TAG_SIZE)
    try:
        plaintext_bytes = cipher.decrypt_and_verify(payload.ciphertext, payload.tag)
    except ValueError:
        return "", False

    return plaintext_bytes.decode("utf-8"), True


def save_file(path: str, data: bytes) -> None:
    dirpath = os.path.dirname(path)
    if dirpath:
        os.makedirs(dirpath, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def load_file(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()

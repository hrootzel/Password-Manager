# Password Manager for ESP32

**Password Manager** designed for the ESP32. It provides **secure password storage, encryption, and USB emulation for typing passwords automatically**.
[Original by geo-tp ](https://github.com/geo-tp/Password-Manager/)


![](/image.jpg)

## Features

- **AES-128 Encryption**: All stored passwords are encrypted for security.
- **Storage on SD Card**: Encrypted password storage on an SD card for persistent data.
- **Random Password Generation**: Generate secure passwords.
- **HID Keyboard Mode**: ESP32 can act as a USB keyboard to automatically type credentials.
- **BLE Keyboard Mode**: ESP32 can act as a Bluetooth keyboard to automatically type credentials. Enable in Settings.
- **User Authentication**: Requires a master password to unlock stored credentials.
- **Auto-Lock Vault**: The vault will be automatically locked after a selected amount of time.

## Installation

- <b>M5Burner</b> : Search into M5CARDPUTER section and burn it
- <b>Github</b> : Build or take the firmware.bin from the [github release](https://github.com/hrootzel/Password-Manager/releases) and flash it

## Usage
- **Create a Vault**: Create a new encrypted vault to securely store your passwords. **Each vault is stored as an encrypted file on the SD card.**

- **Manage Password Entries**: Add, update, delete password entries to the vault. **Up to 100 passwords per vault.**

- **Auto-Type**: Select a field and press `OK`, the ESP32 type it via USB HID.

- **Update Settings**: Adjust app settings as keyboard layout, brightness and vault lock timings.

**NOTE:** You can update a vault name by modifying the filename in the `/vaults/` folder, the file extension must remains `.vault`. **The master password used to create0 a vault can't be modified.**

## CLI utilities

Python helper scripts live in `scripts/` (requires `pycryptodome`, see `requirements.txt`):

- `scripts/encrypt_vault.py`: encrypt a plaintext JSON file to `.vault`. Prompts for master password (with confirmation) or accept `-p/--password`; usage: `python scripts/encrypt_vault.py input.json output.vault`.
- `scripts/decrypt_vault.py`: decrypt a `.vault` to JSON and verify checksum; optionally save with `-o/--output` or print to stdout. Accepts `-p/--password` or prompts.

## Changes in this fork
- Vault encryption upgraded to AES-256-GCM with PBKDF2-HMAC-SHA256 (10k iterations), 16-byte salt, 12-byte nonce, 16-byte tag, and a `VLT2` header to prevent ECB-style block pattern leaks and add built-in integrity; old `.vault` files must be re-encrypted.
- Paranoid memory zeroing: keys, plaintext, passwords, and vault buffers are actively wiped after use on-device; vault buffers are cleared on destruction.
- Vault saves are atomic with a `.tmp` write-and-rename and preserve the previous version as `.bak` to avoid torn writes.

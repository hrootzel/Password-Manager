"""
Terminal UI for editing .vault files using the same AES-GCM scheme as the ESP32 firmware.
- Prompts for a vault file and password, decrypts, and lets you view/edit/delete/reorder entries.
- Saves back to the chosen output path (default: overwrite input) with the same encryption format.

Controls (in the list view):
  Up/Down or k/j : move selection
  Enter or e     : view/edit selected entry
  a              : add new entry
  d              : delete selected entry
  u              : move selected entry up
  n              : move selected entry down
  s              : save and exit
  q              : quit without saving
"""

import argparse
import curses
import json
from copy import deepcopy
from getpass import getpass
from typing import Dict, List, Tuple, Any

from vault_crypto import decrypt_vault, encrypt_vault, load_file, save_file


Entry = Dict[str, Any]


def load_entries(blob: bytes, password: str) -> Tuple[List[Entry], Dict[str, Any]]:
    plaintext, ok = decrypt_vault(blob, password)
    if not ok:
        raise ValueError("Wrong password or corrupted vault")
    data = json.loads(plaintext)
    extras: Dict[str, Any] = {}
    entries: List[Entry]
    if isinstance(data, list):
        entries = data
    elif isinstance(data, dict):
        entries = data.get("entries") or data.get("Entries") or []
        extras = {k: v for k, v in data.items() if k.lower() != "entries"}
    else:
        raise ValueError("Unsupported JSON structure")
    norm = []
    for e in entries:
        entry = dict(e)  # preserve unknown fields
        title = e.get("title") or e.get("Title") or e.get("serviceName") or ""
        entry["title"] = title
        entry["serviceName"] = e.get("serviceName") or title
        entry["username"] = e.get("username") or e.get("Username") or ""
        entry["password"] = e.get("password") or e.get("Password") or ""
        entry["notes"] = e.get("notes") or e.get("Notes") or ""
        norm.append(entry)
    return norm, extras


def serialize_entries(entries: List[Entry], extras: Dict[str, Any]) -> str:
    serialized = []
    for e in entries:
        out = dict(e)
        out["serviceName"] = e.get("title", e.get("serviceName", ""))
        out["title"] = e.get("title", "")
        out["username"] = e.get("username", "")
        out["password"] = e.get("password", "")
        out["notes"] = e.get("notes", "")
        serialized.append(out)

    payload = {"entries": serialized}
    payload.update(extras)
    return json.dumps(payload, indent=2)


def prompt_line(stdscr, prompt: str, default: str = "") -> Tuple[bool, str]:
    """
    Returns (confirmed, value). ESC cancels (confirmed=False), Enter confirms (empty keeps default).
    """
    curses.noecho()
    stdscr.clear()
    stdscr.addstr(0, 0, prompt)
    if default:
        stdscr.addstr(1, 0, f"Current: {default}")
    stdscr.addstr(2, 0, "New value (leave empty to keep current):")
    stdscr.refresh()

    win = curses.newwin(1, curses.COLS - 1, 3, 0)
    buffer: List[str] = []
    pos = 0
    while True:
        win.clear()
        win.addstr(0, 0, "".join(buffer))
        win.move(0, pos)
        win.refresh()
        key = win.getch()
        if key in (curses.KEY_ENTER, 10, 13):
            text = "".join(buffer).strip()
            return True, (text if text else default)
        if key == 27:  # ESC
            return False, default
        if key in (curses.KEY_BACKSPACE, 127, 8):
            if pos > 0:
                buffer.pop(pos - 1)
                pos -= 1
            continue
        if key in (curses.KEY_LEFT,):
            pos = max(0, pos - 1)
            continue
        if key in (curses.KEY_RIGHT,):
            pos = min(len(buffer), pos + 1)
            continue
        # Printable ASCII
        if 32 <= key <= 126:
            buffer.insert(pos, chr(key))
            pos += 1
            continue


def edit_entry_fields(stdscr, entry: Entry) -> Tuple[Entry, bool, bool]:
    """
    Detail view for a single entry.
    Returns (entry, changed, deleted).
    """
    edited = deepcopy(entry)
    fields = [
        ("serviceName", "Service/Title"),
        ("username", "Username"),
        ("password", "Password"),
        ("notes", "Notes"),
    ]
    selected = 0
    changed = False

    while True:
        stdscr.clear()
        stdscr.addstr(0, 0, "Entry detail (Up/Down select field, e=edit, d=delete entry, q=back)")
        for i, (key, label) in enumerate(fields):
            marker = ">" if i == selected else " "
            value = edited.get(key) or ""
            if key == "password" and i != selected:
                display = "*" * len(value)
            else:
                display = value.replace("\n", " ")[:60]
            stdscr.addstr(i + 2, 0, f"{marker} {label}: {display}")
        stdscr.refresh()

        key = stdscr.getch()
        if key in (curses.KEY_UP, ord("k")):
            selected = max(0, selected - 1)
        elif key in (curses.KEY_DOWN, ord("j")):
            selected = min(len(fields) - 1, selected + 1)
        elif key in (ord("q"), 27):  # q or ESC to go back
            return edited, changed, False
        elif key in (ord("d"),):
            curses.noecho()
            stdscr.clear()
            stdscr.addstr(0, 0, "Delete entry [Y/n]? ")
            stdscr.refresh()
            ch = stdscr.getch()
            if ch in (ord("y"), ord("Y"), 10, 13):
                return edited, changed, True
            else:
                continue
        elif key in (ord("e"), ord("\n"), curses.KEY_ENTER):
            key_name, label = fields[selected]
            confirmed, new_val = prompt_line(stdscr, f"{label}:", edited.get(key_name, ""))
            if confirmed and new_val != edited.get(key_name, ""):
                edited[key_name] = new_val
                if key_name == "serviceName":
                    edited["title"] = new_val
                if key_name == "title":
                    edited["serviceName"] = new_val
                changed = True


def draw_list(stdscr, entries: List[Entry], idx: int):
    stdscr.clear()
    stdscr.addstr(0, 0, "Vault entries (s=save, q=quit, a=add, d=del, e=detail, u=up, n=down)")
    for i, e in enumerate(entries):
        marker = ">" if i == idx else " "
        line = f"{marker} {i+1:02d}. {e.get('serviceName') or e.get('title','')[:40]}"
        stdscr.addstr(i + 2, 0, line)
    stdscr.refresh()


def tui(stdscr, entries: List[Entry]) -> Tuple[bool, List[Entry]]:
    idx = 0 if entries else -1
    modified = False
    while True:
        draw_list(stdscr, entries, idx if idx >= 0 else 0)
        key = stdscr.getch()
        if key in (curses.KEY_UP, ord("k")) and entries:
            idx = max(0, idx - 1)
        elif key in (curses.KEY_DOWN, ord("j")) and entries:
            idx = min(len(entries) - 1, idx + 1)
        elif key in (ord("q"),):
            return False, entries
        elif key in (ord("s"),):
            return True, entries
        elif key in (ord("a"),):
            new_entry = {"title": "", "serviceName": "", "username": "", "password": "", "notes": ""}
            edited, changed, deleted = edit_entry_fields(stdscr, new_entry)
            if not deleted and (changed or any(edited.values())):
                entries.append(edited)
                idx = len(entries) - 1
                modified = True
        elif key in (ord("d"),) and entries:
            curses.noecho()
            stdscr.clear()
            stdscr.addstr(0, 0, "Delete entry [Y/n]? ")
            stdscr.refresh()
            ch = stdscr.getch()
            if ch in (ord("y"), ord("Y"), 10, 13):
                entries.pop(idx)
                idx = min(idx, len(entries) - 1)
                modified = True
        elif key in (ord("e"), ord("\n"), curses.KEY_ENTER) and entries:
            updated, changed, deleted = edit_entry_fields(stdscr, entries[idx])
            if deleted:
                entries.pop(idx)
                idx = min(idx, len(entries) - 1)
                modified = True
            else:
                entries[idx] = updated
                modified = modified or changed
        elif key in (ord("u"),) and entries and idx > 0:
            entries[idx - 1], entries[idx] = entries[idx], entries[idx - 1]
            idx -= 1
            modified = True
        elif key in (ord("n"),) and entries and idx < len(entries) - 1:
            entries[idx + 1], entries[idx] = entries[idx], entries[idx + 1]
            idx += 1
            modified = True
        else:
            continue
    return modified, entries


def main():
    parser = argparse.ArgumentParser(description="TUI editor for .vault files.")
    parser.add_argument("input", help="Path to input .vault file")
    parser.add_argument("-o", "--output", help="Path to output .vault file (defaults to input)")
    parser.add_argument("-p", "--password", help="Vault password (prompt if omitted)")
    args = parser.parse_args()

    password = args.password or getpass("Vault password: ")
    if not password:
        raise SystemExit("Password is required")

    blob = load_file(args.input)
    entries, extras = load_entries(blob, password)

    modified, new_entries = curses.wrapper(tui, entries)
    if not modified:
        print("No changes saved.")
        return

    plaintext = serialize_entries(new_entries, extras)
    encrypted = encrypt_vault(plaintext, password)
    output_path = args.output or args.input
    save_file(output_path, encrypted)
    print(f"Saved updated vault to {output_path}")


if __name__ == "__main__":
    main()

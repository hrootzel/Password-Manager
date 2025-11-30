#!/usr/bin/env python
"""
Textual TUI for editing .vault files (AES-GCM via vault_crypto).
Pattern follows a simple list screen + edit screen (similar to keepass_browser style).
"""

from __future__ import annotations

import argparse
import json
import secrets
from dataclasses import dataclass, asdict
from getpass import getpass
from pathlib import Path
from typing import Dict, List, Optional
try:
    import pyperclip
except ImportError:
    pyperclip = None

from textual import on
from textual.app import App, ComposeResult
from textual.containers import Vertical, Horizontal
from textual.events import Key
from textual.screen import Screen, ModalScreen
from textual.widgets import Header, Footer, ListView, ListItem, Label, Button, Input, Static

from vault_crypto import decrypt_vault, encrypt_vault, load_file, save_file

# --- helpers ---------------------------------------------------------------


def generate_password(length: int = 20) -> str:
    alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$&*-_=+"
    return "".join(secrets.choice(alphabet) for _ in range(length))


# ---------------------------------------------------------------------------
# Data model & store using vault_crypto
# ---------------------------------------------------------------------------


@dataclass
class VaultEntry:
    id: Optional[int] = None
    serviceName: str = ""
    username: str = ""
    password: str = ""
    notes: str = ""
    order: int = 0

    @property
    def display(self) -> str:
        user = f"[{self.username}]" if self.username else ""
        return f"{self.serviceName} {user}"


class VaultStore:
    """Encrypted vault store backed by .vault file using vault_crypto."""

    def __init__(self, path: Path, password: str) -> None:
        self.path = path
        self.password = password
        self._entries: Dict[int, VaultEntry] = {}
        self._extras: Dict[str, object] = {}
        self._next_id: int = 1
        self.load()

    # --- persistence -------------------------------------------------------
    def load(self) -> None:
        if not self.path.exists():
            return
        blob_bytes = bytearray(load_file(self.path.as_posix()))
        plaintext, ok = decrypt_vault(bytes(blob_bytes), self.password)
        if not ok:
            # wipe and raise
            for i in range(len(blob_bytes)):
                blob_bytes[i] = 0
            raise ValueError("Wrong password or corrupted vault")
        data = json.loads(plaintext)
        # wipe raw buffers
        for i in range(len(blob_bytes)):
            blob_bytes[i] = 0
        plaintext = None  # type: ignore
        entries = data.get("entries") if isinstance(data, dict) else data
        if not isinstance(entries, list):
            raise ValueError("Invalid vault format")
        self._extras = {k: v for k, v in data.items() if k != "entries"} if isinstance(data, dict) else {}
        for idx, item in enumerate(entries):
            raw_id = item.get("id")
            try:
                parsed_id = int(raw_id) if raw_id is not None else None
            except (TypeError, ValueError):
                parsed_id = None
            raw_order = item.get("order")
            try:
                parsed_order = int(raw_order) if raw_order is not None else idx
            except (TypeError, ValueError):
                parsed_order = idx
            entry = VaultEntry(
                id=parsed_id,
                serviceName=item.get("serviceName") or item.get("title") or "",
                username=item.get("username", ""),
                password=item.get("password", ""),
                notes=item.get("notes", ""),
                order=parsed_order,
            )
            if entry.id is None:
                entry.id = self._next_id
            self._entries[entry.id] = entry
            if isinstance(entry.id, int):
                self._next_id = max(self._next_id, entry.id + 1)
        # best-effort wipe
        data = None  # type: ignore

    def save(self) -> None:
        data_entries = []
        for e in self.all():
            d = asdict(e)
            d["title"] = e.serviceName
            data_entries.append(d)
        payload = {"entries": data_entries, **self._extras}
        plaintext = json.dumps(payload, indent=2)
        blob = encrypt_vault(plaintext, self.password)
        save_file(self.path.as_posix(), blob)
        plaintext = None  # type: ignore
        blob = b""  # type: ignore

    # --- API used by the UI -----------------------------------------------
    def all(self) -> List[VaultEntry]:
        return sorted(self._entries.values(), key=lambda e: (e.order, e.id or 0))

    def get(self, entry_id: int) -> VaultEntry:
        return self._entries[entry_id]

    def add(self, entry: VaultEntry) -> VaultEntry:
        if entry.id is None:
            entry.id = self._next_id
        # place at end
        max_order = max((e.order for e in self._entries.values()), default=-1)
        entry.order = max_order + 1
        self._entries[entry.id] = entry
        self._next_id = max(self._next_id, entry.id + 1)
        self.save()
        return entry

    def update(self, entry: VaultEntry) -> None:
        if entry.id is None:
            raise ValueError("Cannot update entry without id")
        self._entries[entry.id] = entry
        self.save()

    def delete(self, entry_id: int) -> None:
        if entry_id in self._entries:
            del self._entries[entry_id]
            self.save()

    def reorder(self, entry_id: int, delta: int) -> None:
        items = self.all()
        idx_map = {e.id: i for i, e in enumerate(items) if e.id is not None}
        if entry_id not in idx_map:
            return
        idx = idx_map[entry_id]
        new_idx = idx + delta
        if new_idx < 0 or new_idx >= len(items):
            return
        items[idx], items[new_idx] = items[new_idx], items[idx]
        for i, e in enumerate(items):
            e.order = i
            self._entries[e.id] = e  # type: ignore[index]
        self.save()


# ---------------------------------------------------------------------------
# List item widget
# ---------------------------------------------------------------------------


class VaultItem(ListItem):
    """Represents a single entry in the ListView."""

    def __init__(self, entry: VaultEntry) -> None:
        super().__init__(Label(entry.display))
        self.entry_id = entry.id


# ---------------------------------------------------------------------------
# Screens
# ---------------------------------------------------------------------------


class VaultListScreen(Screen):
    """Main screen: list of entries with keybindings to add/edit/delete."""

    BINDINGS = [
        ("q", "app.quit", "Quit"),
        ("a", "add_entry", "Add"),
        ("e", "edit_entry", "Edit"),
        ("d", "delete_entry", "Delete"),
        ("u", "move_up", "Move Up"),
        ("n", "move_down", "Move Down"),
        ("s", "sort_entries", "Sort"),
    ]

    def compose(self) -> ComposeResult:
        yield Header()
        yield Vertical(
            Label("Vault entries (Enter or 'e' to edit, 'a' to add, 'd' to delete)"),
            ListView(id="vault-list"),
        )
        yield Footer()

    @property
    def store(self) -> VaultStore:
        return self.app.store  # type: ignore[attr-defined]

    def on_mount(self) -> None:
        self.refresh_list()

    def refresh_list(self) -> None:
        lv = self.query_one("#vault-list", ListView)
        lv.clear()
        for entry in self.store.all():
            lv.append(VaultItem(entry))

    def _open_editor(self, entry: VaultEntry) -> None:
        def callback(result: Optional[VaultEntry]) -> None:
            if result is not None:
                result.order = entry.order
                self.store.update(result)
                self.refresh_list()

        self.app.push_screen(EditEntryScreen(entry), callback)  # type: ignore[attr-defined]

    def _get_highlighted_item(self) -> Optional[VaultItem]:
        lv = self.query_one("#vault-list", ListView)
        if lv.index is None or not lv.children:
            return None
        try:
            item = lv.children[lv.index]
        except IndexError:
            return None
        return item if isinstance(item, VaultItem) else None

    # --- actions -----------------------------------------------------------
    def action_add_entry(self) -> None:
        new_entry = VaultEntry()
        new_entry.password = generate_password()

        def callback(result: Optional[VaultEntry]) -> None:
            if result is not None:
                self.store.add(result)
                self.refresh_list()

        self.app.push_screen(EditEntryScreen(new_entry), callback)  # type: ignore[attr-defined]

    def action_edit_entry(self) -> None:
        item = self._get_highlighted_item()
        if not item or item.entry_id is None:
            return
        entry = self.store.get(item.entry_id)
        self._open_editor(entry)

    def action_delete_entry(self) -> None:
        item = self._get_highlighted_item()
        if not item or item.entry_id is None:
            return
        confirm = ConfirmModal("Delete entry?")
        self.app.push_screen(confirm, lambda ok: self._handle_delete_confirm(ok, item.entry_id))  # type: ignore[attr-defined]

    def _handle_delete_confirm(self, ok: bool, entry_id: int) -> None:
        if ok:
            self.store.delete(entry_id)
            self.refresh_list()

    def action_move_up(self) -> None:
        item = self._get_highlighted_item()
        if not item or item.entry_id is None:
            return
        self.store.reorder(item.entry_id, -1)
        self.refresh_list()

    def action_move_down(self) -> None:
        item = self._get_highlighted_item()
        if not item or item.entry_id is None:
            return
        self.store.reorder(item.entry_id, 1)
        self.refresh_list()

    def action_sort_entries(self) -> None:
        # Toggle sort direction and reassign order based on case-insensitive title
        current = getattr(self, "_sort_desc", False)
        self._sort_desc = not current
        entries = sorted(self.store.all(), key=lambda e: (e.serviceName or "").lower(), reverse=self._sort_desc)
        for idx, e in enumerate(entries):
            if e.id is not None:
                self.store._entries[e.id] = e  # type: ignore[attr-defined]
                e.order = idx
        self.store.save()
        self.refresh_list()

    # --- mouse/keyboard events --------------------------------------------
    @on(ListView.Selected, "#vault-list")
    def handle_selected(self, event: ListView.Selected) -> None:
        item = event.item
        if not isinstance(item, VaultItem) or item.entry_id is None:
            return
        entry = self.store.get(item.entry_id)
        self._open_editor(entry)


class TrackedInput(Input):
    """Input that reports focus to the edit screen."""

    def on_focus(self, event) -> None:
        screen = getattr(self, "screen", None)
        if isinstance(screen, EditEntryScreen):
            screen.set_last_focused(self)  # type: ignore[attr-defined]

    def on_blur(self, event) -> None:
        screen = getattr(self, "screen", None)
        if isinstance(screen, EditEntryScreen):
            screen.clear_highlight()  # type: ignore[attr-defined]


class PasswordInput(TrackedInput):
    """Input that auto-unmasks while focused and notifies parent screen."""

    def on_focus(self, event) -> None:
        self.password = False
        super().on_focus(event)

    def on_blur(self, event) -> None:
        self.password = True
        super().on_blur(event)


class EditEntryScreen(Screen):
    """Subpage to edit a single vault entry."""

    BINDINGS = [
        ("escape", "cancel", "Cancel"),
    ]

    def __init__(self, entry: VaultEntry) -> None:
        super().__init__()
        self.entry = entry
        self._last_focused_id: str = "name-input"
        # placeholders for inputs
        self.name_input: Input | None = None
        self.user_input: Input | None = None
        self.pass_input: PasswordInput | None = None
        self.notes_input: Input | None = None

    def compose(self) -> ComposeResult:
        yield Header()
        copy_disabled = pyperclip is None
        self.name_input = TrackedInput(value=self.entry.serviceName, id="name-input")
        self.user_input = TrackedInput(value=self.entry.username, id="username-input")
        self.pass_input = PasswordInput(value=self.entry.password, password=True, id="password-input")
        self.notes_input = TrackedInput(value=self.entry.notes, id="notes-input")
        yield Vertical(
            Label(f"Editing entry #{self.entry.id}" if self.entry.id else "New entry"),
            Horizontal(Label("Service/Title:", id="name-label"), self.name_input),
            Horizontal(Label("Username:", id="username-label"), self.user_input),
            Horizontal(Label("Password:", id="password-label"), self.pass_input),
            Horizontal(Label("Notes:", id="notes-label"), self.notes_input),
            Horizontal(
                Button("Save", id="save", variant="success"),
                Button("Generate Password", id="genpass", variant="primary"),
                Button("Copy Field", id="copyfield", disabled=copy_disabled),
                Button("Cancel", id="cancel"),
            ),
            id="edit-layout",
        )
        yield Footer()

    def on_mount(self) -> None:
        if self.name_input:
            self.name_input.focus()
            self._set_copy_target(self.name_input.id)

    def _collect_entry(self) -> VaultEntry:
        name = self.name_input.value if self.name_input else ""
        username = self.user_input.value if self.user_input else ""
        password = self.pass_input.value if self.pass_input else ""
        notes = self.notes_input.value if self.notes_input else ""
        return VaultEntry(
            id=self.entry.id,
            serviceName=name,
            username=username,
            password=password,
            notes=notes,
            order=self.entry.order,
        )

    @on(Button.Pressed, "#save")
    def save(self) -> None:
        updated = self._collect_entry()
        self.dismiss(updated)

    @on(Button.Pressed, "#genpass")
    def generate_password_action(self) -> None:
        if self.pass_input:
            self.pass_input.value = generate_password()
            self.pass_input.password = False  # show once generated
            self.pass_input.focus()
            self._set_copy_target(self.pass_input.id)

    @on(Button.Pressed, "#copyfield")
    def copy_field(self) -> None:
        if pyperclip is None:
            return
        target_id = self._last_focused_id or "name-input"
        try:
            value = self.query_one(f"#{target_id}", Input).value
            pyperclip.copy(value)
        except Exception:
            return

    @on(Button.Pressed, "#cancel")
    def cancel_button(self) -> None:
        self.dismiss(None)

    def action_cancel(self) -> None:
        self.dismiss(None)

    def set_last_focused(self, inp: Input) -> None:
        self._last_focused_id = inp.id or self._last_focused_id
        self._set_copy_target(inp.id)

    def clear_highlight(self) -> None:
        self._set_copy_target(None)

    def _set_copy_target(self, input_id: Optional[str]) -> None:
        for field_id in ["name-input", "username-input", "password-input", "notes-input"]:
            try:
                self.query_one(f"#{field_id}", Input).remove_class("copy-target")
            except Exception:
                continue
        if input_id:
            try:
                self.query_one(f"#{input_id}", Input).add_class("copy-target")
            except Exception:
                pass


class ConfirmModal(ModalScreen[bool]):
    def __init__(self, prompt: str):
        super().__init__()
        self.prompt = prompt

    def compose(self) -> ComposeResult:
        yield Static(self.prompt)
        with Horizontal():
            yield Button("Yes", id="yes", variant="error")
            yield Button("No", id="no")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        self.dismiss(event.button.id == "yes")

    def on_key(self, event: Key) -> None:
        if event.key == "escape":
            self.dismiss(False)


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------


class PasswordManagerApp(App):
    """App wiring: holds the VaultStore and starts on the list screen."""

    CSS = """
    #vault-list { height: 1fr; }
    #edit-layout { padding: 1; }
    #edit-layout > * { margin-bottom: 1; }
    #edit-layout > *:last-child { margin-bottom: 0; }
    .copy-target { border: solid #6c8099; }
    """

    def __init__(self, vault_path: Path, password: str) -> None:
        super().__init__()
        self.store = VaultStore(vault_path, password)

    def on_mount(self) -> None:
        self.push_screen(VaultListScreen())

    def exit(self, result: int | None = None, return_code: int = 0, message: str | None = None) -> None:
        # best-effort wipe of secrets in memory
        if hasattr(self, "store") and isinstance(self.store, VaultStore):
            self.store.password = ""
            for e in self.store._entries.values():
                e.password = ""
        super().exit(result=result, return_code=return_code, message=message)


def main() -> int:
    parser = argparse.ArgumentParser(description="Vault TUI editor (Textual).")
    parser.add_argument("vault", type=Path, help="Path to .vault file")
    parser.add_argument("-p", "--password", help="Vault password (will prompt if omitted)")
    args = parser.parse_args()

    password = args.password or getpass("Vault password: ")
    if not password:
        print("Password is required.")
        return 1

    try:
        app = PasswordManagerApp(args.vault, password)
        app.run()
    except Exception as exc:  # noqa: BLE001
        print(f"Error: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

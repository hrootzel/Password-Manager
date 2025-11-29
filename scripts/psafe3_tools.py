"""
Utilities for reading/writing Password Safe v3 (.psafe3) files using pypwsafev3.
"""

import json
import os
from typing import Iterable, List, Mapping, Sequence

from pypwsafev3 import PWSafe3, Record
from pypwsafev3.errors import PasswordError


def load_psafe_entries(path: str, password: str) -> List[dict]:
    """Open a .psafe3 file and return entries as JSON-ready dicts."""
    safe = PWSafe3(path, password, mode="RO")
    return [record_to_dict(record) for record in safe.getEntries()]


def _normalize_group(value) -> Sequence[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [part for part in value.split(".") if part]
    if isinstance(value, (list, tuple)):
        return [str(v) for v in value if str(v)]
    return []


def record_from_dict(entry: Mapping) -> Record:
    """Build a Record from a mapping with keys like Title/Username/Password/Notes/Group/URL."""
    rec = Record()

    def _set(setter, key: str):
        if key in entry and entry[key] is not None:
            setter(entry[key])

    group = entry.get("Group") or entry.get("group")
    if group:
        rec.setGroup(_normalize_group(group))

    _set(rec.setTitle, "Title")
    _set(rec.setUsername, "Username")
    _set(rec.setPassword, "Password")
    _set(rec.setNote, "Notes")
    _set(rec.setURL, "URL")
    return rec


def record_to_dict(record: Record) -> dict:
    """Serialize a Record without triggering the Python 3 incompatible sort in getHistory."""
    try:
        history = record.getHistory()
        try:
            history = sorted(history, key=lambda h: h.get("saved"))
        except Exception:
            pass
    except Exception:
        history = []

    return {
        "UUID": str(record.getUUID()),
        "Group": ".".join(record.getGroup() or []),
        "Title": record.getTitle(),
        "Username": record.getUsername(),
        "Notes": record.getNote(),
        "Password": record.getPassword(),
        "Creation Time": str(record.getCreated()),
        "Password Last Modified": str(record.getPasswordModified()),
        "Expires": str(record.getExpires()),
        "Entry Last Modified": str(record.getEntryModified()),
        "URL": record.getURL(),
        "Autotype": record.getAutoType(),
        "History": history,
        "Password Policy": record.getPwdPolicy(),
        "Password Expiry Interval (Days)": record.getPasswordExpiryInterval(),
        "Double Click Action": record.getDoubleClickAction(),
        "Email Address": record.getEmail(),
        "Protected Entry": record.getProtectedEntry(),
        "Password Symbols": record.getSymbolsForPassword(),
        "Shift Double Click Action": record.getShiftDoubleClickAction(),
        "Password Policy Name": record.getPasswordPolicyName(),
    }


def write_psafe(entries: Iterable[Mapping], password: str, output_path: str, overwrite: bool = False) -> None:
    """Create/overwrite a .psafe3 vault with the provided entries."""
    if os.path.exists(output_path):
        if not overwrite:
            raise FileExistsError(f"Output file already exists: {output_path}")
        os.remove(output_path)

    safe = PWSafe3(output_path, password, mode="RW")
    safe.records = []
    for entry in entries:
        safe.records.append(record_from_dict(entry))
    safe.save()


__all__ = [
    "load_psafe_entries",
    "write_psafe",
    "record_from_dict",
    "record_to_dict",
    "PasswordError",
]

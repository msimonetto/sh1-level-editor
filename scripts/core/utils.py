def _record_to_dict(rec, fields) -> dict:
    """Convert a manifest Record to an ordered dict preserving field order."""
    d = {}
    for name, status, note in fields:
        val = getattr(rec, name)
        if isinstance(val, bytes):
            d[name] = val.hex()
        else:
            d[name] = val
        # Attach status inline as a sibling key so the JSON is self-documenting
        d[f"_status_{name}"] = status
    return d



def _hex_blob(buf: bytes, start: int, end: int) -> str:
    """Return hex string for buf[start:end] — an opaque preserved region."""
    return buf[start:end].hex()



def _merge_ranges(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Sort and merge overlapping/adjacent (start, end) ranges."""
    if not ranges:
        return []
    result = []
    for start, end in sorted(ranges):
        if result and start <= result[-1][1]:
            result[-1] = (result[-1][0], max(result[-1][1], end))
        else:
            result.append([start, end])
    return [(s, e) for s, e in result]



def _gaps_from_merged(file_size: int, merged: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Compute uncovered (gap) ranges given pre-merged covered ranges."""
    gaps = []
    cursor = 0
    for start, end in merged:
        if start > cursor:
            gaps.append((cursor, start))
        cursor = max(cursor, end)
    if cursor < file_size:
        gaps.append((cursor, file_size))
    return gaps


# ---------------------------------------------------------------------------
# PLM section serialiser
# ---------------------------------------------------------------------------

def _dict_to_record(cls, d: dict):
    """Reconstruct a manifest Record from a dict produced by _record_to_dict().

    _record_to_dict() stores bytes fields as hex strings and attaches
    `_status_<name>` siblings for every field.  This function extracts only
    the real field values (by name) and converts hex strings back to bytes.
    """
    field_names = cls._field_order
    args = []
    for name in field_names:
        val = d[name]
        if isinstance(val, str):
            val = bytes.fromhex(val)
        args.append(val)
    return cls(*args)



def _write_at(buf: bytearray, offset: int, data: bytes) -> None:
    """Write `data` into `buf` at `offset`. Raises if out of bounds."""
    end = offset + len(data)
    if end > len(buf):
        raise ValueError(
            f"_write_at: write of {len(data)} bytes at offset 0x{offset:X} "
            f"would exceed buffer of {len(buf)} bytes (end=0x{end:X})"
        )
    buf[offset:end] = data


# ---------------------------------------------------------------------------
# PLM section encoder (offset-faithful)
# ---------------------------------------------------------------------------


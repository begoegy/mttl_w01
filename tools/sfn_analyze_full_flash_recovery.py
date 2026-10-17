#!/usr/bin/env python3
"""READ-ONLY analyzer for a 1 MiB MTTL-W01 / RTL8711AF SPI flash dump.

Never opens a programmer and never writes/erases flash. It only inspects an
already-read dump and compares the two known Image2 slots with optional FWRs.
"""
from __future__ import annotations
import argparse, hashlib, json, re, struct
from pathlib import Path
from tools.sfn_repack_universal_fwr import decode_fwr

FLASH_SIZE = 0x100000
FIRST = 0x0B000
SECOND = 0x80000
PROTECTED_START = 0xEE000
LOAD = 0x10006000
NEW_SIG = b"81958711"
OLD_SIG = b"01958711"
EMPTY_SIG = b"\xff" * 8


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from('<I', data, off)[0]


def sig_kind(sig: bytes) -> str:
    if sig == NEW_SIG: return 'new'
    if sig == OLD_SIG: return 'old'
    if sig == EMPTY_SIG: return 'empty'
    return 'invalid'


def normalize_installed_image(image: bytes) -> bytes:
    out = bytearray(image)
    if len(out) >= 16:
        out[8:16] = EMPTY_SIG
    return bytes(out)


def versions(data: bytes):
    found = {m.group().decode() for m in re.finditer(rb'(?<![0-9.])[0-9]{1,2}\.[0-9]{1,3}\.[0-9]{1,3}(?![0-9.])', data)}
    return sorted(found)[:16]


def known_images(specs: list[str]):
    out = []
    for spec in specs:
        if '=' not in spec:
            raise SystemExit(f'bad --known-fwr {spec!r}; expected LABEL=/path/file.fwr')
        label, raw_path = spec.split('=', 1)
        p = Path(raw_path)
        raw = p.read_bytes()
        decoded = bytes(decode_fwr(raw))
        out.append({
            'label': label,
            'path': str(p),
            'fwr_sha256': sha(raw),
            'decoded_sha256': sha(decoded),
            'decoded': decoded,
        })
    return out


def inspect_slot(dump: bytes, name: str, base: int, limit: int, known):
    cap = limit - base
    prefix = dump[base:base+20]
    body = u32(prefix, 0)
    load = u32(prefix, 4)
    sig = bytes(prefix[8:16])
    entry = u32(prefix, 16)
    total = body + 20 if body <= 0xffffffff - 20 else 0
    geometry_ok = 44 <= total <= cap and load == LOAD and bool(entry & 1) and LOAD + 24 <= (entry & ~1) < LOAD + body
    result = {
        'name': name, 'base': f'0x{base:05x}', 'limit': f'0x{limit:05x}', 'capacity': cap,
        'body_size': body, 'file_size': total, 'load': f'0x{load:08x}', 'entry': f'0x{entry:08x}',
        'signature_ascii': sig.decode('ascii', 'replace'), 'signature_hex': sig.hex(), 'signature_kind': sig_kind(sig),
        'geometry_ok': geometry_ok, 'checksum_ok_after_signature_normalization': False,
        'normalized_sha256': None, 'versions': [], 'known_matches': [],
    }
    if not geometry_ok:
        return result
    installed = bytes(dump[base:base+total])
    normalized = normalize_installed_image(installed)
    stored = u32(normalized, len(normalized)-4)
    calc = sum(normalized[:-4]) & 0xffffffff
    result['checksum_ok_after_signature_normalization'] = stored == calc
    result['normalized_sha256'] = sha(normalized)
    result['versions'] = versions(normalized[16:-4])
    for k in known:
        if normalized == k['decoded']:
            result['known_matches'].append({
                'label': k['label'], 'fwr_sha256': k['fwr_sha256'], 'decoded_sha256': k['decoded_sha256']
            })
    return result


def analyze(path: Path, known_specs: list[str]):
    dump = path.read_bytes()
    if len(dump) != FLASH_SIZE:
        raise SystemExit(f'REFUSE: dump size={len(dump)}; exact {FLASH_SIZE} bytes required')
    known = known_images(known_specs)
    first_raw = u32(dump, 0x18)
    second_raw = u32(dump, 0x9000)
    boot_first = (first_raw & 0xffff) * 1024
    boot_geometry_ok = boot_first == FIRST and second_raw == SECOND
    slots = [
        inspect_slot(dump, 'first', FIRST, SECOND, known),
        inspect_slot(dump, 'second', SECOND, PROTECTED_START, known),
    ]
    return {
        'mode': 'READ_ONLY_NO_WRITE_NO_ERASE',
        'dump_path': str(path), 'dump_size': len(dump), 'dump_sha256': sha(dump),
        'boot_words': {'0x18': f'0x{first_raw:08x}', '0x9000': f'0x{second_raw:08x}'},
        'derived_first_slot': f'0x{boot_first:05x}', 'derived_second_slot': f'0x{second_raw:05x}',
        'boot_geometry_ok': boot_geometry_ok,
        'slots': slots,
        'write_recommendation': 'NONE. Analyze first; never write from this tool.',
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dump', type=Path)
    ap.add_argument('--known-fwr', action='append', default=[])
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    r = analyze(a.dump, a.known_fwr)
    if a.json:
        print(json.dumps(r, indent=2))
        return
    print(f"READ_ONLY dump={r['dump_path']} size={r['dump_size']} sha256={r['dump_sha256']}")
    print(f"boot_geometry_ok={r['boot_geometry_ok']} words={r['boot_words']} first={r['derived_first_slot']} second={r['derived_second_slot']}")
    for s in r['slots']:
        print(f"{s['name']} base={s['base']} sig={s['signature_ascii']!r}/{s['signature_kind']} geometry={s['geometry_ok']} checksum={s['checksum_ok_after_signature_normalization']} size={s['file_size']} versions={','.join(s['versions']) or '-'}")
        print(f"  normalized_sha256={s['normalized_sha256'] or '-'}")
        print(f"  matches={','.join(x['label'] for x in s['known_matches']) or '-'}")
    print('NO_WRITE NO_ERASE NO_PROGRAMMER_IO')

if __name__ == '__main__':
    main()

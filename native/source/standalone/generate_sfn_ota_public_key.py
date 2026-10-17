#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ed25519

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--public-key',required=True)
    ap.add_argument('--out',required=True)
    args=ap.parse_args()
    key=serialization.load_pem_public_key(Path(args.public_key).read_bytes())
    if not isinstance(key,ed25519.Ed25519PublicKey):
        raise SystemExit('SFN OTA public key must be Ed25519')
    raw=key.public_bytes(serialization.Encoding.Raw,serialization.PublicFormat.Raw)
    if len(raw)!=32:
        raise SystemExit('unexpected Ed25519 public key length')
    fp=hashlib.sha256(raw).digest()
    out=(
        '#ifndef SFN_OTA_PUBLIC_KEY_H\n#define SFN_OTA_PUBLIC_KEY_H\n#include <stdint.h>\n'
        '#define SFN_OTA_PUBLIC_KEY_BYTES 32u\n'
        'static const uint8_t sfn_ota_public_key[SFN_OTA_PUBLIC_KEY_BYTES]={'+','.join(str(x) for x in raw)+'};\n'
        'static const uint8_t sfn_ota_public_key_fingerprint[32]={'+','.join(str(x) for x in fp)+'};\n'
        '#endif\n'
    )
    Path(args.out).write_text(out,encoding='ascii')
    print('SFN_OTA_PUBLIC_KEY_SHA256='+hashlib.sha256(raw).hexdigest())

if __name__=='__main__': main()

"""Image2 inspection only. Never writes flash or builds deployable firmware."""
from dataclasses import dataclass, field
import hashlib
import re
import struct

# Conservative prototype policy: initialize within the SDK's known BD_RAM;
# reserve settings/factory sectors F0000..FFFFF. These are not a device probe.
LOAD = 0x10006000
RAM_SPAN = 0x10070000-LOAD
FILE_CAP = 0xF0000-0x80000
TABLE_TAIL = bytes.fromhex('52544b57696e00ff000101009581010100000000')
XOR_BASE = bytes.fromhex('ca5923e7d79a84e843b3afe794b1afa7')

def fwr_key(header):
    """Native FUN_10028262 + FUN_10028234: header XOR seed, then +1."""
    if len(header)!=16:raise ImageError('FWR header length')
    return bytes(((seed ^ value)+1)&255 for seed,value in zip(XOR_BASE,header))

def transform_fwr(data):
    data=bytes(data)
    if len(data)<16:raise ImageError('FWR header length')
    key=fwr_key(data[:16])
    return data[:16]+bytes(value^key[index%16] for index,value in enumerate(data[16:],16))


class ImageError(ValueError):
    pass


@dataclass(frozen=True)
class Image:
    format: str
    size: int
    body_size: int
    entry: int
    checksum: int
    sha256: str
    version_candidates: tuple
    decoded: bytes = field(repr=False)

    def metadata(self):
        return {key:getattr(self,key) for key in ('format','size','body_size','entry',
                'checksum','sha256','version_candidates')}


def _inspect(decoded):
    size,load = struct.unpack_from('<II',decoded)
    if load != LOAD or size < 24 or size > RAM_SPAN:
        raise ImageError('Image2 load address/body size')
    if len(decoded) != size+20:
        raise ImageError('file length or extra images')
    if decoded[8:16] != b'\xff'*8:
        raise ImageError('expected uninstalled OTA header')
    entry = struct.unpack_from('<I',decoded,16)[0]
    if not entry & 1 or not LOAD+24 <= (entry & ~1) < LOAD+size:
        raise ImageError('entry outside image or not Thumb')
    if decoded[20:40] != TABLE_TAIL:
        raise ImageError('unsupported start table')
    checksum = struct.unpack_from('<I',decoded,len(decoded)-4)[0]
    if sum(decoded[:-4]) & 0xffffffff != checksum:
        raise ImageError('checksum mismatch')
    versions = tuple(sorted({m.group().decode() for m in re.finditer(
        rb'(?<![0-9.])[0-9]{1,2}\.[0-9]{1,3}\.[0-9]{1,3}(?![0-9.])',decoded[16:-4])}))[:8]
    return size,entry,checksum,versions


def inspect_image(data, expected_sha256=None):
    data = bytes(data)
    if not 44 <= len(data) <= FILE_CAP:
        raise ImageError('file outside prototype size budget')
    digest = hashlib.sha256(data).hexdigest()
    if expected_sha256 is not None:
        if not re.fullmatch('[0-9a-fA-F]{64}',expected_sha256):
            raise ImageError('expected SHA-256 syntax')
        if digest != expected_sha256.lower():
            raise ImageError('SHA-256 mismatch')
    # First 16 bytes are not XOR encoded in the manufacturer's OTA package.
    encoded = transform_fwr(data)
    for kind,decoded in [('Image2 BIN',data),('LG U+ FWR (XOR)',encoded)]:
        try:
            body,entry,checksum,versions = _inspect(decoded)
        except ImageError:
            continue
        return Image(kind,len(data),body,entry,checksum,digest,versions,decoded)
    raise ImageError('unsupported or corrupt Image2 package')


class UploadSimulation:
    """Incremental staging in RAM. Commit means simulation state, not boot marking."""
    def __init__(self, image, now, timeout_ms=30000):
        if timeout_ms <= 0:
            raise ValueError('timeout')
        self.image = image
        self.staging = bytearray()
        self.deadline = now+timeout_ms
        self.state = 'receiving'

    def write(self, chunk, now):
        if self.state != 'receiving':
            raise ImageError('not receiving')
        if now >= self.deadline or not 1 <= len(chunk) <= 1024 or len(chunk) > self.image.size-len(self.staging):
            self.state = 'failed'
            raise ImageError('upload budget/deadline')
        self.staging.extend(chunk)

    def finish(self, now):
        if self.state != 'receiving' or now >= self.deadline or bytes(self.staging) != self.image.decoded:
            self.state = 'failed'
            raise ImageError('incomplete/corrupt staging')
        _inspect(bytes(self.staging))
        self.state = 'verified'

    def commit(self):
        if self.state != 'verified':
            raise ImageError('cannot commit unverified upload')
        if bytes(self.staging) != self.image.decoded:
            self.state = 'failed'
            raise ImageError('staging changed after verification')
        self.state = 'simulation_complete'

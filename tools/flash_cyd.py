#!/usr/bin/env python3
"""Install a packaged CYD build after checking its MCU, with backup by default.

No --force option: a standard ESP32 CYD image must never be written to an S3.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime


def run(command, capture=False):
    result = subprocess.run(command, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    if capture:
        print(result.stdout, end="", flush=True)
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='USB serial port; automatically select only when exactly one exists')
    parser.add_argument('--variant', choices=['cyd', 'cyd-st7789'], default='cyd')
    parser.add_argument('--esptool', help='Path to esptool.py (otherwise use the installed esptool module)')
    parser.add_argument('--initialize-cache', action='store_true',
                        help='Write the packaged empty cache filesystem after backing up original Flash')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--skip-backup', action='store_true',
                        help='Skip the original Flash backup only when the owner explicitly requested it')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    package = root / 'firmware' / args.variant
    manifest = json.loads((package / 'manifest.json').read_text())
    assert manifest['chip'] == 'esp32'
    for name, digest in manifest['sha256'].items():
        if hashlib.sha256((package / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError('Firmware checksum mismatch: ' + name)
    port = args.port
    if not port:
        from serial.tools import list_ports
        ports = [p.device for p in list_ports.comports() if p.vid is not None]
        if len(ports) != 1:
            raise RuntimeError('Expected exactly one USB serial port; found: ' + repr(ports))
        port = ports[0]
    tool = [sys.executable, args.esptool] if args.esptool else [sys.executable, '-m', 'esptool']
    base = tool + ['--port', port, '--baud', str(args.baud)]
    identity = run(base + ['--after', 'no_reset', 'flash_id'], capture=True)
    match = re.search(r'Chip is (ESP32[^\r\n]*)', identity)
    chip = match.group(1) if match else ''
    if not (chip == 'ESP32' or chip.startswith('ESP32-D') or chip.startswith('ESP32-PICO')):
        raise RuntimeError('Not a standard ESP32 CYD target: ' + (chip or 'unidentified MCU'))
    if 'Detected flash size: 4MB' not in identity:
        raise RuntimeError('Flash is not the expected CYD 4 MB; review the actual board before installation')
    if args.skip_backup:
        print('Skipping original Flash backup at the owner\'s explicit request.', flush=True)
    else:
        backup_dir = root / '.flash-backups'
        backup_dir.mkdir(mode=0o700, exist_ok=True)
        stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
        backup = backup_dir / (stamp + '-before-cyd.bin')
        run(base + ['--chip', 'esp32', '--after', 'no_reset', 'read_flash', '0x0', '0x400000', str(backup)])
        os.chmod(backup, 0o600)
        if backup.stat().st_size != 0x400000:
            raise RuntimeError('Incomplete backup; not writing firmware')
        print('Original Flash backup:', backup, flush=True)
    pairs = []
    for name, offset in manifest['offsets'].items():
        if name == 'littlefs.bin' and not args.initialize_cache:
            continue
        pairs += [offset, str(package / name)]
    run(base + ['--chip', 'esp32', 'write_flash', '--flash_mode', 'dio',
                '--flash_freq', '40m', '--flash_size', '4MB'] + pairs)
    print('CYD firmware written and verified by esptool. Confirm LCD, touch and Wi-Fi on hardware.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, AssertionError, OSError, subprocess.CalledProcessError) as error:
        print('Not flashed:', error, file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
"""Prepare full-install images from the checked release parts; never read a device."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "docs/web-flash"
PROFILES = {
    "cyd-st7789": ("ESP32-2432S028 / ST7789", True),
    "cyd": ("ESP32-2432S028R / ILI9341", False),
}

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--esptool", help="Path to the pinned esptool.py 4.5.1")
    args = parser.parse_args()
    (WEB / "firmware").mkdir(parents=True, exist_ok=True)
    (WEB / "manifests").mkdir(exist_ok=True)
    tool = [sys.executable, args.esptool] if args.esptool else [sys.executable, "-m", "esptool"]
    assert "v4.5.1" in subprocess.check_output(tool + ["version"], text=True), "Use esptool 4.5.1"
    catalog = {"version": "0.3.3", "esp_web_tools": "10.5.2", "profiles": {}}
    for variant, (label, accepted) in PROFILES.items():
        package = ROOT / "firmware" / variant
        source = json.loads((package / "manifest.json").read_text())
        assert source["version"] == catalog["version"] and source["chip"] == "esp32"
        assert source["flash_bytes"] == 0x400000
        for name, checksum in source["sha256"].items():
            assert digest((package / name).read_bytes()) == checksum, (variant, name)
        filename = f"tra-board-0.3.3-{variant}-full.bin"
        target = WEB / "firmware" / filename
        pairs = [part for name, offset in source["offsets"].items() for part in (offset, str(package / name))]
        subprocess.run(tool + ["--chip", "esp32", "merge_bin", "--output", str(target),
                       "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
                       "--target-offset", "0"] + pairs, check=True)
        merged = target.read_bytes()
        assert len(merged) == 0x3F0000
        assert merged[0x1000] == 0xE9 and merged[0x1002:0x1004] == bytes([2, 0x20])
        assert merged[0x9000:0x10000] == bytes([255]) * 0x7000, "No personal NVS in public images"
        for name in ("partitions.bin", "tra-board-app.bin", "littlefs.bin"):
            offset = int(source["offsets"][name], 0)
            payload = (package / name).read_bytes()
            assert merged[offset:offset + len(payload)] == payload, name
        manifest = {
            "name": f"TRA Station Board - {label}",
            "version": catalog["version"],
            "new_install_prompt_erase": False,
            "new_install_improv_wait_time": 0,
            "builds": [{"chipFamily": "ESP32", "parts": [{"path": "../firmware/" + filename, "offset": 0}]}],
        }
        (WEB / "manifests" / f"{variant}.json").write_text(json.dumps(manifest, indent=2) + "\n")
        catalog["profiles"][variant] = {
            "label": label, "hardware_verified": accepted,
            "manifest": f"manifests/{variant}.json", "firmware": f"firmware/{filename}",
            "sha256": digest(merged), "size": len(merged),
            "source_app_sha256": source["sha256"]["tra-board-app.bin"],
            "flash_mode": "dio", "flash_frequency": "40m", "flash_bytes": 0x400000,
        }
    (WEB / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    print("Verified web images and manifests for both ESP32 variants.")

if __name__ == "__main__":
    main()

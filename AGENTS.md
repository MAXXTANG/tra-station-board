# TRA Station Board

- Hardware profiles: LILYGO T-Display-S3 Touch 1.9 inch (16 MB Flash / 8 MB OPI PSRAM), CYD ESP32-2432S028R (4 MB, no PSRAM; ILI9341 or ST7789 variant). Never apply a profile to a USB device whose MCU does not match. An ESP32-S3 with 8 MB Flash is not identified as a standard CYD.
- Station-name taps open the on-device recent/region/station picker; keep Wi-Fi setup on the long-press GPIO14 action. Preserve saved Wi-Fi and orientation when selecting another station.
- Keep the landscape layout: 320x170 / 3 rows on LILYGO, 320x240 / 5 rows on CYD. Table at top, clock at bottom left, station and direction controls on the bottom row. CYD uses a 15 KiB render band and calibrated XPT2046 coordinates. Update layout and previews together.
- `platformio.ini` pins the toolchain and dependencies. Build with `pio run`; run `test/test_model.cpp` for data model changes.
- Credentials are configured on the device. Never commit Wi-Fi/API credentials, original Flash backups, device screenshots with setup passwords, or build caches.
- Current release uses only free TRA ODS timetables. Poll the publication catalog every 120 seconds, cache unchanged data, and never call TDX or fetch live delay pages. Keep the four columns: time, destination, train type, train number. Do not imply punctuality or delay status.
- Keep demo preview data clearly marked. Preserve the existing Wi-Fi and screen orientation when migrating settings.
- Official ODS V1.6 LineDir is 1=clockwise and 2=counterclockwise. Normalize to internal 0/1, preserving configurable north/south mapping and branch-line labels.
- Record actual build, flashing, and device checks separately from pending user acceptance in `docs/verification.md`. A framebuffer capture does not prove physical touch or successful live API retrieval.
- Preserve third-party font and board attribution. No project-wide license has been selected by the owner.

- Product screens omit the word 免費. The status row reads 更新時間 HH:MM after a successful check, without 表定, 已查 or an interval suffix.

- CYD cache uses LittleFS without automatic formatting, CRC validation, station/date validation and atomic rename. Back up original Flash before initializing a new CYD cache unless the owner explicitly declines a backup. The LILYGO filesystem remains untouched.
- Standard CYD uses GPIO14 for the LCD clock, not as a button. Use BOOT short press for paging and long press for setup on that profile.

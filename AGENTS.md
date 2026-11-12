# TRA Station Board

- Hardware: LILYGO T-Display-S3 Touch 1.9 inch, 16 MB Flash / 8 MB OPI PSRAM. Do not apply Waveshare pin definitions.
- Station-name taps open the on-device recent/region/station picker; keep Wi-Fi setup on the long-press GPIO14 action. Preserve saved Wi-Fi and orientation when selecting another station.
- Keep the 320 x 170 landscape layout: table at top, clock at bottom left, station and direction controls on the bottom row. Update touch coordinates and `tools/render_preview.py` together.
- `platformio.ini` pins the toolchain and dependencies. Build with `pio run`; run `test/test_model.cpp` for data model changes.
- Credentials are configured on the device. Never commit Wi-Fi/API credentials, original Flash backups, device screenshots with setup passwords, or build caches.
- Current release uses only free TRA ODS timetables. Poll the publication catalog every 120 seconds, cache unchanged data, and never call TDX or fetch live delay pages. Keep the four columns: time, destination, train type, train number. Do not imply punctuality or delay status.
- Keep demo preview data clearly marked. Preserve the existing Wi-Fi and screen orientation when migrating settings.
- Official ODS V1.6 LineDir is 1=clockwise and 2=counterclockwise. Normalize to internal 0/1, preserving configurable north/south mapping and branch-line labels.
- Record actual build, flashing, and device checks separately from pending user acceptance in `docs/verification.md`. A framebuffer capture does not prove physical touch or successful live API retrieval.
- Preserve third-party font and board attribution. No project-wide license has been selected by the owner.

- Product screens omit the word 免費. The status row reads 更新時間 HH:MM after a successful check, without 表定, 已查 or an interval suffix.

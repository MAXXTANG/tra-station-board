# TRA Station Board

- Hardware: LILYGO T-Display-S3 Touch 1.9 inch, 16 MB Flash / 8 MB OPI PSRAM. Do not apply Waveshare pin definitions.
- Keep the 320 x 170 landscape layout: table at top, clock at bottom left, station and direction controls on the bottom row. Update touch coordinates and `tools/render_preview.py` together.
- `platformio.ini` pins the toolchain and dependencies. Build with `pio run`; run `test/test_model.cpp` for data model changes.
- Credentials are configured on the device. Never commit Wi-Fi/TDX credentials, original Flash backups, device screenshots with setup passwords, or build caches.
- Keep demo preview data clearly marked. Missing, stale, or disconnected live data must never become an on-time status.
- TDX Direction means clockwise/counterclockwise; preserve configurable north/south mapping and branch-line labels.
- Record actual build, flashing, and device checks separately from pending user acceptance in `docs/verification.md`. A framebuffer capture does not prove physical touch or successful live API retrieval.
- Preserve third-party font and board attribution. No project-wide license has been selected by the owner.

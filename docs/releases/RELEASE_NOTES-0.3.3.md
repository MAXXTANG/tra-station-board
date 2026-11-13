# v0.3.3 — ESP32-2432S028 臺鐵時刻看板

本版主要新增「常用車站 → 螢幕亮度」，可直接觸控調整並保存背光。使用臺鐵官方 ODS 表定時刻，每 2 分鐘檢查更新，不需要 TDX 金鑰，也不提供即時誤點資訊。

## 下載

| 檔案 | 適用板型與驗證範圍 |
| --- | --- |
| [ESP32-2432S028／ST7789 韌體套件](https://github.com/MAXXTANG/tra-station-board/releases/download/v0.3.3/tra-station-board-v0.3.3-esp32-2432s028-st7789.zip) | CYD2USB 雙 USB 版；已完成本專案實機燒錄、連線、觸控及亮度保存驗收 |
| [ESP32-2432S028R／ILI9341 韌體套件](https://github.com/MAXXTANG/tra-station-board/releases/download/v0.3.3/tra-station-board-v0.3.3-esp32-2432s028r-ili9341.zip) | 原 CYD profile；已編譯及主機測試，尚未以此 LCD 實機驗收 |
| [燒錄方式](https://github.com/MAXXTANG/tra-station-board/releases/download/v0.3.3/FLASHING-0.3.3.md) | 首次安裝、保留設定更新、USB 下載模式與配網 |
| [SHA256SUMS.txt](https://github.com/MAXXTANG/tra-station-board/releases/download/v0.3.3/SHA256SUMS.txt) | 所有下載附件的 SHA-256 |

目前這片已驗證的雙 USB／ST7789 板使用第一包。兩包都是傳統 ESP32／4 MB Flash，不能用於 ESP32-S3；請核對實際 LCD IC，不能只依「2.8 吋」或 USB 孔數選檔。

ZIP 內含 bootloader、分割表、app、空白 LittleFS、manifest、安裝工具、燒錄文件及授權告知。解壓後即可操作，不需要 PlatformIO。GitHub 自動附上的 Source code 為完整原始碼快照；一般安裝請選上表的板型套件。

## 更新內容

- 常用車站清單下方新增「螢幕亮度」入口。
- 支援 10–100% 觸控滑桿及 10／25／50／75／100% 快選，立即預覽背光。
- 「儲存並返回」保存至 NVS，不重新開機；右上「返回」取消並恢復原亮度。
- 舊設定預設 100%，最低 10% 避免畫面全黑；選站及配網保留亮度、Wi-Fi、車站、方向與觸控校正。
- 支援正常與 180° 顛倒方向，2.8 吋每頁五班；新增操作測試、互動預覽及最新實機截圖。

## 已有功能

- 全台 246 站選擇、最近常用站、縣市／車站選單。
- 四欄：時間、開往、車種、車次；底部時鐘、站名與方向切換。
- 2.4 GHz Wi-Fi 配網、北上／南下或順行／逆行、每 15 秒自動輪播選項。
- 每 120 秒檢查 ODS 目錄，資料未變時沿用快取；CYD 使用 LittleFS 保存單站時刻表。
- XPT2046 四點觸控校正與 180° 方向設定保存。

## 燒錄摘要

首次安裝：解壓對應套件，進入其中的專案目錄，再執行：

```sh
python -m pip install esptool==4.5.1 pyserial
python tools/flash_cyd.py --variant cyd-st7789 --baud 115200 --initialize-cache
```

ILI9341 包將 variant 改為 cyd。安裝工具會核對映像雜湊、ESP32 與 4 MB Flash，預設備份原 Flash；首次初始化會替換分割表與快取內容。

**已安裝相同 3 MiB app 分割表的使用者，可只更新 app 保留設定與快取。** 不要在一般更新時初始化檔案系統。實際指令與分割表確認方法見附件 [FLASHING-0.3.3.md](https://github.com/MAXXTANG/tra-station-board/releases/download/v0.3.3/FLASHING-0.3.3.md)。

## 驗證與限制

- 三板型交叉編譯，以及亮度 PWM 範圍、滑桿邊界、觸控目標、正常／180° 操作流程測試通過。
- ST7789 實機已確認 0.3.3 連線、校時、正式時刻表與快取；使用者確認實體亮度變化，保存 10% 後明確重啟仍讀回 10%／PWM 26。
- 本次發行沿用上述已驗證的 0.3.3 映像，不重新編譯或刷機；附件封裝另核對 manifest 雜湊與 ZIP 完整性。
- ILI9341 為編譯驗證版；LILYGO 不包含在本次 Release 韌體附件。CYD 採 USB 更新，目前沒有 OTA。
- 資料為表定時刻，無即時準點／誤點。全台站點方向、實際跨日及長時間穩定性尚未全面驗收。

[最新實機截圖與亮度操作](https://github.com/MAXXTANG/tra-station-board/tree/v0.3.3/docs/screenshots-cyd-033) · [完整驗證紀錄](https://github.com/MAXXTANG/tra-station-board/blob/v0.3.3/docs/verification.md)

原始碼已公開，整體軟體授權尚未選定。Noto Sans TC 字型及第三方來源各依原授權；套件附有字型與 LILYGO 來源授權文件。

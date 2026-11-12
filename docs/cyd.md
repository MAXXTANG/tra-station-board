# CYD 2.8 吋開發版

0.3.0-dev，2026-11-13。已通過主機測試與交叉編譯，**尚未在 CYD 燒錄或驗收**。

| 環境 | 目標 | 顯示／觸控 |
| --- | --- | --- |
| cyd | ESP32、4 MB Flash | ILI9341、320×240、XPT2046 |
| cyd-st7789 | ESP32、4 MB Flash | ST7789、320×240、XPT2046 |
| lilygo | 原 ESP32-S3、16 MB Flash／8 MB PSRAM | 原 320×170、CST816／CST328 |

## 架構與操作

- `board_profile.h` 與 `board_setup.h` 分離板型、接腳及尺寸。CYD 每頁五班，LILYGO 每頁三班；時鐘左下、站名及方向控制最下方。
- `frame_buffer.h`：CYD 以 320×24 RGB565 小區塊繪圖，只用 15 KiB 緩衝；每次重畫先取單一資料快照，網路背景工作不操作 LCD。
- `ods_stream.h`：每次解析單一停站，保留精簡站序與時間紀錄，不配置 S3 版的 98 KB 車次 JSON 文件。完整成功才替換資料。
- `cyd_touch.h`、`touch_calibration.h`：LCD 與 XPT2046 各用一個硬體 SPI，首三點計算仿射校正，第四點驗證，校正保存 NVS，180° 觸控同步旋轉。本版不使用 microSD。
- `rail_cache.h`、`cache_store.h`：LittleFS 站別快取，CRC32、日期及車站校驗、先寫暫存檔再原子改名。掛載失敗不自動格式化；離線冷開機未校時則顯示快取日期，時鐘保持 `--:--:--`。
- 每 120 秒檢查 ODS 目錄，檔案異動才下載；不呼叫 TDX，不顯示誤點。資訊列為「更新時間 HH:MM」。
- CYD 的 BOOT 短按翻頁，長按 3 秒開配網。GPIO14 是 LCD 時鐘，不能當成原 LILYGO 的按鈕。
- 配網後重啟依序點四個十字校正；設定頁可勾選重新校正。Wi-Fi、站點、方向與校正保留於 NVS。

[五列畫面示意](../cyd-preview/board-preview.png)／[互動預覽](../cyd-preview/board-preview.html) 使用示範班次，不是實機資料。

## 建置與燒錄

```sh
pio run -e cyd
pio run -e cyd-st7789
pio run -e cyd -t buildfs
```

舊 PlatformIO 的 mklittlefs 在 Apple Silicon 無法執行時，可設定原生工具：

```sh
TRA_MKLITTLEFS=/absolute/path/to/mklittlefs pio run -e cyd -t buildfs
```

本輪以官方 mklittlefs `2.5.1-2`（`7f77f2baa96cf7505e1d1f5846537fe58e3e90f3`）編譯 arm64，驗證 LittleFS 2.0、block=4096、page=256、size=917504 映像可列出內容。

標準 CYD 的板載 CH340 提供 USB-to-UART。先用資料線直接接板載 USB 孔，確認 Mac USB 清單與序列埠。自動下載失敗時，按住 BOOT、按一下 RESET／RST，再放開 BOOT；必要時在 Connecting 時持續按住 BOOT。可先用 115200 baud 燒錄。

```sh
python tools/flash_cyd.py --variant cyd --initialize-cache
# ST7789 批次使用 --variant cyd-st7789
# 可加 --port /dev/cu.wchusbserial... 指定已核對的埠
```

工具先驗證 SHA-256、ESP32 晶片及 4 MB Flash，再完整備份原 Flash，成功後才寫入。沒有 USB 埠、晶片為 S3、容量不符或備份失敗都停止，沒有強制略過選項。備份在 `.flash-backups/`，不可上傳。

`--initialize-cache` 會清除快取分割區原內容，寫入新檔案系統。若原分割表及 LittleFS 相容而要沿用，可省略該參數。新分割表採單一 app、USB 更新，沒有 OTA。

ESP32 CYD 的套件位置：bootloader `0x1000`、partition `0x8000`、app `0x10000`、LittleFS `0x310000`。**不能套用 S3 的 bootloader `0x0` 寫法，也不能套用到未知板型。**

## 驗證範圍

- 三個目標均交叉編譯通過；原資料模型與 LILYGO 座標回歸通過。
- 精簡串流解析器通過完整 2026-11-11 官方檔案，礁溪 92 筆；跨午夜、秒數、終點排除、錯誤及截斷保留既有資料的案例通過。
- 校正計算、180° 座標、退化取樣、快取回讀、站別不符、截斷、CRC 損壞拒絕通過主機測試。
- 燒錄防護測試以模擬 esptool 回應，確認 S3 與 8 MB Flash 目標在任何 Flash 讀寫前即被拒絕。實際執行燒錄工具則因沒有 USB 序列埠而停止，沒有寫入。
- 實機 LCD、觸控校正、下載時操作、兩分鐘週期、斷電快取、跨日及長時間運作仍待驗收。
- 本輪讀到的是 ESP32-S3 revision v0.2、8 MB Flash、MAC `10:b4:1d:e7:f2:28`，與標準 CYD 不同。重啟及換線後沒有 USB 裝置，尚未讀取原分割表或刷入；先前 LILYGO 驗收不能當成 CYD 驗收。

## 來源

- [CYD Setup：USB、CH340 與速度](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/SETUP.md)
- [CYD Troubleshooting：板號與 SPI](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/TROUBLESHOOTING.md)
- [Espressif：BOOT／GPIO0 與 RESET](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html)
- [ILI9341 板型](https://github.com/rzeldent/platformio-espressif32-sunton/blob/main/esp32-2432S028R.json)／[ST7789 板型](https://github.com/rzeldent/platformio-espressif32-sunton/blob/main/esp32-2432S028Rv3.json)

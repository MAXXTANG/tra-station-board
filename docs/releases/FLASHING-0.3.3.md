# v0.3.3 ESP32-2432S028 燒錄方式

## 選擇檔案

| 下載套件 | variant | LCD | MCU／Flash |
| --- | --- | --- | --- |
| tra-station-board-v0.3.3-esp32-2432s028-st7789.zip | cyd-st7789 | ST7789，雙 USB 實機驗證版 | ESP32／4 MB |
| tra-station-board-v0.3.3-esp32-2432s028r-ili9341.zip | cyd | ILI9341，僅編譯及主機驗證 | ESP32／4 MB |

先確認 LCD IC 與商品完整型號；ESP32-S3 不能使用這兩包。解壓 ZIP，進入內含 tools、firmware 的目錄。以下指令中的 python 應為 Python 3。

## 1. 安裝工具並確認 USB

```sh
python -m pip install esptool==4.5.1 pyserial
python -m serial.tools.list_ports -v
```

本專案雙 USB 實機使用 USB-A→Micro-USB 資料線，Mac 辨識為 CH340。只亮背光不代表資料線已連通；部分 USB-C 批次的 C-to-C 相容性不佳。

序列埠例：macOS 為 /dev/cu.usbserial-xxxx，Linux 為 /dev/ttyUSB0，Windows 為 COM3。下列範例使用 macOS 名稱，請換成實際埠。

## 2A. 首次安裝／不同分割表

ST7789：

```sh
python tools/flash_cyd.py --variant cyd-st7789 --port /dev/cu.usbserial-xxxx --baud 115200 --initialize-cache
```

ILI9341：

```sh
python tools/flash_cyd.py --variant cyd --port /dev/cu.usbserial-xxxx --baud 115200 --initialize-cache
```

工具先核對套件 SHA-256、ESP32 與 4 MB Flash，預設備份完整原 Flash 至 .flash-backups/，再寫入：

| 映像 | 位址 | 用途 |
| --- | --- | --- |
| bootloader.bin | 0x1000 | 開機程式 |
| partitions.bin | 0x8000 | 本版分割配置 |
| tra-board-app.bin | 0x10000 | 看板程式，分區大小 0x300000 |
| littlefs.bin | 0x310000 | 空白時刻快取，分區大小 0xE0000 |

完整安裝會替換開機程式、分割表與快取內容。只有自行明確決定不備份時才加 --skip-backup；晶片及映像檢查仍會保留。原 Flash 備份可能含個人設定，請自行妥善保存，勿上傳公開 repo。

## 2B. 已使用相同分割表：只更新 app

此方式保留 NVS 的 Wi-Fi、車站、亮度、螢幕方向及觸控校正，也保留 LittleFS 快取；不寫 bootloader、分割表或檔案系統。

先讀取實際分割表：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-xxxx --baud 115200 read_flash 0x8000 0xC00 partition-current.bin
```

使用 Python 比對，以下以 ST7789 套件為例：

```sh
python -c "from pathlib import Path; assert Path('partition-current.bin').read_bytes() == Path('firmware/cyd-st7789/partitions.bin').read_bytes(), 'Partition mismatch: use first-install instructions'"
```

比對通過才更新 ST7789 app：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-xxxx --baud 115200 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 firmware/cyd-st7789/tra-board-app.bin
```

ILI9341 將兩個指令的 firmware/cyd-st7789 路徑改成 firmware/cyd。此直接更新方式應先確認晶片為 ESP32、Flash 為 4 MB，且 app0 位於 0x10000、容量為 0x300000。

若分割表不同，尤其舊 app 區只有 0x180000（1.5 MiB），不能直接放入本版約 1.8 MB 程式；請改用首次安裝流程，讓工具先備份並重建正確配置。

## 3. 無法進入下載模式

保持資料線連接，按住 BOOT → 按一下 RESET／RST → 放開 BOOT，重試指令。必要時在 Connecting 期間按住 BOOT。先使用 115200 baud。

## 4. 配網、選站與觸控校正

1. 連上螢幕顯示的 TRA-xxxxxx 熱點，使用螢幕上的密碼。
2. 手機開啟 http://192.168.4.1/，填入 2.4 GHz Wi-Fi，搜尋或依縣市選站。
3. 選擇北上／南下或順行／逆行、正常或 180° 螢幕方向，儲存重連。
4. 首次使用依序點擊四個十字完成觸控校正。已有有效校正資料者會沿用。

長按 CYD 的 BOOT 3 秒可再次開啟配網頁。修改既有設定時，Wi-Fi 密碼留白可沿用；更換網路名稱時需填入新密碼。

## 5. 調整亮度

點底部站名 → 常用車站 → 螢幕亮度，點滑桿或 10／25／50／75／100% 快選即可預覽。底部「儲存並返回」保存，右上「返回」取消。舊設定預設 100%，最低 10%；已實機確認保存 10% 後重啟仍保留。

## 核對附件

Release 的 SHA256SUMS.txt 列出 ZIP 與文件雜湊。macOS／Linux 可使用：

```sh
shasum -a 256 -c SHA256SUMS.txt
```

每個 ZIP 內亦附 SHA256SUMS.txt；firmware/<variant>/manifest.json 記錄各映像雜湊、位址與驗證範圍。不要將含真實 NVS、Wi-Fi 或原 Flash 備份的檔案重新封裝發布。

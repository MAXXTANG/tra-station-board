# 分板型韌體寫入

請先核對 MCU、Flash 容量、螢幕驅動及分割表。CYD 的 ESP32 映像不能寫入 ESP32-S3。各套件的 `manifest.json` 包含版本、位址及 SHA-256。

## CYD2USB／ST7789（ESP32、4 MB）

套件：[cyd-st7789/](cyd-st7789/)。本次實機使用 USB-A→Micro-USB 資料線，經 CH340 連線成功。USB-C 對 USB-C 在部分雙 USB 板上不相容；先確認電腦出現 USB 序列埠。

```sh
python -m pip install esptool==4.5.1 pyserial
python tools/flash_cyd.py --variant cyd-st7789 --baud 115200 --initialize-cache
```

以上從 repository 根目錄執行。`--initialize-cache` 是首次安裝用，會清除 LittleFS 快取區原內容。工具預設備份原 Flash；經使用者明確選擇略過時可加 `--skip-backup`。目標晶片、容量與套件雜湊檢查不會因此略過。

## 原 CYD／ILI9341（ESP32、4 MB）

套件：[cyd/](cyd/)。使用相同工具，改成 `--variant cyd`。此目標已交叉編譯，尚未在原單 USB 板上實機驗收。

CYD 套件位址：bootloader `0x1000`、partitions `0x8000`、app `0x10000`、LittleFS `0x310000`。完整寫入會替換分割表；不可套用到其他佈局。

若已確認使用同一分割表，可只更新 app，保留 NVS 的 Wi-Fi、站點與校正，以及 LittleFS 快取：

```sh
python -m esptool --chip esp32 --port /dev/cu.usbserial-xxxx --baud 115200 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 firmware/cyd-st7789/tra-board-app.bin
```

自動下載模式失敗時，按住 BOOT、按一下 RESET／RST，再放開 BOOT。若高速傳輸報雜訊或封包錯誤，使用 115200 baud。

## LILYGO T-Display-S3 Touch 1.9 吋

目標為 ESP32-S3、16 MB Flash、8 MB OPI PSRAM。`firmware/` 根目錄保留曾實機驗證的 **0.2.1** 套件；現行共用程式可用 `pio run -e lilygo` 編譯，不能將根目錄舊版版本號當成現行共用程式的驗證結果。

若已核對分割表與原套件一致、OTA 啟動欄位為 app0，可只更新原 app，保留 bootloader、分割表、NVS 及其他儲存區：

```sh
python -m esptool --chip esp32s3 --port /dev/cu.usbmodemxxxx --baud 460800 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 firmware/tra-board-app.bin
```

全新板或重建佈局時，依根目錄 [manifest.json](manifest.json) 的四個位址寫入；此操作會更動 bootloader、分割表與 OTA metadata。LILYGO 的 bootloader 位址是 `0x0`，與 CYD 的 `0x1000` 不同。

所有版本均透過板上配網頁填寫 Wi-Fi，不包含個人密碼。0.2.1 起使用 ODS，不需要 TDX 金鑰。

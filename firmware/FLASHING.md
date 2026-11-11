# 韌體寫入

對 **16 MB Flash / 8 MB OPI PSRAM 的 LILYGO T-Display-S3 Touch 1.9 吋** 使用。

若分割表與本專案一致，且 OTA 啟動欄位為 app0，可只更新 app，保留原 bootloader、分割表及其他儲存區：

```sh
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 tra-board-app.bin
```

本次實機已先檢查分割表與啟動欄位，再使用此方式更新。不要把這個 offset 套用到其他分割表。

全新板子或有意重新建立相同分割表時，可依 `manifest.json` 的 offset 寫入四個 bin；此操作會改 bootloader、分割表及 OTA metadata，應先備份既有內容。初版不格式化或寫入原有檔案系統。

0.2.0 使用免費 ODS，不需要 TDX 金鑰；Wi-Fi 透過板上配網頁輸入，韌體不包含個人密碼。

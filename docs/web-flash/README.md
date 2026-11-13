# GitHub Pages 網頁燒錄

公開網址：https://maxxtang.github.io/tra-station-board/

網站由 docs/index.html 及本目錄組成，使用 GitHub Pages 的 main／docs 靜態發布方式，docs/.nojekyll 關閉 Jekyll 處理。部署不需要外部後端、使用者帳號或 API 金鑰。

## 安裝範圍

- 僅提供 ESP32／4 MB 的完整安裝：ST7789 實機驗證 profile 及 ILI9341 編譯驗證 profile。
- 完整安裝會清除原韌體、NVS、配網、站點、亮度、校正及快取；必須先選板型並勾選清除確認，才顯示連接按鈕。本頁不提供保留設定更新或自動備份。
- 使用 ESP Web Tools 10.5.2 的能力檢查，支援 HTTPS 且能提供相容序列連線時才啟用。建議電腦版 Chrome／Edge；不以瀏覽器名稱取代實際能力檢查。
- manifest 僅包含 chipFamily=ESP32。工具會辨識 MCU，但無法自動判斷 LCD IC；板型及 4 MB 規格仍須使用者確認。
- 此韌體不支援 Improv Serial，燒錄後依螢幕熱點使用 192.168.4.1 配網，並非在公開網站輸入 Wi-Fi 密碼。

## 合併映像與完整性

使用已發布的 0.3.3 映像，經 esptool 4.5.1 merge_bin 固定 DIO／40 MHz／4 MB，從位址 0 開始燒錄。沒有重新編譯 app。

開發者重建方式：

```sh
python tools/build_web_firmware.py --esptool /path/to/esptool.py
node test/test_web_installer.mjs
```

build_web_firmware.py 先核對來源 manifest 的 SHA-256，再驗證合併檔中的 app、分割表與 LittleFS 完全一致，NVS 區為空白，輸出 catalog、兩份 manifest 及兩個完整安裝映像。

前端會先下載選定板型的映像，驗證長度與 SHA-256，再將已驗證的位元組轉成 Blob URL 交給燒錄工具。校驗失敗、未確認、非安全來源、瀏覽器不支援或燒錄對話框開啟時，連接按鈕不能啟動。切換板型會清除勾選狀態，且保留既有 Blob 直到離開頁面，避免影響已啟動的對話框。

## 驗證範圍

自動測試檢查實際映像雜湊、錯誤 MCU／路徑、損壞及截斷檔案、確認欄位、板型切換、對話框鎖定、HTTP／不支援瀏覽器、載入失敗重試與 Blob 清理。

網頁及下載檔的可用性與版面另行檢查；這些檢查不等於已透過瀏覽器完整清除並重刷實機。既有板子的 Wi-Fi、10% 亮度及校正不因本次網站開發而變更。

## 來源與授權

- [ESP Web Tools 官方文件](https://esphome.github.io/esp-web-tools/)及 [原始碼](https://github.com/esphome/esp-web-tools)。
- vendor/esp-web-tools-10.5.2 為官方 npm dist/web 預建包；SOURCE.json 記錄下載完整性與每個 runtime 檔案雜湊，附原 Apache-2.0 LICENSE。
- vendor/licenses 保留工具與其依賴的原授權文字。SOURCES.json 中的版本指授權文字取得來源，並非依賴鎖定清單；工具本體版本固定為 10.5.2，已保留預建檔內的原始著作權註解。
- 本專案整體軟體授權尚未選定；第三方來源各依其原授權。

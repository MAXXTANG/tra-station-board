# 臺鐵桌上型時刻看板

適用 **LILYGO T-Display-S3 Touch 1.9 吋（170 × 320、ESP32-S3R8）**。橫向 320 × 170 顯示，預設礁溪站，也可於配網頁選擇全台臺鐵車站。

目前版本：**0.1.2**。支援底部站名／方向切換、左下時鐘與螢幕旋轉 180°。

![介面預覽，車次為示範資料](board-preview.png)

## 使用

1. 開機後，手機連接螢幕顯示的 `TRA-xxxxxx` Wi-Fi。密碼顯示在螢幕上，每次開機重新產生。
2. 開啟 `http://192.168.4.1/`，輸入家中 **2.4 GHz Wi-Fi** 名稱及密碼，選縣市／搜尋站名並選站。
3. 在同一頁輸入交通部 **TDX Client ID、Client Secret**。到 [TDX 會員中心](https://tdx.transportdata.tw/user/dataservice/key) 取得，確認帳戶有軌道基礎服務的使用權限及額度。請在裝置設定頁填寫，無須貼到聊天。
4. 儲存後重新開機，連線、NTP 校時及取得資料。金鑰尚未填入時會明確提示，不會顯示示範車次。

Wi-Fi、車站、方向設定及金鑰會存於 ESP32 NVS。這不是加密的密鑰儲存區；原始碼與交付檔不包含你的密碼。重新修改時，密碼及金鑰欄位留白表示沿用；變更 Wi-Fi 名稱或 Client ID 時需一併提供對應密碼。

## 操作

| 操作 | 功能 |
| --- | --- |
| 點底部北上／南下 | 切換方向 |
| 點列車區域 | 往後翻頁，每頁三班 |
| 點底部站名 | 重新開啟配網與選站頁 |
| 點頁碼（例如 1/5） | 下一頁，最後一頁再點回第一頁 |
| GPIO0／BOOT 按鈕 | 切換方向 |
| GPIO14 按鈕短按 | 下一頁 |
| GPIO14 按鈕長按 3 秒 | 重新開啟設定熱點 |
| 設定頁勾選輪播 | 閒置 15 秒後自動切換兩個方向 |

頁碼 `1/5` 代表同一方向列車列表的第 1 頁／共 5 頁，每頁 3 班；第 2 頁接續顯示第 4–6 班，依此類推，並不是五種功能畫面。實際總頁數依目前可顯示班次計算。時間位於左下角，站名與方向按鈕位於最下列。

配網熱點成功連線且閒置到 10 分鐘後會關閉。無法連 Wi-Fi 達 30 秒會重新開啟設定熱點，不會清除既有設定。

## 螢幕上下顛倒

長按 **GPIO14 按鈕 3 秒**開啟設定熱點，以手機進入 `http://192.168.4.1/`，找到 **螢幕方向**：

- **正常橫向**：原本的擺放方式。
- **上下顛倒（旋轉 180°）**：換另一側接線或倒放支架時使用。

儲存並重新開機後套用，拔插電源仍會保留。畫面與觸控座標會一起旋轉，配網提示頁也會採用這個方向。原有設定升級後預設維持正常橫向；既有 Wi-Fi 密碼及 TDX 金鑰留白即可保留。這個選項獨立於「南北方向對調」，後者只調整北上／南下標示。

[離線看板預覽](board-preview.html) 可按「旋轉 180°」體驗倒放及觸控；[設定頁預覽](setup-preview.html) 可查看方向選單，兩者均不會修改實機設定。

## 看板資料

- 欄位：**時間、開往、車種、車次、備註**。時間為表定發車時間，備註另顯示晚點分鐘。
- `準點`：有 5 分鐘內的有效即時資料，明確回報零延誤／準點。
- `晚 N 分`、`取消`：來自 TDX 的 `DelayTime`、`RunningStatus`。
- `停駛`：當日時刻表的 `SuspendedFlag`。
- `待更新`：缺少動態、動態過期、斷線或來源時間不合理，避免把缺資料當成準點。
- 表定時間已過、但仍有有效晚點資訊的列車會繼續顯示。午夜會換取新日期的時刻表，並處理動態回報的昨日晚點列車。
- 更新間隔可選 2／5／10 分鐘；同一份資料供兩個方向使用，切換方向不會額外呼叫 API。時刻表每 4 小時及跨日更新。HTTP 429 等待 15 分鐘再試，其他失敗等待 5 分鐘。
- 無網路時僅保留本次開機已取得的時刻表；本版不寫入原有檔案系統。重新開機後需連網重新取得時刻。

**TDX 的方向欄位是順行 0／逆行 1，並不是通用的南北方向。** 礁溪等東部幹線預設逆行為北上，西部幹線預設順行為北上；設定頁可交換。支線建議選「順行／逆行」，不把行車方向硬稱為南北；本站目的地仍依官方資料顯示。全台站點的南北標示尚未逐站實車驗證。

畫面參考車站看板的黑底、白色欄頭與車種色塊，為民間桌面作品，非臺鐵官方設備。車種以簡稱顯示，部分長站名受欄寬限制顯示前四字。

## 公開 API 與來源

查核日期：**2026-11-10**。

| 用途 | 正式來源 |
| --- | --- |
| 離線選站清單 | [臺鐵車站基本資料集](https://data.gov.tw/dataset/33425)，本版官方下載快照含 246 站，包含主要幹線及支線；不代表所有站當天均有營運班次 |
| 當日時刻表 | `GET https://tdx.transportdata.tw/api/basic/v3/Rail/TRA/DailyStationTimetable/Today/Station/{StationID}?$format=JSON` |
| 即時動態 | `GET https://tdx.transportdata.tw/api/basic/v3/Rail/TRA/StationLiveBoard/Station/{StationID}?$format=JSON` |
| OAuth2 | TDX `client_credentials`；Token 僅留 RAM，提前更新，不存入專案檔案 |
| 正式欄位規格 | [交通部軌道 OpenAPI](https://tdx.transportdata.tw/webapi/File/Swagger/V3/5fa88b0c-120b-43f1-b188-c379ddb2593d) |
| 來源延遲說明 | [TDX 雙鐵資料注意事項](https://motc-ptx.gitbook.io/tdx-zi-liao-shi-yong-kui-hua-bao-dian/data_notice/public_transportation_data/rail_data) |
| 硬體接腳 | [LILYGO 官方 T-Display-S3](https://github.com/Xinyuan-LilyGO/T-Display-S3) |

本站名單採用臺鐵政府資料開放授權條款第 1 版資料。TDX 需要會員金鑰；訪客程式請求本次實測回傳 HTTP 401。**不把「公開 API」等同無限免費**，使用量依 [TDX 訂閱方案](https://tdx.transportdata.tw/pricing) 計算。官方 OpenAPI 目前記載訪客僅限瀏覽器且每 IP 每日最多 20 次；會員限制依方案，並記載計次 1,500 次／點或計量 150 MB／點。以帳戶當下的方案為準。

TDX 說明 LiveBoard 與站內 TIDS 看板不同源，可能約有兩分鐘延遲；搭車請以現場看板為準。

HTTPS 使用 TWCA 根憑證驗證，不使用 `setInsecure()`。本次已在 Mac 以同一組信任根實際驗證 TDX HTTPS 端點。ESP32 的 HTTPS 與正式資料須在配置 Wi-Fi／金鑰後再驗證。未來若 TDX 更換憑證鏈，需要更新信任根。

## 建置

安裝 Python 及 PlatformIO，於本專案目錄執行：

```sh
pio run
pio run -t upload --upload-port /dev/cu.usbmodem101
pio device monitor --port /dev/cu.usbmodem101 --baud 115200
```

注意：一般 `pio upload` 會按專案分割表寫入 bootloader、partition table、OTA metadata 和 app，請先備份已有韌體。此版本供貨的標準設定為 16 MB Flash／8 MB OPI PSRAM；依賴版本已固定於 `platformio.ini`，板型與 LCD 設定隨附專案，不修改全域 Arduino 函式庫。

資料邏輯測試（macOS／Linux 的 C++17 編譯器）：

```sh
c++ -std=c++17 -I include -I .pio/libdeps/lilygo/ArduinoJson/src test/test_model.cpp -o /tmp/tra-model-test
/tmp/tra-model-test
c++ -std=c++17 -I include test/test_display_orientation.cpp -o /tmp/tra-orientation-test
/tmp/tra-orientation-test
```

板上序列埠診斷：`?` 讀取站號／Wi-Fi／觸控晶片／PSRAM，`b` 切方向，`p` 開設定頁，`s` 輸出 RGB888 畫面（僅開發驗證）。

字型為 Noto Sans TC（OFL，見 `docs/FONT-LICENSE.txt`），已轉換為本機可離線建置的 16 px 點陣。`tools/generate_assets.py` 可重新取得官方站名並產生字型。觸控支援 LILYGO 官方早期 CST328（0x1A）與後期 CST816（0x15），開機自動探測；實際座標方向仍需觸控實機確認。

## 可再增加的功能

1. **列車詳情**：點一班車查看完整終點、月台、停靠站及預估到站時間。
2. **通勤模式**：儲存常用車站與目的地，只顯示可到達的班次，加上出門倒數提醒。
3. **夜間模式**：定時調暗／關螢幕，觸控喚醒，配合電池供電。
4. **服務異常公告**：使用 TDX Alert 顯示停駛、路線中斷及重大事件。
5. **收藏站快速切換**：觸控選擇常用站，減少重新配網；目前切站在設定頁完成。

本版不加入語音播報或票務功能；1.9 吋優先保留列車資訊的閱讀空間。

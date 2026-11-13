import {canInstall, prepareFirmware} from "./installer-core.mjs";

export async function mountInstaller({doc = document, win = window, fetcher = fetch,
  importTools = () => import("./vendor/esp-web-tools-10.5.2/install-button.js")} = {}) {
  const status = doc.querySelector("#firmware-status");
  const browserStatus = doc.querySelector("#browser-status");
  const accept = doc.querySelector("#accept-erase");
  const fieldset = doc.querySelector("#board-choices");
  const retry = doc.querySelector("#retry");
  const radios = [...doc.querySelectorAll('input[name="board"]')];
  const components = Object.fromEntries(radios.map(r => [r.value, doc.querySelector("#install-" + r.value)]));
  const ready = new Map(), pending = new Map(), urls = [];
  let selected = radios.find(r => r.checked).value, supported = false, busy = false, catalog;
  const secure = Boolean(win.isSecureContext);
  function update() {
    for (const [key, component] of Object.entries(components)) {
      const enabled = key === selected && canInstall({supported, secure, accepted: accept.checked, ready: ready.has(key), busy});
      component.hidden = !enabled;
      component.querySelector("button").disabled = !enabled;
    }
    doc.querySelector("#locked-install").hidden = canInstall({supported, secure, accepted: accept.checked, ready: ready.has(selected), busy});
    fieldset.disabled = busy; accept.disabled = busy; retry.disabled = busy;
  }
  async function prepare(key) {
    if (ready.has(key)) {
      if (selected === key) {status.textContent = "韌體校驗完成，確認下方事項後即可連接 USB。"; retry.hidden = true;}
      update(); return;
    }
    if (selected === key) {status.textContent = "正在下載並核對韌體，請稍候…"; retry.hidden = true;}
    try {
      if (!pending.has(key)) pending.set(key, prepareFirmware(catalog.profiles[key], catalog.version,
        new URL("./catalog.json", import.meta.url), fetcher, win.crypto));
      const {manifest, buffer} = await pending.get(key);
      if (!ready.has(key)) {
        const firmwareURL = win.URL.createObjectURL(new Blob([buffer], {type: "application/octet-stream"}));
        const verified = {...manifest, builds: [{...manifest.builds[0], parts: [{path: firmwareURL, offset: 0}]}]};
        const manifestURL = win.URL.createObjectURL(new Blob([JSON.stringify(verified)], {type: "application/json"}));
        urls.push(firmwareURL, manifestURL);
        components[key].manifest = manifestURL; ready.set(key, true);
      }
      if (selected === key) {status.textContent = "韌體校驗完成，確認下方事項後即可連接 USB。"; retry.hidden = true;}
    } catch (error) {
      pending.delete(key);
      if (selected === key) {status.textContent = error.message || "載入失敗，請重新載入。"; retry.hidden = false;}
    }
    update();
  }
  for (const radio of radios) radio.addEventListener("change", () => {
    if (!radio.checked || busy) return;
    selected = radio.value; accept.checked = false;
    doc.querySelector("#selected-board").textContent = selected === "cyd-st7789" ? "ST7789・已實機驗證" : "ILI9341・僅編譯驗證";
    update(); if (supported && secure && catalog) prepare(selected);
  });
  accept.addEventListener("change", update);
  retry.addEventListener("click", () => {pending.delete(selected); prepare(selected);});
  for (const [key, component] of Object.entries(components)) component.addEventListener("click", event => {
    if (key !== selected || !canInstall({supported, secure, accepted: accept.checked, ready: ready.has(key), busy})) {
      event.preventDefault(); event.stopImmediatePropagation();
    }
  }, true);
  const observer = new win.MutationObserver(() => {
    busy = Boolean(doc.querySelector("ewt-install-dialog"));
    update();
  });
  observer.observe(doc.body, {childList: true});
  win.addEventListener("pagehide", () => {observer.disconnect(); for (const url of urls) win.URL.revokeObjectURL(url);});
  update();
  try {
    if (!secure) throw new Error("請使用 HTTPS 網址開啟此頁，瀏覽器才允許連接 USB。");
    await importTools();
    const definition = win.customElements.get("esp-web-install-button");
    supported = Boolean(definition?.isSupported && definition?.isAllowed && win.crypto?.subtle);
    browserStatus.textContent = supported ? "瀏覽器支援 USB 燒錄" : "此瀏覽器無法燒錄，請改用電腦版 Chrome 或 Edge";
    browserStatus.dataset.state = supported ? "ok" : "unsupported";
    if (!supported) {status.textContent = "你仍可閱讀步驟或下載 Release。建議在電腦版 Chrome／Edge 開啟此網址。"; update(); return;}
    const response = await fetcher(new URL("./catalog.json", import.meta.url));
    if (!response.ok) throw new Error("無法取得韌體版本資料。");
    catalog = await response.json();
    if (catalog.version !== "0.3.3" || !catalog.profiles?.["cyd-st7789"] || !catalog.profiles?.cyd)
      throw new Error("韌體版本資料不完整。");
    await prepare(selected);
  } catch (error) {
    browserStatus.textContent = "目前無法啟用燒錄";
    browserStatus.dataset.state = "unsupported";
    status.textContent = error.message || "無法載入燒錄元件，請重新整理或下載 Release。";
    update();
  }
}

if (typeof document !== "undefined") mountInstaller();

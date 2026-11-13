import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {webcrypto} from "node:crypto";
import {canInstall, prepareFirmware, validateManifest, verifyFirmware} from "../docs/web-flash/installer-core.mjs";
import {mountInstaller} from "../docs/web-flash/app.mjs";

const base = new URL("../docs/web-flash/", import.meta.url);
const catalog = JSON.parse(await readFile(new URL("catalog.json", base)));
const httpsCatalog = "https://example.test/tra-station-board/web-flash/catalog.json";
for (const [key, profile] of Object.entries(catalog.profiles)) {
  const manifest = JSON.parse(await readFile(new URL(profile.manifest, base)));
  const data = new Uint8Array(await readFile(new URL(profile.firmware, base))).buffer;
  await verifyFirmware(data, profile, webcrypto);
  assert.equal(validateManifest(manifest, profile, catalog.version, new URL(profile.manifest, httpsCatalog), httpsCatalog).pathname,
    new URL(profile.firmware, httpsCatalog).pathname);
  const wrongChip = structuredClone(manifest); wrongChip.builds[0].chipFamily = "ESP32-S3";
  assert.throws(() => validateManifest(wrongChip, profile, catalog.version, new URL(profile.manifest, httpsCatalog), httpsCatalog));
  const noErase = structuredClone(manifest); noErase.new_install_prompt_erase = true;
  assert.throws(() => validateManifest(noErase, profile, catalog.version, new URL(profile.manifest, httpsCatalog), httpsCatalog));
  const wrongPath = structuredClone(manifest); wrongPath.builds[0].parts[0].path = "https://untrusted.test/firmware.bin";
  assert.throws(() => validateManifest(wrongPath, profile, catalog.version, new URL(profile.manifest, httpsCatalog), httpsCatalog));
  const corrupt = new Uint8Array(data.slice(0)); corrupt[65536] ^= 1;
  await assert.rejects(() => verifyFirmware(corrupt.buffer, profile, webcrypto), /校驗/);
  await assert.rejects(() => verifyFirmware(data.slice(0, -1), profile, webcrypto), /長度/);
  console.log("PASS: " + key + " image SHA, manifest paths, wrong MCU and corrupt/truncated firmware");
}
const good = {supported:true, secure:true, accepted:true, ready:true, busy:false};
assert.equal(canInstall(good),true);
for (const key of ["supported","secure","accepted","ready"]) assert.equal(canInstall({...good,[key]:false}),false);
assert.equal(canInstall({...good,busy:true}),false);

class Element {
  constructor(value){this.value=value;this.hidden=true;this.disabled=false;this.checked=false;this.dataset={};this.handlers={};this.button={disabled:true};}
  addEventListener(type, handler){(this.handlers[type] ||= []).push(handler);}
  querySelector(){return this.button;}
  fire(type,event={}){for(const h of this.handlers[type]||[])h(event);}
}
function fixture({supported=true,secure=true,corrupt=false}={}) {
  const ids=Object.fromEntries(["firmware-status","browser-status","accept-erase","board-choices","retry","selected-board","locked-install",
    "install-cyd-st7789","install-cyd"].map(x=>[x,new Element()]));
  const radios=[new Element("cyd-st7789"),new Element("cyd")];radios[0].checked=true;
  let dialog=false, mutation;const urls=new Map(), callbacks={};
  const doc={body:{},querySelector:s=>s==="ewt-install-dialog"?(dialog?{}:null):ids[s.slice(1)],querySelectorAll:()=>radios};
  const win={isSecureContext:secure,crypto:webcrypto,customElements:{get:()=>({isSupported:supported,isAllowed:secure})},
    URL:{createObjectURL:blob=>{const u="blob:test/"+urls.size;urls.set(u,blob);return u;},revokeObjectURL:u=>urls.delete(u)},
    MutationObserver:class{constructor(cb){mutation=cb;}observe(){}disconnect(){}},
    addEventListener:(key,fn)=>{callbacks[key]=fn;}};
  let damage=corrupt, fetches=0;
  async function fetcher(url) {
    fetches++;let bytes=new Uint8Array(await readFile(url));
    if(damage&&String(url).endsWith(".bin")){bytes=bytes.slice();bytes[100000]^=1;}
    return {ok:true,json:async()=>JSON.parse(new TextDecoder().decode(bytes)),arrayBuffer:async()=>bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength)};
  }
  return {doc,win,ids,radios,urls,fetcher,callbacks,get fetches(){return fetches;},
    setDamage:v=>{damage=v;},setDialog:v=>{dialog=v;mutation();}};
}
const tick=()=>new Promise(resolve=>setImmediate(resolve));
async function until(predicate){for(let n=0;n<1000;n++){if(predicate())return;await tick();}assert.fail("UI did not settle");}
const page=fixture();await mountInstaller({...page,importTools:async()=>{}});
assert.equal(page.ids["install-cyd-st7789"].hidden,true,"No install without explicit consent");
page.ids["accept-erase"].checked=true;page.ids["accept-erase"].fire("change");
assert.equal(page.ids["install-cyd-st7789"].hidden,false);
const firstURL=page.ids["install-cyd-st7789"].manifest;
page.radios[0].checked=false;page.radios[1].checked=true;page.radios[1].fire("change");
assert.equal(page.ids["accept-erase"].checked,false,"Changing boards must clear consent");
assert.equal(page.ids["install-cyd-st7789"].hidden,true);
await until(()=>Boolean(page.ids["install-cyd"].manifest));
assert.ok(page.urls.has(firstURL),"Keep verified blobs alive across board changes");
page.ids["accept-erase"].checked=true;page.ids["accept-erase"].fire("change");
assert.equal(page.ids["install-cyd"].hidden,false);
page.setDialog(true);assert.equal(page.ids["board-choices"].disabled,true);assert.equal(page.ids["install-cyd"].hidden,true);
page.setDialog(false);assert.equal(page.ids["board-choices"].disabled,false);
page.callbacks.pagehide();assert.equal(page.urls.size,0);
const unsupported=fixture({supported:false});await mountInstaller({...unsupported,importTools:async()=>{}});
assert.equal(unsupported.fetches,0);assert.equal(unsupported.ids["install-cyd-st7789"].hidden,true);
const insecure=fixture({secure:false});await mountInstaller({...insecure,importTools:async()=>{throw Error("Must not import on HTTP");}});
assert.equal(insecure.fetches,0);assert.match(insecure.ids["firmware-status"].textContent,/HTTPS/);
const broken=fixture({corrupt:true});await mountInstaller({...broken,importTools:async()=>{}});
assert.equal(broken.ids["retry"].hidden,false);assert.equal(broken.ids["install-cyd-st7789"].hidden,true);
broken.setDamage(false);broken.ids["retry"].fire("click");
await until(()=>Boolean(broken.ids["install-cyd-st7789"].manifest));
assert.equal(broken.ids["install-cyd-st7789"].hidden,true,"Retry cannot bypass consent");
broken.callbacks.pagehide();
console.log("PASS: consent, board-switch reset, dialog lock, unsupported/HTTP guards, corruption rejection, retry and blob cleanup");

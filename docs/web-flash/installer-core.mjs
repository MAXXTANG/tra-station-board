export function canInstall({supported, secure, accepted, ready, busy}) {
  return Boolean(supported && secure && accepted && ready && !busy);
}

export function validateManifest(manifest, profile, version, manifestURL, catalogURL) {
  const builds = manifest.builds;
  if (manifest.version !== version || manifest.new_install_prompt_erase !== false ||
      manifest.new_install_improv_wait_time !== 0 || !Array.isArray(builds) || builds.length !== 1 ||
      builds[0].chipFamily !== "ESP32" || builds[0].parts?.length !== 1 ||
      builds[0].parts[0].offset !== 0) {
    throw new Error("韌體清單不符合這個安裝頁的板型與完整安裝設定。");
  }
  const firmwareURL = new URL(builds[0].parts[0].path, manifestURL);
  const expected = new URL(profile.firmware, catalogURL);
  if (firmwareURL.href !== expected.href || expected.origin !== new URL(catalogURL).origin ||
      profile.flash_bytes !== 4194304) throw new Error("韌體路徑或容量不正確。");
  return firmwareURL;
}

export async function verifyFirmware(buffer, profile, cryptoImpl) {
  if (buffer.byteLength !== profile.size || profile.size !== 4128768 ||
      !/^[a-f0-9]{64}$/.test(profile.sha256)) throw new Error("韌體長度不正確。");
  const digest = await cryptoImpl.subtle.digest("SHA-256", buffer);
  const hex = [...new Uint8Array(digest)].map(b => b.toString(16).padStart(2, "0")).join("");
  if (hex !== profile.sha256) throw new Error("韌體校驗失敗，已阻止燒錄。請重新載入。");
}

export async function prepareFirmware(profile, version, catalogURL, fetchImpl, cryptoImpl) {
  const manifestURL = new URL(profile.manifest, catalogURL);
  const manifestResponse = await fetchImpl(manifestURL);
  if (!manifestResponse.ok) throw new Error("無法載入韌體清單。");
  const manifest = await manifestResponse.json();
  const firmwareURL = validateManifest(manifest, profile, version, manifestURL, catalogURL);
  const firmwareResponse = await fetchImpl(firmwareURL);
  if (!firmwareResponse.ok) throw new Error("無法下載韌體。");
  const buffer = await firmwareResponse.arrayBuffer();
  await verifyFirmware(buffer, profile, cryptoImpl);
  return {manifest, buffer};
}

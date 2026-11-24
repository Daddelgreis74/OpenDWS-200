const { app, BrowserWindow, dialog, ipcMain, session, shell } = require('electron');
const { execFile } = require('node:child_process');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const REPO_API_RELEASES = 'https://api.github.com/repos/Daddelgreis74/OpenDWS-200/releases?per_page=20';
const MIN_FW_BYTES = 4096;
const MAX_FW_BYTES = 245760;

let mainWindow = null;
let pendingSerialCallback = null;
let autoSelectOpenDws = false;

function clearPendingSerialCallback() {
  if (typeof pendingSerialCallback === 'function') {
    const cb = pendingSerialCallback;
    pendingSerialCallback = null;
    try {
      cb('');
    } catch {
      // Ignore stale callback errors during shutdown
    }
  }
}

function isOpenDwsPort(port) {
  const vidRaw = String(port.vendorId || '').trim().toLowerCase();
  const pidRaw = String(port.productId || '').trim().toLowerCase();
  const vidMatch = vidRaw === '2e3c' || vidRaw === '0x2e3c' || Number.parseInt(vidRaw, 10) === 0x2e3c;
  const pidMatch = pidRaw === '5740' || pidRaw === '0x5740' || Number.parseInt(pidRaw, 10) === 0x5740;
  return vidMatch && (!pidRaw || pidMatch);
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 860,
    minWidth: 980,
    minHeight: 700,
    backgroundColor: '#0b101b',
    title: 'OpenDWS-200 Studio',
    icon: path.join(__dirname, 'assets', 'icon.png'),
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));
  mainWindow.on('closed', () => {
    clearPendingSerialCallback();
    mainWindow = null;
  });
}

function buildGithubHeaders(token, accept) {
  const headers = {
    Accept: accept || 'application/vnd.github+json',
    'User-Agent': `OpenDWS-200-Studio/${app.getVersion()}`,
  };
  const trimmed = String(token || '').trim();
  if (trimmed) {
    headers.Authorization = `Bearer ${trimmed}`;
  }
  return headers;
}

function detectBootloaderVolume() {
  return new Promise((resolve) => {
    if (process.platform !== 'win32') {
      resolve({ found: false, reason: 'Bootloader-Autoerkennung ist auf Windows verfügbar.' });
      return;
    }
    const ps = [
      "Get-Volume | Where-Object { $_.FileSystemLabel -eq 'BOOTLOADER' }",
      '| Select-Object DriveLetter, FileSystemLabel, Size, SizeRemaining',
      '| ConvertTo-Json -Compress',
    ].join(' ');
    execFile('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', ps], { timeout: 6000 }, (err, stdout) => {
      if (err) {
        resolve({ found: false, reason: err.message });
        return;
      }
      const raw = String(stdout || '').trim();
      if (!raw) {
        resolve({ found: false });
        return;
      }
      try {
        const parsed = JSON.parse(raw);
        const vol = Array.isArray(parsed) ? parsed[0] : parsed;
        if (!vol || !vol.DriveLetter) {
          resolve({ found: false });
          return;
        }
        const driveLetter = String(vol.DriveLetter).toUpperCase().slice(0, 1);
        const rootPath = `${driveLetter}:\\`;
        if (!fs.existsSync(rootPath)) {
          resolve({ found: false });
          return;
        }
        resolve({
          found: true,
          driveLetter,
          rootPath,
          label: String(vol.FileSystemLabel || 'BOOTLOADER'),
          sizeBytes: Number(vol.Size || 0),
          freeBytes: Number(vol.SizeRemaining || 0),
        });
      } catch (parseErr) {
        resolve({ found: false, reason: parseErr.message });
      }
    });
  });
}

function assertValidFirmwareBuffer(buf) {
  if (!Buffer.isBuffer(buf) || buf.length < MIN_FW_BYTES || buf.length > MAX_FW_BYTES) {
    throw new Error(`Ungültige Firmware-Größe (${buf ? buf.length : 0} Bytes, erwartet ${MIN_FW_BYTES}..${MAX_FW_BYTES}).`);
  }
}

function assertBootloaderRoot(driveLetter) {
  const letter = String(driveLetter || '').trim().toUpperCase();
  if (!/^[A-Z]$/.test(letter)) {
    throw new Error('Ungültiger Laufwerksbuchstabe für BOOTLOADER.');
  }
  const rootPath = `${letter}:\\`;
  if (!fs.existsSync(rootPath)) {
    throw new Error(`Laufwerk ${rootPath} nicht gefunden. Bitte Station im BOOTLOADER-Modus verbinden.`);
  }
  return rootPath;
}

app.whenReady().then(() => {
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => permission === 'serial');
  session.defaultSession.setDevicePermissionHandler((details) => details.deviceType === 'serial');

  session.defaultSession.on('select-serial-port', (event, portList, _webContents, callback) => {
    event.preventDefault();
    clearPendingSerialCallback();
    const autoSelect = autoSelectOpenDws;
    autoSelectOpenDws = false;

    const mapped = (portList || [])
      .map((p) => ({
        portId: p.portId,
        portName: p.portName || p.displayName || p.portId,
        displayName: p.displayName || p.portName || '',
        vendorId: p.vendorId || '',
        productId: p.productId || '',
        serialNumber: p.serialNumber || '',
        isOpenDws: isOpenDwsPort(p),
      }))
      .sort((a, b) => {
        if (a.isOpenDws !== b.isOpenDws) return a.isOpenDws ? -1 : 1;
        return String(a.portName).localeCompare(String(b.portName));
      });

    if (autoSelect) {
      const match = mapped.find((p) => p.isOpenDws);
      callback(match ? String(match.portId) : '');
      return;
    }

    pendingSerialCallback = callback;

    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('serial:port-list', mapped);
    } else {
      clearPendingSerialCallback();
    }
  });

  ipcMain.handle('serial:set-auto-select', (_event, enabled) => {
    autoSelectOpenDws = !!enabled;
    return autoSelectOpenDws;
  });

  ipcMain.handle('serial:select-port', (_event, portId) => {
    if (typeof pendingSerialCallback === 'function') {
      const cb = pendingSerialCallback;
      pendingSerialCallback = null;
      cb(String(portId || ''));
      return true;
    }
    return false;
  });

  ipcMain.handle('csv:save', async (_event, csvContent) => {
    const now = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    const stamp = `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}-${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
    const res = await dialog.showSaveDialog(mainWindow, {
      title: 'OpenDWS-200 Telemetrie als CSV speichern',
      defaultPath: `opendws-log-${stamp}.csv`,
      filters: [{ name: 'CSV Dateien', extensions: ['csv'] }],
    });
    if (res.canceled || !res.filePath) {
      return { canceled: true };
    }
    await fs.promises.writeFile(res.filePath, String(csvContent || ''), 'utf8');
    return { canceled: false, path: res.filePath };
  });

  ipcMain.handle('fw:detect-bootloader', async () => detectBootloaderVolume());

  ipcMain.handle('fw:pick-local-bin', async () => {
    const res = await dialog.showOpenDialog(mainWindow, {
      title: 'Firmware-Binary (.bin) auswählen',
      filters: [{ name: 'Firmware Binary', extensions: ['bin'] }],
      properties: ['openFile'],
    });
    if (res.canceled || !res.filePaths || res.filePaths.length === 0) {
      return { canceled: true };
    }
    const filePath = res.filePaths[0];
    const buf = await fs.promises.readFile(filePath);
    assertValidFirmwareBuffer(buf);
    const sha256 = crypto.createHash('sha256').update(buf).digest('hex');
    return {
      canceled: false,
      path: filePath,
      name: path.basename(filePath),
      size: buf.length,
      sha256,
    };
  });

  ipcMain.handle('fw:flash-local', async (_event, { sourcePath, driveLetter }) => {
    const buf = await fs.promises.readFile(String(sourcePath || ''));
    assertValidFirmwareBuffer(buf);
    const rootPath = assertBootloaderRoot(driveLetter);
    const destPath = path.join(rootPath, path.basename(String(sourcePath)));
    await fs.promises.writeFile(destPath, buf);
    const sha256 = crypto.createHash('sha256').update(buf).digest('hex');
    return { ok: true, destPath, bytesWritten: buf.length, sha256 };
  });

  ipcMain.handle('fw:check-github', async (_event, { token }) => {
    const resp = await fetch(REPO_API_RELEASES, {
      headers: buildGithubHeaders(token, 'application/vnd.github+json'),
    });
    if (!resp.ok) {
      if (resp.status === 404 || resp.status === 401 || resp.status === 403) {
        throw new Error(`GitHub API HTTP ${resp.status} — für private Repositories bitte ein GitHub Token eintragen.`);
      }
      throw new Error(`GitHub API HTTP ${resp.status}`);
    }
    const releases = await resp.json();
    if (!Array.isArray(releases)) {
      throw new Error('Unerwartete Antwort der GitHub API.');
    }
    for (const rel of releases) {
      if (!rel || rel.draft) continue;
      const assets = Array.isArray(rel.assets) ? rel.assets : [];
      const binAsset =
        assets.find((a) => /^OpenDWS-200-Firmware.*\.bin$/i.test(a.name)) ||
        assets.find((a) => /^APP_DWS_200_.*\.bin$/i.test(a.name)) ||
        assets.find((a) => /\.bin$/i.test(a.name));
      if (!binAsset) continue;
      const shaAsset = assets.find((a) => a.name === `${binAsset.name}.sha256`) || null;
      return {
        ok: true,
        tag: rel.tag_name,
        name: rel.name || rel.tag_name,
        publishedAt: rel.published_at || '',
        htmlUrl: rel.html_url || '',
        binAsset: {
          name: binAsset.name,
          size: binAsset.size,
          url: binAsset.url,
          browserDownloadUrl: binAsset.browser_download_url,
        },
        shaAsset: shaAsset
          ? {
              name: shaAsset.name,
              url: shaAsset.url,
              browserDownloadUrl: shaAsset.browser_download_url,
            }
          : null,
      };
    }
    throw new Error('Kein Firmware-Release mit .bin Asset gefunden.');
  });

  ipcMain.handle('fw:download-and-flash', async (_event, { binAsset, shaAsset, driveLetter, token }) => {
    if (!binAsset || !binAsset.url || !binAsset.name) {
      throw new Error('Ungültige Release-Asset-Daten.');
    }
    const rootPath = assertBootloaderRoot(driveLetter);

    const binResp = await fetch(binAsset.url, {
      headers: buildGithubHeaders(token, 'application/octet-stream'),
    });
    if (!binResp.ok) {
      throw new Error(`Download fehlgeschlagen (HTTP ${binResp.status}).`);
    }
    const buf = Buffer.from(await binResp.arrayBuffer());
    assertValidFirmwareBuffer(buf);
    const sha256 = crypto.createHash('sha256').update(buf).digest('hex');

    let verified = false;
    if (shaAsset && shaAsset.url) {
      const shaResp = await fetch(shaAsset.url, {
        headers: buildGithubHeaders(token, 'application/octet-stream'),
      });
      if (!shaResp.ok) {
        throw new Error(`SHA-256-Prüfdatei konnte nicht geladen werden (HTTP ${shaResp.status}).`);
      }
      const shaText = await shaResp.text();
      const match = /([0-9a-fA-F]{64})/.exec(shaText);
      if (match && match[1].toLowerCase() !== sha256.toLowerCase()) {
        throw new Error(`SHA-256 stimmt nicht überein! Erwartet ${match[1]}, erhalten ${sha256}`);
      }
      if (match) {
        verified = true;
      }
    }

    const safeName = path.basename(String(binAsset.name));
    const destPath = path.join(rootPath, safeName);
    await fs.promises.writeFile(destPath, buf);
    return {
      ok: true,
      destPath,
      bytesWritten: buf.length,
      sha256,
      verified,
    };
  });

  ipcMain.handle('app:open-external', async (_event, rawUrl) => {
    const u = String(rawUrl || '');
    if (u.startsWith('https://github.com/Daddelgreis74/OpenDWS-200')) {
      await shell.openExternal(u);
      return true;
    }
    return false;
  });

  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  clearPendingSerialCallback();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

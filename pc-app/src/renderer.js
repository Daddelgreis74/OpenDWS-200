(function () {
  'use strict';

  const { parseInfoLine, parseStatLine, formatCsvHeader, formatCsvRow } = window.OpenDwsProtocol;

  const MAX_HISTORY_SAMPLES = 3000;
  const CHART_TEMP_MAX = 480;
  const OPENDWS_VID = 0x2e3c;
  const OPENDWS_PID = 0x5740;
  const STORAGE_AUTO_CONNECT = 'opendws.autoConnect';

  const state = {
    port: null,
    reader: null,
    writer: null,
    connected: false,
    connecting: false,
    manualDisconnect: false,
    autoConnect: localStorage.getItem(STORAGE_AUTO_CONNECT) !== '0',
    chartPaused: false,
    windowSec: 180,
    samples: [],
    lastStat: null,
    lastInfo: null,
    logging: false,
    logStartMs: 0,
    logRows: [],
    bootloader: { found: false, driveLetter: '' },
    githubRelease: null,
    localBin: null,
  };

  const el = {
    connDot: document.getElementById('connDot'),
    mcuLabel: document.getElementById('mcuLabel'),
    badgeFw: document.getElementById('badgeFw'),
    badgeHandle: document.getElementById('badgeHandle'),
    badgeState: document.getElementById('badgeState'),
    badgeFault: document.getElementById('badgeFault'),
    chkAutoConnect: document.getElementById('chkAutoConnect'),
    btnConnect: document.getElementById('btnConnect'),

    kpiTemp: document.getElementById('kpiTemp'),
    kpiTarget: document.getElementById('kpiTarget'),
    kpiPower: document.getElementById('kpiPower'),
    dutyFill: document.getElementById('dutyFill'),
    kpiWatts: document.getElementById('kpiWatts'),
    kpiCurrent: document.getElementById('kpiCurrent'),
    kpiPeak: document.getElementById('kpiPeak'),
    kpiRail: document.getElementById('kpiRail'),
    kpiStand: document.getElementById('kpiStand'),

    selectWindow: document.getElementById('selectWindow'),
    btnChartPause: document.getElementById('btnChartPause'),
    btnChartClear: document.getElementById('btnChartClear'),
    chart: document.getElementById('telemetryChart'),

    ctrlTargetBadge: document.getElementById('ctrlTargetBadge'),
    sliderTemp: document.getElementById('sliderTemp'),
    btnTempMinus: document.getElementById('btnTempMinus'),
    btnTempPlus: document.getElementById('btnTempPlus'),
    btnHeatOn: document.getElementById('btnHeatOn'),
    btnHeatOff: document.getElementById('btnHeatOff'),
    btnClearFault: document.getElementById('btnClearFault'),
    btnSleepOn: document.getElementById('btnSleepOn'),
    btnSleepOff: document.getElementById('btnSleepOff'),
    inputSleepTemp: document.getElementById('inputSleepTemp'),
    btnSetSleepTemp: document.getElementById('btnSetSleepTemp'),
    inputCal: document.getElementById('inputCal'),
    btnSetCal: document.getElementById('btnSetCal'),
    sliderBl: document.getElementById('sliderBl'),
    valBl: document.getElementById('valBl'),
    selectStream: document.getElementById('selectStream'),
    btnBeep: document.getElementById('btnBeep'),
    btnLedAuto: document.getElementById('btnLedAuto'),
    btnSaveFlash: document.getElementById('btnSaveFlash'),
    btnReboot: document.getElementById('btnReboot'),

    statusToast: document.getElementById('statusToast'),
    btnLogStart: document.getElementById('btnLogStart'),
    btnLogStop: document.getElementById('btnLogStop'),
    btnLogExport: document.getElementById('btnLogExport'),
    btnLogClear: document.getElementById('btnLogClear'),
    logStatusBadge: document.getElementById('logStatusBadge'),
    logCount: document.getElementById('logCount'),
    logElapsed: document.getElementById('logElapsed'),

    chkHideStat: document.getElementById('chkHideStat'),
    btnConsoleClear: document.getElementById('btnConsoleClear'),
    consoleLog: document.getElementById('consoleLog'),
    consoleForm: document.getElementById('consoleForm'),
    consoleInput: document.getElementById('consoleInput'),

    bootloaderStatus: document.getElementById('bootloaderStatus'),
    btnDetectBootloader: document.getElementById('btnDetectBootloader'),
    inputGhToken: document.getElementById('inputGhToken'),
    btnCheckGithub: document.getElementById('btnCheckGithub'),
    githubReleaseInfo: document.getElementById('githubReleaseInfo'),
    btnDownloadFlash: document.getElementById('btnDownloadFlash'),
    linkGithubRelease: document.getElementById('linkGithubRelease'),
    localBinInfo: document.getElementById('localBinInfo'),
    btnPickLocalBin: document.getElementById('btnPickLocalBin'),
    btnFlashLocalBin: document.getElementById('btnFlashLocalBin'),

    portModal: document.getElementById('portModal'),
    portList: document.getElementById('portList'),
    btnCancelPort: document.getElementById('btnCancelPort'),
  };

  function setToast(msg) {
    el.statusToast.textContent = msg;
  }

  function appendConsole(text, prefix = '< ') {
    const line = `${prefix}${text}\n`;
    el.consoleLog.textContent += line;
    if (el.consoleLog.textContent.length > 32000) {
      el.consoleLog.textContent = el.consoleLog.textContent.slice(-24000);
    }
    el.consoleLog.scrollTop = el.consoleLog.scrollHeight;
  }

  function applyStateBadgeStyle(stName, hasFault) {
    el.badgeState.className = 'badge';
    if (hasFault || stName === 'FAULT') {
      el.badgeState.classList.add('badge-danger');
    } else if (stName === 'HEAT') {
      el.badgeState.classList.add('badge-warn');
    } else if (stName === 'SLEEP' || stName === 'PROBE') {
      el.badgeState.classList.add('badge-cyan');
    } else if (stName === 'STANDBY') {
      el.badgeState.classList.add('badge-ok');
    } else {
      el.badgeState.classList.add('badge-muted');
    }
    el.badgeState.textContent = stName;
  }

  function updateTelemetryUI(stat) {
    state.lastStat = stat;
    const activeFault = stat.faultName && stat.faultName !== 'NONE' && stat.faultName !== 'SETTLING';

    el.badgeHandle.textContent = `HDL:${stat.handle}`;
    el.badgeHandle.className = `badge ${stat.handle === 'F245' ? 'badge-cyan' : 'badge-warn'}`;

    const isReady = stat.state === 'HEAT' && Math.abs(stat.tipC - stat.actC) <= 3.0;
    if (isReady) {
      el.badgeState.className = 'badge badge-ok';
      el.badgeState.textContent = 'READY';
    } else {
      applyStateBadgeStyle(stat.state, activeFault);
    }

    if (activeFault) {
      el.badgeFault.classList.remove('hidden');
      el.badgeFault.className = 'badge badge-danger';
      el.badgeFault.textContent = `${stat.faultHex} ${stat.faultName}`;
    } else {
      el.badgeFault.classList.add('hidden');
    }

    el.kpiTemp.textContent = stat.faultName === 'OPEN_TIP' ? 'OPEN!' : `${stat.tipC.toFixed(1)} °C`;
    el.kpiTemp.className = `kpi-value ${
      stat.faultName === 'OPEN_TIP' || activeFault ? 'danger' : stat.state === 'SLEEP' ? 'cyan' : isReady ? 'ok' : 'amber'
    }`;
    el.kpiTarget.textContent = `Soll: ${stat.targetC} °C · Aktiv: ${stat.actC} °C`;

    el.kpiPower.textContent = `${stat.dutyPct} %`;
    el.dutyFill.style.width = `${Math.max(0, Math.min(100, stat.dutyPct))}%`;
    el.kpiWatts.textContent = `${stat.watts.toFixed(0)} W · ${stat.ticks} Ticks`;

    el.kpiCurrent.textContent = `${(stat.curMa / 1000).toFixed(2)} A`;
    el.kpiPeak.textContent = `Peak: ${(stat.curPkMa / 1000).toFixed(2)} A · Zero: ${stat.zeroMv} mV`;

    const railV = stat.railMv / 1000;
    el.kpiRail.textContent = `${railV.toFixed(1)} V`;
    el.kpiRail.className = `kpi-value ${railV >= 15 && railV <= 30 ? 'ok' : 'danger'}`;
    el.kpiStand.textContent = `Ständer: ${stat.stand ? 'IN (AUTO)' : 'ENTNOMMEN'} · ${stat.faultName}`;

    if (document.activeElement !== el.sliderTemp) {
      el.sliderTemp.value = String(stat.targetC);
      el.ctrlTargetBadge.textContent = `${stat.targetC} °C`;
    }
    if (document.activeElement !== el.sliderBl) {
      el.sliderBl.value = String(stat.bl);
      el.valBl.textContent = `${stat.bl}%`;
    }

    document.querySelectorAll('.btn-preset').forEach((btn) => {
      const idx = btn.dataset.preset;
      const match =
        (idx === '1' && stat.targetC === 200) ||
        (idx === '2' && stat.targetC === 300) ||
        (idx === '3' && stat.targetC === 350);
      btn.classList.toggle('active', match);
    });
  }

  function updateInfoUI(info) {
    state.lastInfo = info;
    el.badgeFw.textContent = `FW: ${info.fw}`;
    el.badgeFw.className = 'badge badge-cyan';
    el.mcuLabel.textContent = `${info.mcu} @ ${info.clk} · Verbunden`;
  }

  function handleIncomingLine(rawLine) {
    const line = rawLine.trim();
    if (!line) return;

    const stat = parseStatLine(line);
    if (stat) {
      updateTelemetryUI(stat);
      if (!state.chartPaused) {
        state.samples.push({ t: Date.now(), stat });
        if (state.samples.length > MAX_HISTORY_SAMPLES) {
          state.samples.splice(0, state.samples.length - MAX_HISTORY_SAMPLES);
        }
        drawChart();
      }
      if (state.logging) {
        state.logRows.push(formatCsvRow(stat));
        el.logCount.textContent = String(state.logRows.length);
        updateLoggerElapsed();
      }
      if (!el.chkHideStat.checked) {
        appendConsole(line);
      }
      return;
    }

    const info = parseInfoLine(line);
    if (info) {
      updateInfoUI(info);
    }
    appendConsole(line);
  }

  async function sendCommand(cmd) {
    const clean = String(cmd || '').trim();
    if (!clean) return;
    if (!state.connected || !state.writer) {
      setToast('Nicht verbunden.');
      return;
    }
    try {
      const encoded = new TextEncoder().encode(`${clean}\n`);
      await state.writer.write(encoded);
      appendConsole(clean, '> ');
    } catch (err) {
      setToast(`Sende-Fehler: ${err.message}`);
    }
  }

  async function readLoop(readable) {
    const decoder = new TextDecoderStream();
    const pipeDone = readable.pipeTo(decoder.writable).catch(() => {});
    const reader = decoder.readable.getReader();
    state.reader = reader;

    let buf = '';
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += value;
        const lines = buf.split(/\r?\n/);
        buf = lines.pop() || '';
        for (const l of lines) {
          handleIncomingLine(l);
        }
      }
    } catch (err) {
      if (state.connected) {
        appendConsole(`USB-Lesefehler: ${err.message}`, '! ');
      }
    } finally {
      reader.releaseLock();
      await pipeDone;
      if (state.connected) {
        await disconnectSerial('Verbindung getrennt.');
      }
    }
  }

  function isOpenDwsWebSerialPort(port) {
    if (!port || typeof port.getInfo !== 'function') return false;
    const info = port.getInfo() || {};
    return info.usbVendorId === OPENDWS_VID && info.usbProductId === OPENDWS_PID;
  }

  async function openSerialPort(port) {
    await port.open({ baudRate: 115200 });
    try {
      state.port = port;
      state.writer = port.writable.getWriter();
      state.connected = true;
      state.manualDisconnect = false;
    } catch (err) {
      state.port = null;
      state.writer = null;
      state.connected = false;
      await port.close().catch(() => {});
      throw err;
    }

    el.connDot.classList.add('online');
    el.btnConnect.textContent = 'Trennen';
    el.btnConnect.className = 'btn btn-danger';
    el.mcuLabel.textContent = 'Verbunden — frage Geräteinformationen ab...';
    setToast('Verbunden mit OpenDWS-200.');

    readLoop(port.readable);

    await sendCommand('INFO');
    await sendCommand('STAT');
    const interval = el.selectStream.value;
    await sendCommand(interval === 'OFF' ? 'STREAM OFF' : `STREAM ${interval}`);
  }

  async function connectSerial() {
    if (!('serial' in navigator)) {
      setToast('Web Serial wird in dieser Umgebung nicht unterstützt.');
      return;
    }
    if (state.connected || state.connecting) return;
    state.manualDisconnect = false;
    state.connecting = true;
    try {
      await window.opendws.setAutoSelect(false);
      const port = await navigator.serial.requestPort();
      await openSerialPort(port);
    } catch (err) {
      if (err && err.name !== 'NotFoundError') {
        setToast(`Verbindung fehlgeschlagen: ${err.message}`);
      }
    } finally {
      state.connecting = false;
    }
  }

  async function tryAutoConnect() {
    if (!('serial' in navigator)) return;
    if (!state.autoConnect || state.connected || state.connecting || state.manualDisconnect) return;
    if (el.portModal && !el.portModal.classList.contains('hidden')) return;

    state.connecting = true;
    try {
      if (typeof navigator.serial.getPorts === 'function') {
        const granted = await navigator.serial.getPorts();
        const known = granted.find(isOpenDwsWebSerialPort);
        if (known) {
          try {
            await openSerialPort(known);
            return;
          } catch (_err) {
            // Fall through to requestPort if previously granted handle failed to reopen
          }
        }
      }
      await window.opendws.setAutoSelect(true);
      const port = await navigator.serial.requestPort();
      await openSerialPort(port);
    } catch (_err) {
      // Ignored during background auto-connect (e.g. NotFoundError when unplugged)
    } finally {
      window.opendws.setAutoSelect(false).catch(() => {});
      state.connecting = false;
    }
  }

  async function disconnectSerial(message = 'Getrennt.', manual = false) {
    state.connected = false;
    if (manual) {
      state.manualDisconnect = true;
    }
    try {
      if (state.reader) {
        await state.reader.cancel().catch(() => {});
        state.reader = null;
      }
    } catch {}
    try {
      if (state.writer) {
        await state.writer.close().catch(() => {});
        state.writer.releaseLock();
        state.writer = null;
      }
    } catch {}
    try {
      if (state.port) {
        await state.port.close().catch(() => {});
        state.port = null;
      }
    } catch {}

    el.connDot.classList.remove('online');
    el.btnConnect.textContent = 'Verbinden';
    el.btnConnect.className = 'btn btn-primary';
    el.badgeState.className = 'badge badge-muted';
    el.badgeState.textContent = 'OFFLINE';
    el.mcuLabel.textContent = 'Nicht verbunden (USB CDC VID 0x2E3C)';
    setToast(message);
  }

  /* High-DPI Live Dual-Axis Chart */
  function drawChart() {
    const canvas = el.chart;
    const rect = canvas.getBoundingClientRect();
    if (rect.width < 40 || rect.height < 40) return;

    const dpr = window.devicePixelRatio || 1;
    const w = Math.floor(rect.width);
    const h = Math.floor(rect.height);
    if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
      canvas.width = w * dpr;
      canvas.height = h * dpr;
    }

    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    const pad = { left: 44, right: 44, top: 14, bottom: 22 };
    const pw = w - pad.left - pad.right;
    const ph = h - pad.top - pad.bottom;
    if (pw <= 10 || ph <= 10) return;

    ctx.fillStyle = '#0d1421';
    ctx.fillRect(0, 0, w, h);

    /* Grid + Y-axis labels */
    ctx.strokeStyle = '#1c2a42';
    ctx.lineWidth = 1;
    ctx.font = '11px ui-monospace, Consolas, monospace';

    for (let i = 0; i <= 4; i++) {
      const y = pad.top + (ph * i) / 4;
      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(pad.left + pw, y);
      ctx.stroke();

      const tempLabel = Math.round(CHART_TEMP_MAX * (1 - i / 4));
      ctx.fillStyle = '#00e5ff';
      ctx.textAlign = 'right';
      ctx.fillText(`${tempLabel}°`, pad.left - 6, y + 4);

      const pwrLabel = Math.round(100 * (1 - i / 4));
      ctx.fillStyle = '#f59e0b';
      ctx.textAlign = 'left';
      ctx.fillText(`${pwrLabel}%`, pad.left + pw + 6, y + 4);
    }

    const now = Date.now();
    const spanMs = state.windowSec * 1000;
    const tStart = now - spanMs;

    for (let i = 0; i <= 4; i++) {
      const x = pad.left + (pw * i) / 4;
      ctx.beginPath();
      ctx.moveTo(x, pad.top);
      ctx.lineTo(x, pad.top + ph);
      ctx.stroke();

      const secAgo = Math.round(state.windowSec * (1 - i / 4));
      ctx.fillStyle = '#7a8ca5';
      ctx.textAlign = 'center';
      ctx.fillText(secAgo === 0 ? 'Jetzt' : `-${secAgo}s`, x, h - 6);
    }

    const visible = state.samples.filter((s) => s.t >= tStart);
    if (visible.length === 0) {
      ctx.fillStyle = '#7a8ca5';
      ctx.textAlign = 'center';
      ctx.fillText('Warte auf Telemetrie...', pad.left + pw / 2, pad.top + ph / 2);
      return;
    }

    const xOf = (t) => pad.left + ((t - tStart) / spanMs) * pw;
    const yTemp = (c) => pad.top + ph * (1 - Math.max(0, Math.min(CHART_TEMP_MAX, c)) / CHART_TEMP_MAX);
    const yPwr = (p) => pad.top + ph * (1 - Math.max(0, Math.min(100, p)) / 100);

    /* Power filled area */
    ctx.beginPath();
    ctx.moveTo(xOf(visible[0].t), pad.top + ph);
    for (const s of visible) {
      ctx.lineTo(xOf(s.t), yPwr(s.stat.dutyPct));
    }
    ctx.lineTo(xOf(visible[visible.length - 1].t), pad.top + ph);
    ctx.closePath();
    ctx.fillStyle = 'rgba(245, 158, 11, 0.16)';
    ctx.fill();

    /* Power curve */
    ctx.beginPath();
    visible.forEach((s, idx) => {
      const x = xOf(s.t);
      const y = yPwr(s.stat.dutyPct);
      if (idx === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#f59e0b';
    ctx.lineWidth = 1.6;
    ctx.stroke();

    /* Dashed Target curve */
    ctx.save();
    ctx.setLineDash([5, 5]);
    ctx.beginPath();
    visible.forEach((s, idx) => {
      const x = xOf(s.t);
      const y = yTemp(s.stat.actC || s.stat.targetC);
      if (idx === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#7a8ca5';
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.restore();

    /* Temperature curve */
    ctx.beginPath();
    visible.forEach((s, idx) => {
      const x = xOf(s.t);
      const y = yTemp(s.stat.tipC);
      if (idx === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#00e5ff';
    ctx.lineWidth = 2.2;
    ctx.stroke();
  }

  function updateLoggerElapsed() {
    if (!state.logging) return;
    const s = Math.floor((Date.now() - state.logStartMs) / 1000);
    const mm = String(Math.floor(s / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    el.logElapsed.textContent = `${mm}:${ss}`;
  }

  /* COM Port Modal Handler */
  window.opendws.onSerialPortList((ports) => {
    el.portList.innerHTML = '';
    if (!ports || ports.length === 0) {
      el.portList.innerHTML = '<div class="muted">Keine seriellen Ports gefunden. Bitte Station anschließen.</div>';
    } else {
      for (const p of ports) {
        const div = document.createElement('div');
        div.className = `port-item ${p.isOpenDws ? 'opendws' : ''}`;
        const badge = p.isOpenDws
          ? '<span class="badge badge-cyan">OpenDWS-200</span>'
          : '<span class="badge badge-muted">COM-Port</span>';
        div.innerHTML = `
          <div>
            <div><strong>${p.portName}</strong></div>
            <div class="muted" style="font-size:11px">${p.displayName || ''} ${p.vendorId ? `(VID:${p.vendorId})` : ''}</div>
          </div>
          ${badge}
        `;
        div.addEventListener('click', () => {
          el.portModal.classList.add('hidden');
          window.opendws.selectSerialPort(p.portId);
        });
        el.portList.appendChild(div);
      }
    }
    el.portModal.classList.remove('hidden');
  });

  el.btnCancelPort.addEventListener('click', () => {
    el.portModal.classList.add('hidden');
    window.opendws.selectSerialPort('');
  });

  /* Connect & Chart Controls */
  if (el.chkAutoConnect) {
    el.chkAutoConnect.checked = state.autoConnect;
    el.chkAutoConnect.addEventListener('change', () => {
      state.autoConnect = el.chkAutoConnect.checked;
      localStorage.setItem(STORAGE_AUTO_CONNECT, state.autoConnect ? '1' : '0');
      if (state.autoConnect) {
        state.manualDisconnect = false;
        tryAutoConnect();
      }
    });
  }

  el.btnConnect.addEventListener('click', () => {
    if (state.connected) disconnectSerial('Verbindung getrennt.', true);
    else connectSerial();
  });

  el.selectWindow.addEventListener('change', () => {
    state.windowSec = Number.parseInt(el.selectWindow.value, 10) || 180;
    drawChart();
  });

  el.btnChartPause.addEventListener('click', () => {
    state.chartPaused = !state.chartPaused;
    el.btnChartPause.textContent = state.chartPaused ? 'Fortsetzen' : 'Pause';
    el.btnChartPause.classList.toggle('btn-warn', state.chartPaused);
  });

  el.btnChartClear.addEventListener('click', () => {
    state.samples = [];
    drawChart();
  });

  /* Remote Control Events */
  el.sliderTemp.addEventListener('input', () => {
    el.ctrlTargetBadge.textContent = `${el.sliderTemp.value} °C`;
  });
  el.sliderTemp.addEventListener('change', () => {
    sendCommand(`TEMP ${el.sliderTemp.value}`);
  });
  el.btnTempMinus.addEventListener('click', () => {
    const v = Math.max(100, Number(el.sliderTemp.value) - 5);
    el.sliderTemp.value = String(v);
    el.ctrlTargetBadge.textContent = `${v} °C`;
    sendCommand(`TEMP ${v}`);
  });
  el.btnTempPlus.addEventListener('click', () => {
    const v = Math.min(450, Number(el.sliderTemp.value) + 5);
    el.sliderTemp.value = String(v);
    el.ctrlTargetBadge.textContent = `${v} °C`;
    sendCommand(`TEMP ${v}`);
  });

  document.querySelectorAll('.btn-preset').forEach((btn) => {
    btn.addEventListener('click', () => sendCommand(`PRESET ${btn.dataset.preset}`));
  });

  el.btnHeatOn.addEventListener('click', () => sendCommand('HEAT ON'));
  el.btnHeatOff.addEventListener('click', () => sendCommand('HEAT OFF'));
  el.btnClearFault.addEventListener('click', () => sendCommand('CLEAR'));
  el.btnSleepOn.addEventListener('click', () => sendCommand('SLEEP ON'));
  el.btnSleepOff.addEventListener('click', () => sendCommand('SLEEP OFF'));
  el.btnSetSleepTemp.addEventListener('click', () => sendCommand(`SLEEP ${el.inputSleepTemp.value}`));
  el.btnSetCal.addEventListener('click', () => sendCommand(`CAL ${el.inputCal.value}`));

  el.sliderBl.addEventListener('input', () => {
    el.valBl.textContent = `${el.sliderBl.value}%`;
  });
  el.sliderBl.addEventListener('change', () => sendCommand(`BL ${el.sliderBl.value}`));
  el.selectStream.addEventListener('change', () => {
    const v = el.selectStream.value;
    sendCommand(v === 'OFF' ? 'STREAM OFF' : `STREAM ${v}`);
  });

  el.btnBeep.addEventListener('click', () => sendCommand('BEEP 80'));
  el.btnLedAuto.addEventListener('click', () => sendCommand('LED AUTO'));
  el.btnSaveFlash.addEventListener('click', () => sendCommand('SAVE'));
  el.btnReboot.addEventListener('click', () => sendCommand('REBOOT'));

  /* Bottom Drawer Tabs */
  document.querySelectorAll('.tab-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach((b) => b.classList.remove('active'));
      document.querySelectorAll('.tab-panel').forEach((p) => p.classList.remove('active'));
      btn.classList.add('active');
      document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
    });
  });

  /* CSV Logger Events */
  el.btnLogStart.addEventListener('click', () => {
    state.logging = true;
    state.logStartMs = Date.now();
    el.btnLogStart.disabled = true;
    el.btnLogStop.disabled = false;
    el.logStatusBadge.textContent = 'AUFNAHME';
    el.logStatusBadge.className = 'badge badge-ok';
    setToast('CSV-Aufzeichnung gestartet.');
  });

  el.btnLogStop.addEventListener('click', () => {
    state.logging = false;
    el.btnLogStart.disabled = false;
    el.btnLogStop.disabled = true;
    el.btnLogExport.disabled = state.logRows.length === 0;
    el.logStatusBadge.textContent = 'GESTOPPT';
    el.logStatusBadge.className = 'badge badge-muted';
    setToast(`CSV-Aufzeichnung gestoppt (${state.logRows.length} Zeilen).`);
  });

  el.btnLogClear.addEventListener('click', () => {
    state.logRows = [];
    el.logCount.textContent = '0';
    el.logElapsed.textContent = '00:00';
    el.btnLogExport.disabled = true;
  });

  el.btnLogExport.addEventListener('click', async () => {
    if (state.logRows.length === 0) return;
    const csv = [formatCsvHeader(), ...state.logRows].join('\n');
    const res = await window.opendws.saveCsv(csv);
    if (res && !res.canceled) {
      setToast(`CSV gespeichert: ${res.path}`);
    }
  });

  /* Console Events */
  el.consoleForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const cmd = el.consoleInput.value.trim();
    if (!cmd) return;
    el.consoleInput.value = '';
    sendCommand(cmd);
  });
  document.querySelectorAll('.quick-cmds .btn').forEach((b) => {
    b.addEventListener('click', () => sendCommand(b.dataset.cmd));
  });
  el.btnConsoleClear.addEventListener('click', () => {
    el.consoleLog.textContent = '';
  });

  /* Firmware Updater Events */
  el.inputGhToken.value = localStorage.getItem('opendws_gh_token') || '';
  el.inputGhToken.addEventListener('change', () => {
    localStorage.setItem('opendws_gh_token', el.inputGhToken.value.trim());
  });

  function updateFlashButtons() {
    const hasBl = Boolean(state.bootloader && state.bootloader.found);
    el.btnDownloadFlash.disabled = !(hasBl && state.githubRelease && state.githubRelease.binAsset);
    el.btnFlashLocalBin.disabled = !(hasBl && state.localBin && state.localBin.path);
  }

  async function refreshBootloader() {
    el.bootloaderStatus.textContent = 'Suche...';
    el.bootloaderStatus.className = 'badge badge-muted';
    const res = await window.opendws.detectBootloader();
    state.bootloader = res || { found: false };
    if (state.bootloader.found) {
      el.bootloaderStatus.textContent = `Gefunden: ${state.bootloader.rootPath}`;
      el.bootloaderStatus.className = 'badge badge-ok';
      setToast(`BOOTLOADER-Laufwerk gefunden: ${state.bootloader.rootPath}`);
    } else {
      el.bootloaderStatus.textContent = 'Nicht gefunden';
      el.bootloaderStatus.className = 'badge badge-warn';
    }
    updateFlashButtons();
  }

  el.btnDetectBootloader.addEventListener('click', refreshBootloader);

  el.btnCheckGithub.addEventListener('click', async () => {
    el.githubReleaseInfo.textContent = 'Frage GitHub Releases ab...';
    el.btnDownloadFlash.disabled = true;
    try {
      const token = el.inputGhToken.value.trim();
      const rel = await window.opendws.checkGithubRelease(token);
      state.githubRelease = rel;
      el.githubReleaseInfo.textContent = `${rel.tag} · ${rel.binAsset.name} (${(rel.binAsset.size / 1024).toFixed(1)} KB)`;
      el.linkGithubRelease.classList.remove('hidden');
      el.linkGithubRelease.onclick = (e) => {
        e.preventDefault();
        window.opendws.openExternal(rel.htmlUrl);
      };
      setToast(`Neuestes Firmware-Release gefunden: ${rel.tag}`);
    } catch (err) {
      state.githubRelease = null;
      el.githubReleaseInfo.textContent = err.message;
    }
    updateFlashButtons();
  });

  el.btnDownloadFlash.addEventListener('click', async () => {
    if (!state.githubRelease || !state.bootloader.found) return;
    el.btnDownloadFlash.disabled = true;
    setToast('Lade Firmware & SHA-256 herunter und flashe...');
    try {
      const res = await window.opendws.downloadAndFlash({
        binAsset: state.githubRelease.binAsset,
        shaAsset: state.githubRelease.shaAsset,
        driveLetter: state.bootloader.driveLetter,
        token: el.inputGhToken.value.trim(),
      });
      setToast(`Erfolgreich geflasht (${res.bytesWritten} B${res.verified ? ', SHA-256 OK' : ''}): ${res.destPath}`);
    } catch (err) {
      setToast(`Flash-Fehler: ${err.message}`);
    }
    updateFlashButtons();
  });

  el.btnPickLocalBin.addEventListener('click', async () => {
    try {
      const res = await window.opendws.pickLocalBin();
      if (!res || res.canceled) return;
      state.localBin = res;
      el.localBinInfo.textContent = `${res.name} (${(res.size / 1024).toFixed(1)} KB) · ${res.sha256.slice(0, 12)}...`;
      updateFlashButtons();
    } catch (err) {
      setToast(`Datei ungültig: ${err.message}`);
    }
  });

  el.btnFlashLocalBin.addEventListener('click', async () => {
    if (!state.localBin || !state.bootloader.found) return;
    try {
      const res = await window.opendws.flashLocal({
        sourcePath: state.localBin.path,
        driveLetter: state.bootloader.driveLetter,
      });
      setToast(`Lokale Firmware geflasht (${res.bytesWritten} B): ${res.destPath}`);
    } catch (err) {
      setToast(`Flash-Fehler: ${err.message}`);
    }
  });

  window.addEventListener('resize', drawChart);
  setInterval(() => {
    updateLoggerElapsed();
    if (!state.chartPaused && state.samples.length > 0) {
      drawChart();
    }
  }, 500);
  setInterval(() => {
    el.btnLogExport.disabled = !(state.logRows.length > 0);
  }, 400);

  if ('serial' in navigator) {
    navigator.serial.addEventListener('connect', (event) => {
      if (event && event.target && !isOpenDwsWebSerialPort(event.target)) {
        return;
      }
      state.manualDisconnect = false;
      setTimeout(tryAutoConnect, 250);
    });
    navigator.serial.addEventListener('disconnect', (event) => {
      if (state.connected && event && event.target === state.port) {
        disconnectSerial('USB-Verbindung getrennt.');
      }
    });
  }
  setInterval(tryAutoConnect, 2500);
  setTimeout(tryAutoConnect, 300);

  drawChart();
})();

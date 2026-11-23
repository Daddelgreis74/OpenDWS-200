(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) {
    module.exports = api;
  }
  if (root) {
    root.OpenDwsProtocol = api;
  }
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';

  function parseKeyValues(body) {
    const out = Object.create(null);
    const tokens = String(body || '').trim().split(/\s+/);
    for (const tok of tokens) {
      if (!tok) continue;
      const eq = tok.indexOf('=');
      if (eq <= 0) continue;
      const k = tok.slice(0, eq);
      const v = tok.slice(eq + 1);
      out[k] = v;
    }
    return out;
  }

  function toInt(val, fallback) {
    if (val === undefined) return fallback;
    const n = Number.parseInt(val, 10);
    return Number.isFinite(n) ? n : fallback;
  }

  function toFloat(val, fallback) {
    if (val === undefined) return fallback;
    const n = Number.parseFloat(val);
    return Number.isFinite(n) ? n : fallback;
  }

  function parseFaultField(raw) {
    const str = String(raw || '0x00(NONE)').trim();
    const m = /^([^()]+)\(([^)]+)\)$/.exec(str);
    if (m) {
      return { faultHex: m[1], faultName: m[2] };
    }
    return { faultHex: str, faultName: str };
  }

  function parseInfoLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed.startsWith('INFO ')) return null;
    const kv = parseKeyValues(trimmed.slice(5));
    if (!kv.fw && !kv.mcu) return null;
    return {
      fw: kv.fw || '',
      mcu: kv.mcu || '',
      clk: kv.clk || '',
      safeMode: toInt(kv.safe_mode, 0),
      handle: kv.handle || '',
      state: kv.state || '',
      railMv: toInt(kv.rail_mv, 0),
      uptimeMs: toInt(kv.uptime_ms, 0),
    };
  }

  function parseStatLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed.startsWith('STAT ')) return null;
    const kv = parseKeyValues(trimmed.slice(5));
    if (kv.ms === undefined || kv.tip_c === undefined) return null;

    const fault = parseFaultField(kv.fault);
    const railMv = toInt(kv.rail_mv, 0);
    const curMa = toInt(kv.cur_ma, 0);
    const curPkMa = toInt(kv.cur_pk, 0);
    const watts = Math.max(0, (railMv / 1000) * (curMa / 1000));

    return {
      ms: toInt(kv.ms, 0),
      state: kv.state || 'UNK',
      faultHex: fault.faultHex,
      faultName: fault.faultName,
      handle: kv.handle || 'UNK',
      tipC: toFloat(kv.tip_c, 0),
      targetC: toInt(kv.target_c, 0),
      actC: toInt(kv.act_c, 0),
      dutyPct: toInt(kv.duty, 0),
      ticks: toInt(kv.ticks, 0),
      railMv,
      curMa,
      curPkMa,
      zeroMv: toInt(kv.zero_mv, 0),
      tipAdc: toInt(kv.tip_adc, 0),
      curAdc: toInt(kv.cur_adc, 0),
      auxAdc: toInt(kv.aux_adc, 0),
      stand: toInt(kv.stand, 0),
      sleep: toInt(kv.sleep, 0),
      keysHex: kv.keys || '0x00',
      pb5: toInt(kv.pb5, 0),
      pb6: toInt(kv.pb6, 0),
      bl: toInt(kv.bl, 0),
      ledHex: kv.led || '0x00',
      heaterOn: toInt(kv.heater, 0),
      watts,
    };
  }

  function formatCsvHeader() {
    return 'timestamp,ms,state,fault,handle,tip_c,target_c,act_c,duty_pct,ticks,rail_v,cur_a,cur_pk_a,watts,stand,sleep';
  }

  function formatCsvRow(stat, isoTime) {
    const ts = isoTime || new Date().toISOString();
    const railV = (stat.railMv / 1000).toFixed(3);
    const curA = (stat.curMa / 1000).toFixed(3);
    const pkA = (stat.curPkMa / 1000).toFixed(3);
    const watts = stat.watts.toFixed(1);
    const faultLabel = `${stat.faultHex}(${stat.faultName})`;
    return [
      ts,
      stat.ms,
      stat.state,
      faultLabel,
      stat.handle,
      stat.tipC.toFixed(1),
      stat.targetC,
      stat.actC,
      stat.dutyPct,
      stat.ticks,
      railV,
      curA,
      pkA,
      watts,
      stat.stand,
      stat.sleep,
    ].join(',');
  }

  return {
    parseKeyValues,
    parseInfoLine,
    parseStatLine,
    formatCsvHeader,
    formatCsvRow,
  };
});

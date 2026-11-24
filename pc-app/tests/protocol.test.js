const test = require('node:test');
const assert = require('node:assert/strict');

const {
  parseKeyValues,
  parseInfoLine,
  parseStatLine,
  formatCsvHeader,
  formatCsvRow,
} = require('../src/protocol');

test('parseKeyValues extracts tokens', () => {
  const kv = parseKeyValues('fw=v0.2.7-phase2 mcu=AT32F415RCT7 rail_mv=22928');
  assert.equal(kv.fw, 'v0.2.7-phase2');
  assert.equal(kv.mcu, 'AT32F415RCT7');
  assert.equal(kv.rail_mv, '22928');
});

test('parseInfoLine parses hardware-captured INFO response', () => {
  const line = 'INFO fw=v0.2.0-phase2 mcu=AT32F415RCT7 clk=144MHz safe_mode=0 handle=F245 state=SLEEP rail_mv=22928 uptime_ms=286065';
  const info = parseInfoLine(line);
  assert.ok(info);
  assert.equal(info.fw, 'v0.2.0-phase2');
  assert.equal(info.mcu, 'AT32F415RCT7');
  assert.equal(info.clk, '144MHz');
  assert.equal(info.safeMode, 0);
  assert.equal(info.handle, 'F245');
  assert.equal(info.state, 'SLEEP');
  assert.equal(info.railMv, 22928);
  assert.equal(info.uptimeMs, 286065);

  assert.equal(parseInfoLine('STAT ms=10'), null);
  assert.equal(parseInfoLine('OK SAVE'), null);
});

test('parseStatLine parses real SLEEP line and clamps negative current watts', () => {
  const line = 'STAT ms=286065 state=SLEEP fault=0x00(NONE) handle=F245 tip_c=155.8 target_c=300 act_c=150 duty=0 ticks=0 rail_mv=22928 cur_ma=-15 cur_pk=7500 zero_mv=1643 tip_adc=1148 cur_adc=2038 aux_adc=1777 stand=1 sleep=1 keys=0x00 pb5=0 pb6=1 bl=100 led=0x00 heater=1';
  const st = parseStatLine(line);
  assert.ok(st);
  assert.equal(st.ms, 286065);
  assert.equal(st.state, 'SLEEP');
  assert.equal(st.faultHex, '0x00');
  assert.equal(st.faultName, 'NONE');
  assert.equal(st.handle, 'F245');
  assert.equal(st.tipC, 155.8);
  assert.equal(st.targetC, 300);
  assert.equal(st.actC, 150);
  assert.equal(st.dutyPct, 0);
  assert.equal(st.ticks, 0);
  assert.equal(st.railMv, 22928);
  assert.equal(st.curMa, -15);
  assert.equal(st.curPkMa, 7500);
  assert.equal(st.stand, 1);
  assert.equal(st.sleep, 1);
  assert.equal(st.bl, 100);
  assert.equal(st.heaterOn, 1);
  assert.equal(st.cal, null);
  assert.equal(st.presets, null);
  assert.equal(st.watts, 0);
});

test('parseStatLine parses 3-point cal and presets lists when present', () => {
  const line = 'STAT ms=2500 state=HEAT fault=0x00(NONE) handle=F245 tip_c=250.2 target_c=250 act_c=250 duty=20 ticks=18 rail_mv=23100 cur_ma=2100 cur_pk=6900 zero_mv=1643 stand=0 sleep=1 bl=80 heater=1 cal=8,-5,12 presets=210,320,380';
  const st = parseStatLine(line);
  assert.ok(st);
  assert.deepEqual(st.cal, [8, -5, 12]);
  assert.deepEqual(st.presets, [210, 320, 380]);
});

test('parseStatLine computes active watts and fault details', () => {
  const line = 'STAT ms=5000 state=FAULT fault=0x11(NO_24V) handle=F210 tip_c=315.4 target_c=320 act_c=320 duty=71 ticks=64 rail_mv=22920 cur_ma=6450 cur_pk=7820 zero_mv=1642 tip_adc=2360 cur_adc=2560 aux_adc=1778 stand=0 sleep=1 keys=0x04 pb5=1 pb6=0 bl=80 led=0x3F heater=1';
  const st = parseStatLine(line);
  assert.ok(st);
  assert.equal(st.faultHex, '0x11');
  assert.equal(st.faultName, 'NO_24V');
  assert.equal(st.handle, 'F210');
  assert.ok(Math.abs(st.watts - 147.834) < 0.01);
});

test('formatCsvHeader and formatCsvRow output clean CSV', () => {
  assert.equal(
    formatCsvHeader(),
    'timestamp,ms,state,fault,handle,tip_c,target_c,act_c,duty_pct,ticks,rail_v,cur_a,cur_pk_a,watts,stand,sleep'
  );
  const st = parseStatLine('STAT ms=1200 state=HEAT fault=0x00(NONE) handle=F245 tip_c=300.0 target_c=300 act_c=300 duty=45 ticks=40 rail_mv=23000 cur_ma=5000 cur_pk=7500 stand=0 sleep=1');
  const row = formatCsvRow(st, '2026-11-23T17:00:00.000Z');
  assert.equal(
    row,
    '2026-11-23T17:00:00.000Z,1200,HEAT,0x00(NONE),F245,300.0,300,300,45,40,23.000,5.000,7.500,115.0,0,1'
  );
});

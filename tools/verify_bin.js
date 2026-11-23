#!/usr/bin/env node
/**
 * Binary & disassembly verifier for OpenDWS-200 AT32F415RCT7 firmware images.
 * Verifies Flash bounds, AT32F415 vector table, early PA2 heater lockout, and absence of FPU code.
 */

'use strict';

const fs = require('fs');

const APP_BASE = 0x08006000;
const MAX_APP_BYTES = 0x00039800; // 230 KB (0x08006000..0x0803F7FF)
const EXPECTED_SP = 0x20008000;
const VECTOR_COUNT = 92;
const RESERVED_IRQS = new Set([43, 44, 45, 46, 47, 48, 51, 54, 55, 60, 61, 62, 63, 64, 65, 66, 68, 69, 72, 73, 74]);

function fail(msg) {
  console.error(`VERIFY FAILED: ${msg}`);
  process.exit(1);
}

function isThumbFlashAddr(val, size) {
  return (val & 1) === 1 && val >= APP_BASE + VECTOR_COUNT * 4 && val < APP_BASE + size;
}

function main() {
  const args = process.argv.slice(2);
  const binPath = args[0];
  let disasmPath = null;
  for (let i = 1; i + 1 < args.length; i++) {
    if (args[i] === '--disasm') {
      disasmPath = args[i + 1];
    }
  }

  if (!binPath) {
    fail('Usage: node tools/verify_bin.js <firmware.bin> [--disasm <disasm.txt>]');
  }
  if (!fs.existsSync(binPath)) {
    fail(`File not found: ${binPath}`);
  }

  const buf = fs.readFileSync(binPath);
  if (buf.length <= VECTOR_COUNT * 4) {
    fail(`Binary too small (${buf.length} bytes)`);
  }
  if (buf.length > MAX_APP_BYTES) {
    fail(`Binary exceeds 230 KB limit (${buf.length} > ${MAX_APP_BYTES}), collides with settings page 0x0803F800`);
  }

  const sp = buf.readUInt32LE(0);
  if (sp !== EXPECTED_SP) {
    fail(`Vector[0] Initial SP mismatch: expected 0x${EXPECTED_SP.toString(16)}, got 0x${sp.toString(16)}`);
  }

  const reset = buf.readUInt32LE(4);
  if (!isThumbFlashAddr(reset, buf.length)) {
    fail(`Vector[1] Reset_Handler invalid: 0x${reset.toString(16)}`);
  }

  const nmi = buf.readUInt32LE(8);
  const hardFault = buf.readUInt32LE(12);
  const sysTick = buf.readUInt32LE(15 * 4);
  const exint9_5 = buf.readUInt32LE(39 * 4);
  const tmr1Ovf = buf.readUInt32LE(41 * 4);
  const tmr2 = buf.readUInt32LE(44 * 4);
  const otgfs1 = buf.readUInt32LE(83 * 4);
  const defaultIrq = buf.readUInt32LE(16 * 4);

  for (const [name, vec] of [
    ['NMI_Handler', nmi],
    ['HardFault_Handler', hardFault],
    ['SysTick_Handler', sysTick],
    ['EXINT9_5_IRQHandler', exint9_5],
    ['TMR1_OVF_TMR10_IRQHandler', tmr1Ovf],
    ['TMR2_IRQHandler', tmr2],
    ['OTGFS1_IRQHandler', otgfs1]
  ]) {
    if (!isThumbFlashAddr(vec, buf.length)) {
      fail(`${name} vector invalid: 0x${vec.toString(16)}`);
    }
  }

  if (sysTick === defaultIrq || exint9_5 === defaultIrq || tmr1Ovf === defaultIrq || tmr2 === defaultIrq || otgfs1 === defaultIrq) {
    fail('Critical IRQ handler points to Default_Handler');
  }

  for (const irq of RESERVED_IRQS) {
    const val = buf.readUInt32LE((16 + irq) * 4);
    if (val !== 0) {
      fail(`Reserved AT32F415 IRQ ${irq} expected 0, got 0x${val.toString(16)}`);
    }
  }

  // Ensure SCB->CPACR (0xE000ED88) is never referenced and GPIOA_CLR (0x40010814) is present
  let foundGpioaClr = false;
  for (let off = VECTOR_COUNT * 4; off + 4 <= buf.length; off += 4) {
    const word = buf.readUInt32LE(off);
    if (word === 0xE000ED88) {
      fail(`Found forbidden SCB->CPACR (0xE000ED88) reference at offset 0x${off.toString(16)}`);
    }
    if (word === 0x40010814) {
      foundGpioaClr = true;
    }
  }
  if (!foundGpioaClr) {
    fail('Missing GPIOA_CLR (0x40010814) reference for PA2 heater lockout');
  }

  if (disasmPath) {
    if (!fs.existsSync(disasmPath)) {
      fail(`Disassembly file not found: ${disasmPath}`);
    }
    const disasm = fs.readFileSync(disasmPath, 'utf8');
    const vfpLine = disasm
      .split(/\r?\n/)
      .find((line) => /^\s*[0-9a-f]+:\s+[0-9a-f ]+\s+(v[a-z]{2,}|fmrx|fmxr)\b/i.test(line));
    if (vfpLine) {
      fail(`Found VFP/FPU instruction in disassembly: ${vfpLine.trim()}`);
    }
  }

  console.log(
    `VERIFY OK: ${binPath} (${buf.length} bytes, SP=0x${sp.toString(16)}, Reset=0x${reset.toString(16)}, ` +
    `SysTick=0x${sysTick.toString(16)}, TMR1=0x${tmr1Ovf.toString(16)}, TMR2=0x${tmr2.toString(16)}, OTGFS1=0x${otgfs1.toString(16)})`
  );
}

main();

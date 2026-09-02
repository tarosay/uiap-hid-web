// ============================================================
//  URB コンパイラ（Ruby AST → URB1 バイトコード）
//
//  もともと uiapruby-ee.html の中にあったものを、そのままここへ移した。
//  分けた理由は URB Block Lab（uiapruby-block.html）が同じコンパイラを
//  使うため。両方に同じコードを置くと、片方だけ古くなる。
//
//  ⚠ このファイルは DOM を触らない。画面の更新は呼び出し側の仕事。
//     コンポーネント構成は comps オブジェクトで渡す（getComps() の戻り値）。
// ============================================================

function lineFromOffset(source, offset) {
  if (offset === undefined || offset === null) return '?';
  let line = 1;
  for (let i = 0; i < Math.min(offset, source.length); i++) {
    if (source.charCodeAt(i) === 10) line++;
  }
  return line;
}

// Np: LED 数。割り込みを止める時間が個数に比例するので、固定 64 にせず選ばせる。
const NEO_LED_CHOICES = [8, 12, 16, 24, 32, 64];
function neoMaxLeds(comps) {
  const n = comps && comps.leds;
  return NEO_LED_CHOICES.includes(n) ? n : 64;
}

// ============================================================
//  命令サイズ計算
// ============================================================
function instrSize(ins) {
  switch (ins.op) {
    case 'END':         return 1;
    case 'WAIT_MS':     return 3;
    case 'WAIT_MS_REG': return 4;
    case 'GPIO_MODE':   return 3;
    case 'GPIO_WRITE':  return 3;
    case 'GPIO_READ':   return 3;
    case 'GPIO_TOGGLE': return 2;
    case 'TONE_FREQ':    return 4;
    case 'JMP':         return 3;
    case 'JZ':          return 4;
    case 'JNZ':         return 4;
    case 'LOAD_Q16':    return 6;
    case 'ADD_Q16': case 'SUB_Q16': case 'MUL_Q16': case 'DIV_Q16': return 3;
    case 'CMP_LT_Q16': case 'CMP_GT_Q16': case 'CMP_EQ_Q16': return 4;
    case 'LOAD_BOOL':   return 3;
    case 'PRINT_STR':   return 3 + (ins.str ? ins.str.length : 0);
    case 'HALT':        return 2;
    case 'ADC_READ':        return 3;
    case 'ULTRASONIC_READ': return 4;
    case 'WARN_REG':        return 2;
    case 'PRINT_REG':       return 3;
    case 'RAND':            return 4;
    case 'SRAND':           return 3;
    case 'EVERY_MS':        return 4;
    case 'TIMER_RESET':     return 2;
    case 'TIMER_MS': case 'TIMER_US': return 3;
    case 'PWM_DUTY':        return 3;
    case 'PWM_DUTY_REG':    return 3;
    case 'PWM_BASE_FREQ':   return 4;
    case 'VAR_LOAD':        return 3;
    case 'VAR_STORE':       return 3;
    case 'VAR_LOAD_IDX':    return 4;
    case 'VAR_STORE_IDX':   return 4;
    case 'TO_S':            return 3;
    case 'VAR_STR_SET':     return 3 + ins.str.length;
    case 'VAR_STR_COPY':    return 3;
    case 'VAR_STR_CAT':     return 3;
    case 'VAR_STR_CAT_LIT': return 3 + ins.str.length;
    case 'VAR_PRINT':       return 3;
    case 'VAR_STR_CMP':     return 4 + ins.str.length;
    case 'VAR_STR_CMP_V':   return 4;
    case 'I2C_MASTER_INIT': return 1;
    case 'SERIAL_BEGIN':      return 5;
    case 'SERIAL_PRINT':      return 3 + new TextEncoder().encode(ins.text ?? '').length;
    case 'SERIAL_WRITE':      return 2;
    case 'SERIAL_WRITE_REG':  return 2;
    case 'SERIAL_AVAILABLE':  return 2;
    case 'SERIAL_READ':       return 2;
    case 'SERIAL_PRINT_REG':  return 3;
    case 'SERIAL_PRINT_VAR':  return 3;
    case 'SERIAL_READ_LINE':  return 5;
    case 'NEO_BEGIN':         return 2;
    case 'NEO_SHOW':          return 1;
    case 'NEO_CLEAR':         return 1;
    case 'NEO_BRIGHTNESS':    return 2;
    case 'NEO_SET_RGB':       return 5;
    case 'NEO_SET_HSV':       return 5;
    case 'NEO_AUTO':          return 2;
    case 'NEO_FILL':          return 6;
    case 'NEO_RAINBOW':       return 2;
    case 'NEO_SHIFT':         return 2;
    case 'NEO_SHIFT_REG':     return 2;
    case 'NEO_ORDER':         return 2;
    case 'NEO_DIM':           return 2;
    case 'I2C_MASTER_GET':  return 4;
    case 'I2C_MASTER_SET':  return 4;
    default:            return 1;
  }
}

function instrOffset(instructions, idx) {
  let off = 0;
  for (let i = 0; i < idx; i++) off += instrSize(instructions[i]);
  return off;
}

// ============================================================
//  コンパイラ
// ============================================================
class Compiler {
  constructor(source, comps = {}) {
    this.source = source;
    this.sourceBytes = new TextEncoder().encode(source);  // Prism の offset は UTF-8 バイト単位
    this.comps  = comps;
    this.vars  = {};
    this.defs  = {};
    this.regs  = {};
    this.eeVars = {};  // EEPROM 変数: name → { idx, type, count, name(スロット名用・$なし) }
    this.instructions = [];
    this.errors = [];
    this.loopStack = [];
    this.returnPatchStack = [];
    this.lambdas = {};       // ラムダ式: name → { params: [仮引数名], body: 式ノード }
    this.lambdaArgs = null;  // インライン展開中の 仮引数名 → 実引数ノード（変数束縛）
    this.lambdaDepth = 0;    // 入れ子展開の深さ（再帰防止）
    this.tmSlots = 0;        // Tm: 起点の割り当て数（every_ms の記述箇所と Timer で共有・最大 8）
  }

  // Tm: 起点を 1 つ確保して番号を返す。every_ms は「書いた場所ごと」、Timer は 1 オブジェクトごと。
  allocTmSlot(node) {
    if (this.tmSlots >= 8) {
      this.error(node, '絶対周期と Timer は合わせて 8 個までです（every_ms を書いた箇所と Timer.new の数の合計）');
      return 0;
    }
    return this.tmSlots++;
  }

  get currentOffset() { return this.instructions.reduce((s,i) => s + instrSize(i), 0); }
  emit(instr) { this.instructions.push(instr); return this.instructions.length - 1; }
  patchJump(idx) {
    const instr = this.instructions[idx];
    const instrEnd = instrOffset(this.instructions, idx) + instrSize(instr);
    instr.relOffset = this.currentOffset - instrEnd;
  }
  error(node, msg) {
    const line = node?.location?.startOffset !== undefined
      ? lineFromOffset(this.source, node.location.startOffset) : '?';
    this.errors.push(`Line ${line}: ${msg}`);
  }
  unsupported(node) { this.error(node, `unsupported Ruby syntax "${node.constructor.name}"`); }

  compile(programNode) {
    this.visitStatements(programNode.statements);
    if (this.errors.length === 0) this.emit({ op: 'END' });
  }

  visitStatements(stmtsNode) {
    if (!stmtsNode) return;
    for (const node of stmtsNode.body) {
      this.visitNode(node);
      if (this.errors.length > 5) { this.errors.push('(エラーが多いため中断)'); break; }
    }
  }

  visitNode(node) {
    if (!node) return;
    switch (node.constructor.name) {
      case 'LocalVariableWriteNode': return this.visitAssign(node);
      case 'GlobalVariableWriteNode': return this.visitAssign(node);  // $var → EEPROM 変数（永続）
      case 'CallNode':               return this.visitCall(node);
      case 'IfNode':                 return this.visitIf(node);
      case 'DefNode':                return this.visitDef(node);
      case 'ReturnNode':             return this.visitReturn(node);
      case 'ForNode':                return this.visitFor(node);
      case 'UnlessNode':             return this.visitUnless(node);
      case 'BreakNode':              return this.visitBreak(node);
      case 'NextNode':               return this.visitNext(node);
      case 'WhileNode':              return this.visitWhile(node);
      case 'UntilNode':              return this.visitUntil(node);
      case 'CaseNode':               return this.visitCase(node);
      case 'MultiWriteNode':         return this.visitMultiWrite(node);
      default:                       return this.unsupported(node);
    }
  }

  visitAssign(node) {
    const name = node.name, val = node.value;
    if (val.constructor.name === 'LambdaNode') return this.handleLambdaAssign(name, val, node);
    if (this.lambdas[name]) { this.error(node, `"${name}" はラムダ式として定義済みです。別の変数名を使ってください`); return; }
    if (val.constructor.name === 'CallNode') {
      const recv = val.receiver;
      if (recv?.constructor.name === 'ConstantReadNode') {
        if (recv.name === 'GPIO'       && val.name === 'new') return this.handleGpioNew(name, val);
        if (recv.name === 'Tone'       && val.name === 'new') return this.handleToneNew(name, val);
        if (recv.name === 'PWM'        && val.name === 'new') return this.handlePwmNew(name, val);
        if (recv.name === 'ADC'        && val.name === 'new') return this.handleAdcNew(name, val);
        if (recv.name === 'Ultrasonic' && val.name === 'new') return this.handleUltrasonicNew(name, val);
        if (recv.name === 'Serial'     && val.name === 'new') return this.handleSerialNew(name, val);
        if (recv.name === 'NeoPixel'   && val.name === 'new') return this.handleNeoPixelNew(name, val);
        if (recv.name === 'Timer'      && val.name === 'new') return this.handleTimerNew(name, val);
        if (recv.name === 'I2C' && (val.name === 'slave_get' || val.name === 'master_get')) return this.handleI2cGetAssign(name, val, node);
        if (recv.name === 'Array' && val.name === 'new') return this.handleArrayNew(name, val, node);
      }
      if (!val.receiver && val.name === 'rand') return this.handleRandAssign(name, val, node);
      // arr = [0.0] * 10
      if (val.name === '*' && val.receiver?.constructor.name === 'ArrayNode') {
        const n = this.evalInt(val.arguments_?.arguments_?.[0]);
        if (n === null) return;
        return this.declareArray(name, n, node);
      }
      // v = arr[i]
      if (val.name === '[]' && this.eeVars[this.numericVarName(val.receiver) ?? '']) return this.handleArrayReadAssign(name, val, node);
      // d = t.ms / u = t.us（Timer の経過時間をレジスタ変数へ。読んでも起点は動かない）
      if ((val.name === 'ms' || val.name === 'us') && val.receiver) {
        const rn = this.numericVarName(val.receiver);
        const vi = rn ? this.vars[rn] : null;
        if (vi?.kind === 'Timer') return this.handleTimerReadAssign(name, vi, val.name, node);
      }
      // v = sensor.read / v = sonar.read（センサ値をレジスタ変数へ）
      if (val.name === 'read' && val.receiver) {
        const rn = this.numericVarName(val.receiver);
        const vi = rn ? this.vars[rn] : null;
        if (vi?.kind === 'ADC' || vi?.kind === 'Ultrasonic') return this.handleSensorReadAssign(name, vi, node);
        if (vi?.kind === 'Serial') return this.handleSerialReadAssign(name, node);
      }
      // line = ser.gets(delim, timeout) → 文字変数へ 1 行読み込み
      if (val.name === 'gets' && val.receiver) {
        const rn = this.numericVarName(val.receiver);
        if (this.vars[rn]?.kind === 'Serial') return this.handleSerialGetsAssign(name, val, node);
      }
      // s = n.to_s（数値 → 文字列 EEPROM 変数）
      if (val.name === 'to_s' && val.receiver) return this.handleToSAssign(name, val, node);
    }
    // name = "リテラル"（文字 EEPROM 変数）/ ヒアドキュメントが 0/1 のみならビット配列（ドット絵等）
    // 注: <<~ ヒアドキュメントは Prism では InterpolatedStringNode（行ごとの StringNode パーツ）になる
    if (val.constructor.name === 'StringNode' || val.constructor.name === 'InterpolatedStringNode') {
      const bits = this.heredocBits(val);
      if (bits !== null) return this.handleBitArrayAssign(name, bits, node);
      return this.handleStrAssign(name, val, node);
    }
    // name = other（文字変数からのコピー）— isQ16Expr より先に判定（文字変数を数値扱いさせない）
    {
      const vn = this.numericVarName(val);
      if (vn && this.eeVars[vn] && (this.eeVars[vn].type & 0x7F) === 1) return this.handleStrCopy(name, vn, node);
    }
    if (val.constructor.name === 'IfNode' && val.statements && val.subsequent) return this.handleTernaryAssign(name, val, node);
    if (this.isQ16Expr(val)) return this.handleNumericAssign(name, val, node);
    this.error(node, `unsupported assignment to "${name}" (値の型: ${val.constructor.name})`);
  }

  // EE 版のピン排他（spec §2.3）。3/4=EEPROM バス、13/14=USB、17=RESET は常時。
  // Np 選択時は 8（SPI1 MOSI = NeoPixel DIN）、Se 選択時は 15/16（UART TX/RX）。
  checkPinUsable(pin, node) {
    if (pin === 3 || pin === 4) { this.error(node, `ピン ${pin} は EEPROM 専用（${pin === 3 ? "SDA" : "SCL"}）です。EE 版では GPIO に使えません`); return false; }
    if (pin === 13 || pin === 14) { this.error(node, `ピン ${pin} は USB 専用です`); return false; }
    if (pin === 17) { this.error(node, `ピン 17 は RESET 専用です`); return false; }
    if ((this.comps.Np || this.comps.Nr) && (pin === 8)) { this.error(node, `ピン 8 は NeoPixel の DIN 専用です（${this.comps.Nr ? 'Nr' : 'Np'} 選択中）。外すと GPIO として使えます`); return false; }
    if (this.comps.Se && (pin === 15 || pin === 16)) { this.error(node, `ピン ${pin} は UART ${pin === 15 ? "TX" : "RX"} 専用です（Se 選択中）。Se を外すと使えます`); return false; }
    return true;
  }

  handleGpioNew(varName, callNode) {
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 2) { this.error(callNode, 'GPIO.new requires 2 arguments'); return; }
    const pin = this.evalInt(args[0]), mode = this.evalGpioMode(args[1]);
    if (pin === null || mode === null) return;
    if (!this.checkPinUsable(pin, callNode)) return;
    this.vars[varName] = { kind: 'GPIO', pin };
    this.emit({ op: 'GPIO_MODE', pin, mode });
  }

  handleToneNew(varName, callNode) {
    if (!this.comps.Tn) { this.error(callNode, 'Tone には Tn コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 1) { this.error(callNode, 'Tone.new requires 1 argument'); return; }
    const pin = this.evalInt(args[0]);
    if (pin === null) return;
    if (![0, 2, 5, 6, 12].includes(pin)) { this.error(callNode, `Tone の対応ピンは 0, 2, 5, 6, 12 です（pin ${pin} は使用不可）`); return; }
    this.vars[varName] = { kind: 'Tone', pin };
    this.emit({ op: 'GPIO_MODE', pin, mode: 1 });
  }

  handlePwmNew(varName, callNode) {
    if (!this.comps.Pw) { this.error(callNode, 'PWM には Pw コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 1) { this.error(callNode, 'PWM.new requires 1 argument'); return; }
    const pin = this.evalInt(args[0]);
    if (pin === null) return;
    if (![0, 2, 5, 6, 12].includes(pin)) { this.error(callNode, `PWM の対応ピンは 0, 2, 5, 6, 12 です（pin ${pin} は使用不可）`); return; }
    this.vars[varName] = { kind: 'PWM', pin };
    this.emit({ op: 'GPIO_MODE', pin, mode: 1 });
  }

  handleAdcNew(varName, callNode) {
    if (!this.comps.Ad) { this.error(callNode, 'ADC には Ad コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 1) { this.error(callNode, 'ADC.new requires 1 argument'); return; }
    const pin = this.evalInt(args[0]);
    if (pin === null) return;
    if (![0, 1, 6, 12, 15, 16].includes(pin)) { this.error(callNode, `ADC の対応ピンは 0, 1, 6, 12, 15, 16 です（pin ${pin} は使用不可）`); return; }
    if (!this.checkPinUsable(pin, callNode)) return;
    this.vars[varName] = { kind: 'ADC', pin };
  }

  // t = Timer.new — 起点を 1 つ確保し、その場を起点にする（Timer.new と t.reset は同じ命令）
  handleTimerNew(varName, callNode) {
    if (!this.comps.Tm) { this.error(callNode, 'Timer には Tm コンポーネントが必要です。チェックしてください。'); return; }
    const slot = this.allocTmSlot(callNode);
    this.vars[varName] = { kind: 'Timer', slot };
    this.emit({ op: 'TIMER_RESET', slot });
  }

  // d = t.ms / u = t.us — 起点からの経過をレジスタへ。起点は動かさない（区間を区切るのは t.reset）
  handleTimerReadAssign(name, varInfo, unit, node) {
    if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(node, '数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[name] = n;
    }
    this.emit({ op: unit === 'us' ? 'TIMER_US' : 'TIMER_MS', slot: varInfo.slot, reg: this.regs[name] });
  }

  handleUltrasonicNew(varName, callNode) {
    if (!this.comps.Us) { this.error(callNode, 'Ultrasonic には Us コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 2) { this.error(callNode, 'Ultrasonic.new requires 2 arguments (trig, echo)'); return; }
    const trig = this.evalInt(args[0]), echo = this.evalInt(args[1]);
    if (trig === null || echo === null) return;
    if (!this.checkPinUsable(trig, callNode) || !this.checkPinUsable(echo, callNode)) return;
    this.vars[varName] = { kind: 'Ultrasonic', trig, echo };
    this.emit({ op: 'GPIO_MODE', pin: trig, mode: 1 });
    this.emit({ op: 'GPIO_MODE', pin: echo, mode: 0 });
  }

  // b = ser.read（1 バイトを数値変数へ。handleSensorReadAssign と同じレジスタ確保の流儀）
  handleSerialReadAssign(name, node) {
    if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(node, '数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[name] = n;
    }
    this.emit({ op: 'SERIAL_READ', reg: this.regs[name] });
  }

  // line = ser.gets(delim, timeout)（1 行を文字変数へ）
  handleSerialGetsAssign(name, callNode, node) {
    if (!this.comps.Ec) { this.error(node, `${name}: ser.gets（行を文字変数に入れる）には Ec コンポーネントが必要です。チェックしてください。`); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    const delim = args[0] ? this.evalInt(args[0]) : 10;   // 既定は LF
    const tmo   = args[1] ? this.evalInt(args[1]) : 1000; // 既定は 1 秒
    if (delim === null || tmo === null) return;
    if (tmo < 0 || tmo > 65535) { this.error(node, `ser.gets のタイムアウトは 0〜65535 ミリ秒です（現在 ${tmo}）`); return; }
    const evar = this.eeVar(node, name, name.startsWith('$') ? 0x81 : 0x01, 1);  // type 1 = 文字
    if (!evar) return;
    this.emit({ op: 'SERIAL_READ_LINE', varIdx: evar.idx, delim: delim & 0xFF, timeout: tmo });
  }

  handleSerialNew(varName, callNode) {
    if (!this.comps.Se) { this.error(callNode, 'Serial には Se コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 1) { this.error(callNode, 'Serial.new はボーレートを 1 つ取ります（例: Serial.new(9600)）'); return; }
    const baud = this.evalInt(args[0]);
    if (baud === null) return;
    if (baud < 300 || baud > 1000000) { this.error(callNode, `ボーレートが範囲外です: ${baud}`); return; }
    if (this.comps.Ad) {
      this.warnings = this.warnings || [];
      this.warnings.push('Se を使うとピン 15/16 が UART になり、ADC では使えなくなります（残るのは 0, 1, 6, 12）');
    }
    this.vars[varName] = { kind: 'Serial' };
    this.emit({ op: 'SERIAL_BEGIN', baud });
  }

  handleNeoPixelNew(varName, callNode) {
    if (!this.comps.Np && !this.comps.Nr) { this.error(callNode, 'NeoPixel には Np（または Nr）コンポーネントが必要です。チェックしてください。'); return; }
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length < 1) { this.error(callNode, 'NeoPixel.new は LED の個数を 1 つ取ります（例: NeoPixel.new(12)）'); return; }
    const n = this.evalInt(args[0]);
    if (n === null) return;
    const max = (typeof neoMaxLeds === 'function') ? neoMaxLeds(this.comps) : 64;
    if (n < 1 || n > max) {
      this.error(callNode, `LED の個数は 1〜${max} です（現在の設定: ${max} 個）。コンポーネント選択の「NeoPixel の LED 数」を変えてください`);
      return;
    }
    this.vars[varName] = { kind: 'NeoPixel', count: n };
    this.emit({ op: 'NEO_BEGIN', count: n });
  }

  handleRandAssign(varName, callNode, assignNode) {
    if (!this.comps.Rn) { this.error(assignNode, 'rand には Rn コンポーネントが必要です。チェックしてください。'); return; }
    if (!(varName in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(assignNode, 'Q16.8: 数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[varName] = n;
    }
    const dstReg = this.regs[varName];
    const args = callNode.arguments_?.arguments_ ?? [];
    if (args.length === 0) { this.emit({ op: 'RAND', min: 0, max: 0, reg: dstReg }); return; }
    const arg = args[0];
    if (arg.constructor.name === 'IntegerNode') {
      this.emit({ op: 'RAND', min: 0, max: Number(arg.value), reg: dstReg }); return;
    }
    if (arg.constructor.name === 'RangeNode') {
      const a = this.evalInt(arg.left), b = this.evalInt(arg.right);
      if (a === null || b === null) return;
      this.emit({ op: 'RAND', min: a, max: b + 1, reg: dstReg }); return;
    }
    this.error(callNode, 'rand() の引数は整数リテラルまたは整数範囲 (a..b) のみ対応');
  }

  // v = sensor.read（ADC: 0.00〜0.99）/ v = sonar.read（距離cm、整数部あり）
  handleSensorReadAssign(name, varInfo, node) {
    if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(node, '数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[name] = n;
    }
    const dstReg = this.regs[name];
    if (varInfo.kind === 'ADC') this.emit({ op: 'ADC_READ', pin: varInfo.pin, reg: dstReg });
    else this.emit({ op: 'ULTRASONIC_READ', trig: varInfo.trig, echo: varInfo.echo, reg: dstReg });
  }

  evalGpioMode(node) {
    if (node.constructor.name === 'ConstantPathNode') {
      const p = node.parent?.name, c = node.name;
      if (p === 'GPIO') {
        if (c === 'OUT') return 1;
        if (c === 'IN')  return 0;
      }
      this.error(node, `unknown GPIO mode "${p}::${c}"`); return null;
    }
    if (node.constructor.name === 'CallNode' && node.name === '|') {
      const left = this.evalGpioMode(node.receiver);
      const rarg = node.arguments_?.arguments_?.[0];
      if (left === 0 && rarg?.constructor.name === 'ConstantPathNode' && rarg.parent?.name === 'GPIO') {
        if (rarg.name === 'PULL_UP')   return 2;
        if (rarg.name === 'PULL_DOWN') return 3;
      }
    }
    this.error(node, 'unsupported GPIO mode expression'); return null;
  }

  visitCall(node) {
    if (!node.receiver) return this.visitTopLevelCall(node);
    const recv = node.receiver;
    if (recv.constructor.name === 'LocalVariableReadNode') {
      if (node.name === '[]=' && this.eeVars[recv.name]) return this.handleArrayStore(node, this.eeVars[recv.name]);
      if (node.name === '<<' && this.eeVars[recv.name] && (this.eeVars[recv.name].type & 0x7F) === 1) return this.handleStrAppend(node, this.eeVars[recv.name]);
      const varInfo = this.vars[recv.name];
      if (!varInfo) { this.error(node, `unknown variable "${recv.name}"`); return; }
      return this.visitVarMethod(node, varInfo);
    }
    if (recv.constructor.name === 'GlobalVariableReadNode' && this.eeVars[recv.name]) {
      if (node.name === '[]=') return this.handleArrayStore(node, this.eeVars[recv.name]);
      if (node.name === '<<' && (this.eeVars[recv.name].type & 0x7F) === 1) return this.handleStrAppend(node, this.eeVars[recv.name]);
    }
    if (recv.constructor.name === 'IntegerNode' && node.name === 'times' && node.block)
      return this.handleTimes(node);
    if (recv.constructor.name === 'CallNode' && !recv.receiver && !(recv.arguments_?.arguments_?.length)) {
      const varInfo = this.vars[recv.name];
      if (varInfo) return this.visitVarMethod(node, varInfo);
    }
    if (node.name === 'each' && node.block) {
      if (recv.constructor.name === 'ParenthesesNode') {
        let inner = recv.body;
        if (inner?.constructor.name === 'StatementsNode') inner = inner.body?.[0];
        if (inner?.constructor.name === 'RangeNode') return this.handleEachRange(node, inner);
      }
      if (recv.constructor.name === 'RangeNode') return this.handleEachRange(node, recv);
      if (recv.constructor.name === 'ArrayNode') return this.handleEachArray(node, recv);
    }
    if (recv.constructor.name === 'ConstantReadNode' && recv.name === 'I2C') return this.handleI2cCall(node);
    this.error(node, `unsupported receiver type "${recv.constructor.name}"`);
  }

  // I2C 文（master_init / master_set — スレーブは決定 13 によりコンパイルエラー）
  handleI2cCall(node) {
    // 決定 13: EE 版は EEPROM を読むため I2C マスターであり続ける必要があり、スレーブになれない
    if (node.name && node.name.startsWith('slave_')) {
      this.error(node, `I2C.${node.name} は EE 版では使えません。EEPROM がバスを使い続けるため、デバイスをスレーブにできません（マスター機能 master_init / master_get / master_set は使えます）`);
      return;
    }
    const args = node.arguments_?.arguments_ ?? [];
    switch (node.name) {
      case 'master_init': this.emit({ op: 'I2C_MASTER_INIT' }); return;
      case 'master_set': {
        const addr = this.evalInt(args[0]), reg = this.evalInt(args[1]);
        if (addr === null || reg === null) return;
        const srcReg = this.loadNumericVar(args[2]);
        if (srcReg === undefined) { this.error(node, 'master_set: 第3引数は数値変数のみ対応'); return; }
        this.emit({ op: 'I2C_MASTER_SET', addr, reg, srcReg }); return;
      }
      default: this.error(node, `unsupported I2C method "${node.name}"`);
    }
  }

  // v = I2C.master_get(addr, reg) — 値は生の整数（Q16.8 ではない）
  handleI2cGetAssign(name, val, node) {
    // 決定 13: 代入形の I2C.slave_get もここを通るため、ガードは handleI2cCall と両方に必要
    if (val.name === 'slave_get') {
      this.error(node, 'I2C.slave_get は EE 版では使えません。EEPROM がバスを使い続けるため、デバイスをスレーブにできません（I2C.master_get は使えます）');
      return;
    }
    if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(node, '数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[name] = n;
    }
    const dstReg = this.regs[name];
    const args = val.arguments_?.arguments_ ?? [];
    const addr = this.evalInt(args[0]), reg = this.evalInt(args[1]);
    if (addr === null || reg === null) return;
    this.emit({ op: 'I2C_MASTER_GET', addr, reg, dstReg });
  }

  // s = n.to_s — Q16.8 レジスタ値 → 文字列変数（EEPROM）に "n.nn" 形式で書き込み
  handleToSAssign(name, val, node) {
    if (!this.comps.Ec) { this.error(node, `${name}: to_s（文字列 EEPROM 変数）には Ec コンポーネントが必要です。チェックしてください。`); return; }
    const srcReg = this.loadNumericVar(val.receiver);
    if (srcReg === undefined) { this.error(node, 'to_s: レシーバは数値変数のみ対応'); return; }
    const evar = this.eeVar(node, name, name.startsWith('$') ? 0x81 : 0x01, 1);  // type 1 = 文字
    if (!evar) return;
    this.emit({ op: 'TO_S', reg: srcReg, varIdx: evar.idx });
  }

  // テンポラリレジスタ確保（不足時はエラーを出して undefined）
  allocTempRegChecked(excludeRegs, node) {
    const used = new Set([...Object.values(this.regs), ...excludeRegs]);
    for (let r = 0; r < 4; r++) { if (!used.has(r)) return r; }
    this.error(node, '式が複雑すぎます（レジスタ不足）。式を分割してください');
    return undefined;
  }

  // 汎用数値式の評価: 結果を「呼び出し側が自由に壊せるレジスタ」に置いて reg 番号を返す。
  // 対応: 数値リテラル / 変数（レジスタ・EEPROM・配列要素）/ 括弧 / 二項演算 + - * /（Q1 必須）
  evalExprToReg(node, excludeRegs = []) {
    if (!node) return undefined;
    // 定数畳み込み
    const cv = this.evalQ168(node);
    if (cv !== null) {
      if (!this.comps.Q1) { this.error(node, '式の評価には Q1 コンポーネントが必要です。チェックしてください。'); return undefined; }
      const t = this.allocTempRegChecked(excludeRegs, node);
      if (t === undefined) return undefined;
      this.emit({ op: 'LOAD_Q16', reg: t, value: cv });
      return t;
    }
    const cn = node.constructor.name;
    if (cn === 'ParenthesesNode') {
      let inner = node.body;
      if (inner?.constructor.name === 'StatementsNode') inner = inner.body?.[0];
      return this.evalExprToReg(inner, excludeRegs);
    }
    // 変数・配列要素（loadNumericVar が EEPROM/配列は temp に積む。名前付きレジスタはコピーして返す）
    const vr = this.loadNumericVar(node, excludeRegs);
    if (vr !== undefined) {
      if (Object.values(this.regs).includes(vr)) {
        if (!this.comps.Q1) { this.error(node, '式の評価には Q1 コンポーネントが必要です。チェックしてください。'); return undefined; }
        const t = this.allocTempRegChecked([...excludeRegs, vr], node);
        if (t === undefined) return undefined;
        this.emit({ op: 'LOAD_Q16', reg: t, value: 0 });
        this.emit({ op: 'ADD_Q16', dst: t, src: vr });
        return t;
      }
      return vr;
    }
    // f.call(x) — ラムダ本体を数値式として展開（戻り値 = 本体の式の値）
    if (cn === 'CallNode' && node.name === 'call' && node.receiver) {
      const lam = this.lambdaOf(node.receiver);
      if (lam === null && node.receiver.constructor.name === 'LambdaNode') return undefined;  // 検証エラー済み
      if (lam) {
        const r = this.withLambdaEnv(lam, node.arguments_?.arguments_ ?? [], node, () => this.evalExprToReg(lam.body, excludeRegs));
        return r === null ? undefined : r;
      }
    }
    // 二項演算
    if (cn === 'CallNode' && node.receiver) {
      const OP = { '+': 'ADD_Q16', '-': 'SUB_Q16', '*': 'MUL_Q16', '/': 'DIV_Q16' }[node.name];
      if (OP) {
        if (!this.comps.Q1) { this.error(node, `演算 "${node.name}" には Q1 コンポーネントが必要です。チェックしてください。`); return undefined; }
        const lt = this.evalExprToReg(node.receiver, excludeRegs);
        if (lt === undefined) return undefined;
        const rt = this.evalExprToReg(node.arguments_?.arguments_?.[0], [...excludeRegs, lt]);
        if (rt === undefined) return undefined;
        this.emit({ op: OP, dst: lt, src: rt });
        return lt;
      }
    }
    this.error(node, `数値式として評価できません: "${cn}"`);
    return undefined;
  }

  // wait_ms / sleep の引数が「変数」または「変数×リテラル / リテラル×変数」なら WAIT_MS_REG を emit して true。
  // 定数式なら false を返し、従来の即値 WAIT_MS にフォールバックする
  emitWaitReg(arg, baseMul, node) {
    if (!arg) return false;
    if (this.evalFloatConst(arg) !== null) return false;  // 定数畳み込みできる → 即値で
    let varNode = arg, lit = 1;
    if (arg.constructor.name === 'CallNode' && arg.name === '*') {
      const rConst = this.evalFloatConst(arg.arguments_?.arguments_?.[0]);
      const lConst = this.evalFloatConst(arg.receiver);
      if (rConst !== null && lConst === null) { varNode = arg.receiver; lit = rConst; }
      else if (lConst !== null && rConst === null) { varNode = arg.arguments_.arguments_[0]; lit = lConst; }
      else return false;
    }
    const reg = this.loadNumericVar(varNode);
    if (reg === undefined) return false;
    let mul = Math.round(lit * baseMul);
    if (mul < 1) mul = 1;
    if (mul > 65535) { this.error(node, 'wait_ms/sleep: 乗数は最大 65535 です'); return true; }
    this.emit({ op: 'WAIT_MS_REG', reg, mul });
    return true;
  }

  // 文字列リテラル → UTF-8 バイト列（最大32バイト）
  strBytes(node, ctx) {
    let s = this.stringNodeText(node);
    if (typeof s !== 'string') s = '';
    const bytes = Array.from(new TextEncoder().encode(s));
    if (bytes.length > 32) { this.error(ctx, `文字列リテラルは最大32バイトです（現在 ${bytes.length} バイト）`); return null; }
    return bytes;
  }

  // 文字列ノードの中身を取り出す（StringNode / InterpolatedStringNode のパーツ連結）。式埋め込みは null
  stringNodeText(node) {
    if (node.constructor.name === 'StringNode') {
      let s = node.unescaped;
      if (typeof s !== 'string' || s.length === 0) {
        if (node.contentLoc && this.sourceBytes) {
          s = new TextDecoder().decode(this.sourceBytes.slice(node.contentLoc.startOffset, node.contentLoc.startOffset + node.contentLoc.length));
          // 生ソースへのフォールバックではエスケープ列が未処理 — シングルクォート以外は手動で展開
          // （Prism の unescaped が空を返すため。'...' は Ruby 仕様でエスケープしない）
          const op = node.openingLoc && this.sourceBytes
            ? new TextDecoder().decode(this.sourceBytes.slice(node.openingLoc.startOffset, node.openingLoc.startOffset + node.openingLoc.length))
            : '';
          if (op !== "'") {
            s = s.replace(/\\(.)/g, (m, c) => ({ n: '\n', t: '\t', r: '\r', '0': '\0', 'e': '\x1b', 's': ' ', '\\': '\\', '"': '"', "'": "'" }[c] ?? c));
          }
        }
      }
      return (typeof s === 'string') ? s : null;
    }
    if (node.constructor.name === 'InterpolatedStringNode') {
      let s = '';
      for (const part of (node.parts ?? [])) {
        if (part.constructor.name !== 'StringNode') return null;  // #{} 式埋め込みは非対応
        const t = this.stringNodeText(part);
        if (t === null) return null;
        s += t;
      }
      return s;
    }
    return null;
  }

  // ヒアドキュメント（<<~XXX）の中身が 0/1 と空白のみならビット配列として返す。そうでなければ null
  heredocBits(node) {
    if (!node.openingLoc || !this.sourceBytes) return null;
    const op = new TextDecoder().decode(this.sourceBytes.slice(node.openingLoc.startOffset, node.openingLoc.startOffset + node.openingLoc.length));
    if (!op.startsWith('<<')) return null;
    const s = this.stringNodeText(node);
    if (typeof s !== 'string') return null;
    if (!/^[01\s]+$/.test(s) || !/[01]/.test(s)) return null;
    return [...s].filter(c => c === '0' || c === '1').map(c => (c === '1' ? 1 : 0));
  }

  // ビット配列の初期化（Method A）: 数値配列を宣言し、1 のビットだけ VAR_STORE_IDX で書き込む
  // （0 はファームウェアのスロット 0 クリアで埋まっているため省略 → バイトコード削減）
  handleBitArrayAssign(name, bits, node) {
    if (!this.comps.Ec && !this.comps.Ev) { this.error(node, `${name}: ビット配列（EEPROM 変数）には Ev（または Ec）コンポーネントが必要です。チェックしてください。`); return; }
    if (!this.comps.Q1) { this.error(node, `${name}: ビット配列の初期化には Q1 コンポーネントが必要です。チェックしてください。`); return; }
    const evar = this.eeVar(node, name, name.startsWith('$') ? 0x82 : 0x02, bits.length);
    if (!evar) return;
    const idxReg = this.allocTempRegChecked([], node);
    if (idxReg === undefined) return;
    const valReg = this.allocTempRegChecked([idxReg], node);
    if (valReg === undefined) return;
    this.emit({ op: 'LOAD_Q16', reg: valReg, value: 1 << 8 });  // 値 1 は1回だけロード
    for (let i = 0; i < bits.length; i++) {
      if (!bits[i]) continue;
      this.emit({ op: 'LOAD_Q16', reg: idxReg, value: i << 8 });
      this.emit({ op: 'VAR_STORE_IDX', varIdx: evar.idx, idxReg, reg: valReg });
    }
  }

  requireSvStr(node, name) {
    if (!this.comps.Ec) { this.error(node, `${name}: 文字変数には Ec コンポーネントが必要です。チェックしてください。`); return false; }
    return true;
  }

  // name = "リテラル"
  handleStrAssign(name, val, node) {
    if (!this.requireSvStr(node, name)) return;
    const bytes = this.strBytes(val, node);
    if (bytes === null) return;
    const evar = this.eeVar(node, name, name.startsWith('$') ? 0x81 : 0x01, 1);
    if (!evar) return;
    this.emit({ op: 'VAR_STR_SET', varIdx: evar.idx, str: bytes });
  }

  // name = other（文字変数コピー）
  handleStrCopy(name, srcName, node) {
    if (!this.requireSvStr(node, name)) return;
    const evar = this.eeVar(node, name, name.startsWith('$') ? 0x81 : 0x01, 1);
    if (!evar) return;
    this.emit({ op: 'VAR_STR_COPY', dstIdx: evar.idx, srcIdx: this.eeVars[srcName].idx });
  }

  // msg << s / msg << "リテラル"
  handleStrAppend(node, evar) {
    const arg = node.arguments_?.arguments_?.[0];
    if (!arg) { this.error(node, '<<: 引数が必要です'); return; }
    if (arg.constructor.name === 'StringNode') {
      const bytes = this.strBytes(arg, node);
      if (bytes === null) return;
      this.emit({ op: 'VAR_STR_CAT_LIT', dstIdx: evar.idx, str: bytes });
      return;
    }
    const sn = this.numericVarName(arg);
    if (sn && this.eeVars[sn] && (this.eeVars[sn].type & 0x7F) === 1) {
      this.emit({ op: 'VAR_STR_CAT', dstIdx: evar.idx, srcIdx: this.eeVars[sn].idx });
      return;
    }
    this.error(node, '<<: 文字列リテラルまたは文字変数のみ連結できます');
  }

  // name == "リテラル" / name == other（!= は JNZ 変換）
  evalStrCond(node, lsv) {
    const jump = node.name === '==' ? 'JZ' : 'JNZ';
    const arg = node.arguments_?.arguments_?.[0];
    const out = this.allocTempReg([]);
    if (arg?.constructor.name === 'StringNode') {
      const bytes = this.strBytes(arg, node);
      if (bytes === null) return null;
      this.emit({ op: 'VAR_STR_CMP', varIdx: lsv.idx, out, str: bytes });
      return { jumpOp: jump, reg: out };
    }
    const rn = this.numericVarName(arg);
    if (rn && this.eeVars[rn] && (this.eeVars[rn].type & 0x7F) === 1) {
      this.emit({ op: 'VAR_STR_CMP_V', aIdx: lsv.idx, bIdx: this.eeVars[rn].idx, out });
      return { jumpOp: jump, reg: out };
    }
    this.error(node, '文字変数の比較相手は文字列リテラルまたは文字変数のみ対応'); return null;
  }

  // arr = Array.new(n) / Array.new(n, x) — 要素数はコンパイル時確定。初期値は無視（全要素0）
  handleArrayNew(name, val, node) {
    const n = this.evalInt(val.arguments_?.arguments_?.[0]);
    if (n === null) return;
    this.declareArray(name, n, node);
  }

  declareArray(name, count, node) {
    if (!this.comps.Ec && !this.comps.Ev) { this.error(node, `${name}: 配列（EEPROM 変数）には Ev（または Ec）コンポーネントが必要です。チェックしてください。`); return; }
    if (!this.comps.Q1) { this.error(node, `${name}: 配列の要素アクセスには Q1 コンポーネントが必要です。チェックしてください。`); return; }
    if (count < 1 || count > 65535) { this.error(node, '配列の要素数は 1〜65535 で指定してください'); return; }
    const persist = name.startsWith('$');
    this.eeVar(node, name, persist ? 0x82 : 0x02, count);  // type 2 = 数値配列
  }

  // インデックスをレジスタに用意（リテラル / 変数 / 式 — 式は evalExprToReg で評価）
  loadIndexReg(idxNode, excludeRegs, node) {
    const ci = this.evalInt2(idxNode);
    if (ci !== null) {
      const t = this.allocTempReg(excludeRegs);
      this.emit({ op: 'LOAD_Q16', reg: t, value: ci << 8 });
      return t;
    }
    const r = this.loadNumericVar(idxNode, excludeRegs);
    if (r !== undefined) return r;
    // r * 24 + c のような添字式
    return this.evalExprToReg(idxNode, excludeRegs);
  }

  // evalInt のエラーを出さない版（リテラルでなければ null）
  evalInt2(node) {
    const f = this.evalFloatConst(node);
    return f !== null ? Math.round(f) : null;
  }

  // arr[i] = v（文）。v はリテラル / 変数 / 三項演算子 / 一般式に対応
  handleArrayStore(node, evar) {
    const args = node.arguments_?.arguments_ ?? [];
    if (args.length < 2) { this.error(node, '[]=: 引数が不足しています'); return; }
    const rhs = args[1];
    let valReg, idxReg;
    // dots[i] = cond ? 0 : 1 — 三項演算子。
    // レジスタ衝突を避けるため「条件 → 値 → 添字」の順で評価する
    // （条件評価のテンポラリは合流時点で死んでいるので、その後に valReg / idxReg を確保すれば安全）
    if (rhs.constructor.name === 'IfNode' && rhs.statements && rhs.subsequent) {
      const thenStmts = rhs.statements?.body ?? [];
      const elseStmts = rhs.subsequent?.statements?.body ?? [];
      if (thenStmts.length !== 1 || elseStmts.length !== 1) { this.error(node, '[]=: 三項演算子は単一の値のみ対応'); return; }
      const tv = this.evalQ168(thenStmts[0]), ev = this.evalQ168(elseStmts[0]);
      if (tv === null || ev === null) { this.error(node, '[]=: 三項演算子の値は数値リテラルのみ対応'); return; }
      const cond = this.evalCondition(rhs.predicate);
      if (!cond) return;
      const jumpToElse = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
      for (const j of (cond.extraBodyJumps ?? [])) this.patchJump(j);
      valReg = this.allocTempRegChecked([], node);
      if (valReg === undefined) return;
      this.emit({ op: 'LOAD_Q16', reg: valReg, value: tv });
      const jumpToEnd = this.emit({ op: 'JMP', relOffset: 0 });
      for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
      this.patchJump(jumpToElse);
      this.emit({ op: 'LOAD_Q16', reg: valReg, value: ev });
      this.patchJump(jumpToEnd);
      idxReg = this.loadIndexReg(args[0], [valReg], node);
      if (idxReg === undefined) return;
    } else {
      idxReg = this.loadIndexReg(args[0], [], node);
      if (idxReg === undefined) return;
      const cv = this.evalQ168(rhs);
      if (cv !== null) {
        valReg = this.allocTempReg([idxReg]);
        this.emit({ op: 'LOAD_Q16', reg: valReg, value: cv });
      } else {
        valReg = this.loadNumericVar(rhs, [idxReg]);
        if (valReg === undefined) valReg = this.evalExprToReg(rhs, [idxReg]);  // 一般式
        if (valReg === undefined) return;
      }
    }
    this.emit({ op: 'VAR_STORE_IDX', varIdx: evar.idx, idxReg, reg: valReg });
  }

  // v = arr[i]
  handleArrayReadAssign(name, val, node) {
    const evar = this.eeVars[this.numericVarName(val.receiver)];
    const idxReg = this.loadIndexReg(val.arguments_?.arguments_?.[0], [], node);
    if (idxReg === undefined) return;
    // 代入先（レジスタ優先 → EEPROM）
    let sdEntry = null;
    if (name.startsWith('$') || name in this.eeVars) {
      sdEntry = this.eeVar(node, name, name.startsWith('$') ? 0x80 : 0x00, 1);
      if (!sdEntry) return;
    } else if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n < 2) this.regs[name] = n;
      else { sdEntry = this.eeVar(node, name, 0x00, 1); if (!sdEntry) return; }
    }
    const dstReg = sdEntry ? this.allocTempReg([idxReg]) : this.regs[name];
    this.emit({ op: 'VAR_LOAD_IDX', varIdx: evar.idx, idxReg, reg: dstReg });
    if (sdEntry) this.emit({ op: 'VAR_STORE', varIdx: sdEntry.idx, reg: dstReg });
  }

  // b = cond ? x : y — x/y が 0/1 → LOAD_BOOL（生の値）、その他数値 → LOAD_Q16（Q1 必須）
  handleTernaryAssign(name, val, node) {
    const thenStmts = val.statements?.body ?? [];
    const elseStmts = val.subsequent?.statements?.body ?? [];
    if (thenStmts.length !== 1 || elseStmts.length !== 1) { this.error(node, '三項演算子の代入は単一の値のみ対応'); return; }
    const tv = this.evalFloatConst(thenStmts[0]);
    const ev = this.evalFloatConst(elseStmts[0]);
    if (tv === null || ev === null) { this.error(node, '三項演算子の代入は数値リテラルのみ対応'); return; }
    if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n >= 2) { this.error(node, '数値変数は最大 2 つです (R0/R1)'); return; }
      this.regs[name] = n;
    }
    const dstReg = this.regs[name];
    const isBool = (tv === 0 || tv === 1) && (ev === 0 || ev === 1);
    if (!isBool && !this.comps.Q1) { this.error(node, '三項代入（数値）: Q1 コンポーネントが必要です。チェックしてください。'); return; }
    const cond = this.evalCondition(val.predicate);
    if (!cond) return;
    const jumpToElse = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
    for (const j of (cond.extraBodyJumps ?? [])) this.patchJump(j);
    this.emit(isBool ? { op: 'LOAD_BOOL', reg: dstReg, value: tv } : { op: 'LOAD_Q16', reg: dstReg, value: Math.round(tv * 256) });
    const jumpToEnd = this.emit({ op: 'JMP', relOffset: 0 });
    for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
    this.patchJump(jumpToElse);
    this.emit(isBool ? { op: 'LOAD_BOOL', reg: dstReg, value: ev } : { op: 'LOAD_Q16', reg: dstReg, value: Math.round(ev * 256) });
    this.patchJump(jumpToEnd);
  }

  visitTopLevelCall(node) {
    const args = node.arguments_?.arguments_ ?? [], block = node.block;
    switch (node.name) {
      case 'loop':
        if (!block) { this.error(node, 'loop requires a do...end block'); return; }
        return this.handleLoop(block);
      case 'sleep': {
        // 変数 → WAIT_MS_REG（待ち時間 = レジスタ整数部 × mul ms。掛け算はファームウェアが行う）
        if (this.emitWaitReg(args[0], 1000, node)) return;
        const secs = this.evalFloat(args[0]);
        if (secs === null) return;
        this.emit({ op: 'WAIT_MS', ms: Math.max(0, Math.round(secs * 1000)) }); return;
      }
      case 'wait_ms': {
        if (this.emitWaitReg(args[0], 1, node)) return;
        const ms = this.evalInt(args[0]);
        if (ms === null) return;
        this.emit({ op: 'WAIT_MS', ms: Math.max(0, ms) }); return;
      }
      case 'print': return this.handlePrint(node, 0x00);
      case 'puts':  return this.handlePrint(node, 0x01);
      case 'p':     return this.handlePrint(node, 0x03);
      case 'putc':  return this.handlePutc(node);
      case 'warn':  return this.handleWarn(node);
      case 'raise': return this.handleRaise(node);
      case 'every_ms': {
        if (!this.comps.Tm) { this.error(node, 'every_ms には Tm コンポーネントが必要です。チェックしてください。'); return; }
        if (args.length !== 1) { this.error(node, 'every_ms は引数 1 個（周期 ms）が必要です'); return; }
        const ms = this.evalInt(args[0]);
        if (ms === null) return;
        if (ms < 1 || ms > 65535) { this.error(node, `every_ms の周期は 1〜65535 ms です（${ms} は範囲外）`); return; }
        const slot = this.allocTmSlot(node);
        // ブロックの形: 本体 → EVERY_MS → 先頭へ戻る（loop do の末尾に周期待ちを置くのと同じ）
        if (block) {
          const loopStart = this.currentOffset, breakPatches = [];
          this.loopStack.push({ startOffset: loopStart, breakPatches });
          this.visitStatements(block.body);
          this.loopStack.pop();
          this.emit({ op: 'EVERY_MS', slot, ms });
          const jmpIdx = this.emit({ op: 'JMP', relOffset: 0 });
          const jmpEnd = instrOffset(this.instructions, jmpIdx) + instrSize(this.instructions[jmpIdx]);
          this.instructions[jmpIdx].relOffset = loopStart - jmpEnd;
          for (const bIdx of breakPatches) this.patchJump(bIdx);
          return;
        }
        // 文の形: その場に 1 命令だけ置く（loop do の中に書く）
        this.emit({ op: 'EVERY_MS', slot, ms }); return;
      }
      case 'srand': {
        if (!this.comps.Rn) { this.error(node, 'srand には Rn コンポーネントが必要です。チェックしてください。'); return; }
        const seed = args.length > 0 ? this.evalInt(args[0]) : 0;
        if (seed === null) return;
        this.emit({ op: 'SRAND', seed: seed & 0xFFFF }); return;
      }
      default:
        if (this.defs[node.name]) return this.inlineDef(node.name, node);
        this.error(node, `unsupported method "${node.name}"`);
    }
  }

  visitVarMethod(node, varInfo) {
    const method = node.name, args = node.arguments_?.arguments_ ?? [];
    if (varInfo.kind === 'GPIO') {
      switch (method) {
        case 'write': {
          const cv = this.evalFloatConst(args[0]);
          if (cv !== null) { this.emit({ op: 'GPIO_WRITE', pin: varInfo.pin, value: cv ? 1 : 0 }); return; }
          // 変数: v != 0 → HIGH / v == 0 → LOW（JZ 分岐、新オペコード不要）
          const reg = this.loadNumericVar(args[0]);
          if (reg === undefined) { this.error(node, 'write: 引数は 0/1 リテラルまたは数値変数のみ対応'); return; }
          const jz = this.emit({ op: 'JZ', reg, relOffset: 0 });
          this.emit({ op: 'GPIO_WRITE', pin: varInfo.pin, value: 1 });
          const jmp = this.emit({ op: 'JMP', relOffset: 0 });
          this.patchJump(jz);
          this.emit({ op: 'GPIO_WRITE', pin: varInfo.pin, value: 0 });
          this.patchJump(jmp);
          return;
        }
        case 'on': case 'high': this.emit({ op: 'GPIO_WRITE', pin: varInfo.pin, value: 1 }); return;
        case 'off': case 'low': this.emit({ op: 'GPIO_WRITE', pin: varInfo.pin, value: 0 }); return;
        case 'toggle': this.emit({ op: 'GPIO_TOGGLE', pin: varInfo.pin }); return;
        case 'low?': case 'high?': case 'read':
          this.error(node, `"${method}" can only be used as an if condition`); return;
        default: this.error(node, `unsupported GPIO method "${method}"`);
      }
      return;
    }
    if (varInfo.kind === 'Timer') {
      if (method === 'reset') { this.emit({ op: 'TIMER_RESET', slot: varInfo.slot }); return; }
      if (method === 'ms' || method === 'us') {
        this.error(node, `t.${method} は値なので、変数に受けるか warn に渡してください（例: d = t.${method}）`); return;
      }
      this.error(node, `Timer に "${method}" はありません（reset / ms / us）`); return;
    }
    if (varInfo.kind === 'ADC') {
      if (node.name === 'read') this.error(node, '"sensor.read" は puts/print/p の引数として使ってください');
      else this.error(node, `unsupported ADC method "${node.name}"`);
      return;
    }
    if (varInfo.kind === 'Ultrasonic') {
      if (node.name === 'read') this.error(node, '"sonar.read" は puts/print/p の引数として使ってください');
      else this.error(node, `unsupported Ultrasonic method "${node.name}"`);
      return;
    }
    if (varInfo.kind === 'Serial') {
      switch (method) {
        case 'write': {
          if (!args[0]) { this.error(node, 'write は 1 引数です'); return; }
          // 変数を先に見る（evalInt は非リテラルでエラーを出してしまうため）
          const rn = this.numericVarName(args[0]);
          if (rn && rn in this.regs) { this.emit({ op: 'SERIAL_WRITE_REG', reg: this.regs[rn] }); return; }
          if (args[0].constructor.name === 'IntegerNode') {
            const imm = this.evalInt(args[0]);
            if (imm === null) return;
            this.emit({ op: 'SERIAL_WRITE', val: imm & 0xFF }); return;
          }
          this.error(node, 'ser.write の引数は 0〜255 の数値リテラルか、数値変数です'); return;
        }
        case 'print':
        case 'puts': {
          const flags = (method === 'puts') ? 1 : 0;
          if (!args[0]) { if (flags) this.emit({ op: 'SERIAL_PRINT', flags, text: '' }); return; }
          if (args[0].constructor.name === 'StringNode' || args[0].constructor.name === 'InterpolatedStringNode') {
            const s = this.stringNodeText(args[0]);
            if (s === null) { this.error(node, `ser.${method} は文字列リテラルのみです（式の埋め込みは不可）`); return; }
            this.emit({ op: 'SERIAL_PRINT', flags, text: s }); return;
          }
          const vn = this.numericVarName(args[0]);
          if (vn && this.eeVars && this.eeVars[vn]) {
            if (!this.comps.Ec) { this.error(node, `ser.${method} で変数を送るには Ec コンポーネントが必要です`); return; }
            this.emit({ op: 'SERIAL_PRINT_VAR', flags, varIdx: this.eeVars[vn].idx }); return;
          }
          if (vn && vn in this.regs) {
            if (!this.comps.Ec) { this.error(node, `ser.${method} で数値を送るには Ec コンポーネントが必要です`); return; }
            this.emit({ op: 'SERIAL_PRINT_REG', flags, reg: this.regs[vn] }); return;
          }
          this.error(node, `ser.${method} は文字列リテラルか変数を取ります`); return;
        }
        case 'read':
          this.error(node, '"ser.read" は代入か puts の引数として使ってください（例: b = ser.read）'); return;
        case 'available?':
        case 'available':
          this.error(node, '"ser.available?" は if の条件として使ってください'); return;
        case 'gets':
          this.error(node, '"ser.gets" は代入で使ってください（例: line = ser.gets(13, 1000)）'); return;
        default:
          this.error(node, `unsupported Serial method "${node.name}"`); return;
      }
    }
    if (varInfo.kind === 'NeoPixel') {
      const n0 = a => { const v = this.evalInt(a); return v === null ? null : v; };
      switch (method) {
        case 'show':   this.emit({ op: 'NEO_SHOW' }); return;
        case 'clear':  this.emit({ op: 'NEO_CLEAR' }); return;
        case 'brightness': { const v = n0(args[0]); if (v === null) return; this.emit({ op: 'NEO_BRIGHTNESS', val: Math.max(0, Math.min(255, v)) }); return; }
        case 'set': {
          if (args.length < 4) { this.error(node, 'set は (番号, 赤, 緑, 青) の 4 引数です'); return; }
          const i = n0(args[0]), r = n0(args[1]), g = n0(args[2]), b = n0(args[3]);
          if ([i,r,g,b].some(v => v === null)) return;
          if (i < 0 || i >= varInfo.count) { this.error(node, `LED の番号は 0〜${varInfo.count - 1} です`); return; }
          this.emit({ op: 'NEO_SET_RGB', idx: i, r: r & 0xFF, g: g & 0xFF, b: b & 0xFF }); return;
        }
        case 'hsv': {
          if (!this.comps.Nr) { this.error(node, 'np.hsv には Nr コンポーネントが必要です（Np には色の計算が入っていません）。チェックしてください。'); return; }
          if (args.length < 4) { this.error(node, 'hsv は (番号, 色, 鮮やかさ, 明るさ) の 4 引数です'); return; }
          const i = n0(args[0]), h = n0(args[1]), s = n0(args[2]), v = n0(args[3]);
          if ([i,h,s,v].some(x => x === null)) return;
          if (i < 0 || i >= varInfo.count) { this.error(node, `LED の番号は 0〜${varInfo.count - 1} です`); return; }
          this.emit({ op: 'NEO_SET_HSV', idx: i, h: h % 101, s: Math.min(100, s), v: Math.min(100, v) }); return;
        }
        case 'fill': {
          if (args.length < 3) { this.error(node, 'fill は (赤, 緑, 青) の 3 引数です'); return; }
          const r = n0(args[0]), g = n0(args[1]), b = n0(args[2]);
          if ([r,g,b].some(v => v === null)) return;
          this.emit({ op: 'NEO_FILL', start: 0, count: 0, r: r & 0xFF, g: g & 0xFF, b: b & 0xFF }); return;
        }
        case 'auto=': {
          // 決定 44: 自動表示の抑止。true / false リテラルのみ（他の NeoPixel 命令と同じ即値専用）
          const t = args[0]?.constructor.name;
          if (t !== 'TrueNode' && t !== 'FalseNode') {
            this.error(node, 'np.auto は true か false のどちらかです（変数は使えません）'); return;
          }
          this.emit({ op: 'NEO_AUTO', on: t === 'TrueNode' ? 1 : 0 }); return;
        }
        case 'auto': this.error(node, 'np.auto は代入で使ってください（例: np.auto = false）'); return;
        case 'rainbow': {
          if (!this.comps.Nr) { this.error(node, 'np.rainbow には Nr コンポーネントが必要です（Np には色の計算が入っていません）。チェックしてください。'); return; }
          const o = args[0] ? n0(args[0]) : 0; if (o === null) return;
          this.emit({ op: 'NEO_RAINBOW', offset: o % 100 }); return;
        }
        case 'shift': {
          // リテラルなら、ファームは 0..n-1 の歩数しか受けないのでここで畳む。
          // 個数はコンパイル時に確定しているので、実行時に % を持つ必要が無い。
          // evalInt はここでエラーを出してしまうので、静かに試せる方（duty と同じ）を使う
          const c = args[0] ? this.evalFloatConst(args[0]) : 1;
          if (c !== null) {
            const n = varInfo.count;
            const step = Math.round(c);
            this.emit({ op: 'NEO_SHIFT', step: ((step % n) + n) % n, raw: step }); return;
          }
          // 変数なら歩数が実行時にしか分からない。正規化はファーム側が行う。
          const reg = this.getVarReg(args[0]);
          if (reg !== undefined) {
            // Np 単独の構成は Flash に 1 命令ぶんの空きも無いので、この命令は Nr にしかない
            if (!this.comps.Nr) { this.error(node, 'shift(変数): Nr コンポーネントが必要です（Np には入っていません）。チェックしてください。'); return; }
            if (!this.comps.Q1) { this.error(node, 'shift(変数): Q1 コンポーネントが必要です。チェックしてください。'); return; }
            this.emit({ op: 'NEO_SHIFT_REG', reg }); return;
          }
          this.error(node, 'shift: 引数はリテラルまたは数値変数のみ対応'); return;
        }
        case 'seen': {
          // テープごとの色の並び。並びの名前（GRB など）は箱にも商品にも書いていないので、
          // 「赤が何に見えるか」「緑が何に見えるか」の 2 つで一意に決める。
          // 1 つでは足りない（6 通りが 3 組に分かれるだけ）。scratch3-uiapduino と同じ表。
          if (!this.comps.Nr) { this.error(node, 'np.seen には Nr コンポーネントが必要です（Np には入っていません）。チェックしてください。'); return; }
          if (args.length < 2) { this.error(node, "seen は ('赤に見える色', '緑に見える色') の 2 引数です（例: np.seen('B', 'G')）"); return; }
          const pick = a => {
            const t = this.stringNodeText(a);
            const k = typeof t === 'string' ? t.trim().toUpperCase() : null;
            return (k === 'R' || k === 'G' || k === 'B') ? k : null;
          };
          const sr = pick(args[0]), sg = pick(args[1]);
          if (!sr || !sg) { this.error(node, "seen: 'R' / 'G' / 'B' の文字リテラルで指定してください（変数は使えません）"); return; }
          if (sr === sg) { this.error(node, '赤と緑が同じ色に見えることはありません。別々の色を選んでください'); return; }
          // 「そのまま送ったときにこう見える」→ 送るときに使う元の色の並び
          const ORDER = { 'R,G': [0,1,2], 'B,G': [2,1,0], 'G,R': [1,0,2], 'R,B': [0,2,1], 'G,B': [1,2,0], 'B,R': [2,0,1] };
          const o = ORDER[`${sr},${sg}`];
          this.emit({ op: 'NEO_ORDER', order: o, pack: o[0] | (o[1] << 2) | (o[2] << 4), seen: `${sr},${sg}` }); return;
        }
        case 'dim': {
          // ファームは「残す割合」を k/256 で受ける（k = 送る値 + 1）。％からの変換はここで行う。
          const d = args[0] ? n0(args[0]) : 20; if (d === null) return;
          const percent = Math.max(0, Math.min(100, d));
          const scale = Math.max(0, Math.min(255, Math.round((100 - percent) / 100 * 256) - 1));
          this.emit({ op: 'NEO_DIM', percent, scale }); return;
        }
        default: this.error(node, `unsupported NeoPixel method "${node.name}"`); return;
      }
    }
    if (varInfo.kind === 'Tone') {
      switch (method) {
        case 'frequency': { const freq = this.evalInt(args[0]); if (freq === null) return; this.emit({ op: 'TONE_FREQ', pin: varInfo.pin, freq }); return; }
        case 'off': this.emit({ op: 'TONE_FREQ', pin: varInfo.pin, freq: 0 }); return;
        case 'tone': {
          const freq = this.evalInt(args[0]), secs = args[1] ? this.evalFloat(args[1]) : null;
          if (freq === null) return;
          this.emit({ op: 'TONE_FREQ', pin: varInfo.pin, freq });
          if (secs !== null) this.emit({ op: 'WAIT_MS', ms: Math.max(0, Math.round(secs * 1000)) });
          this.emit({ op: 'TONE_FREQ', pin: varInfo.pin, freq: 0 }); return;
        }
        default: this.error(node, `unsupported Tone method "${method}"`);
      }
      return;
    }
    if (varInfo.kind === 'PWM') {
      switch (method) {
        case 'frequency': {
          const freq = this.evalInt(args[0]);
          if (freq === null) return;
          varInfo.freq = freq;  // angle() の 50Hz チェック用にコンパイル時記録
          this.emit({ op: 'PWM_BASE_FREQ', pin: varInfo.pin, freq });
          return;
        }
        case 'duty': {
          const argNode = args[0];
          if (!argNode) { this.error(node, 'duty requires an argument'); return; }
          const constVal = this.evalFloatConst(argNode);
          if (constVal !== null) {
            const duty = Math.round(constVal);
            if (duty < 0 || duty > 255) { this.error(node, 'duty: 0〜255 の範囲で指定してください'); return; }
            this.emit({ op: 'PWM_DUTY', pin: varInfo.pin, duty });
            return;
          }
          const reg = this.getVarReg(argNode);
          if (reg !== undefined) {
            if (!this.comps.Q1) { this.error(node, 'duty(変数): Q1 コンポーネントが必要です。チェックしてください。'); return; }
            this.emit({ op: 'PWM_DUTY_REG', pin: varInfo.pin, reg });
            return;
          }
          this.error(node, 'duty: 引数はリテラルまたは数値変数のみ対応');
          return;
        }
        case 'angle': {
          // サーボは 50Hz 標準タイミング（周期 20ms、パルス 0.5〜2.4ms = 0〜180°）
          // duty = 6.4 + θ×24.32/180（8bit・78µs 刻みのため分解能は約7°/ステップ）
          // 旧式（500Hz・1〜2ms）はパルス間の Low が消えてサーボが誤動作するため廃止
          const argNode = args[0];
          if (!argNode) { this.error(node, 'angle requires an argument'); return; }
          if (varInfo.freq !== 50) { this.error(node, 'angle: サーボは 50Hz 標準タイミングで動かします。先に servo.frequency(50) を呼んでください'); return; }
          const constVal = this.evalFloatConst(argNode);
          if (constVal !== null) {
            if (constVal < 0 || constVal > 180) { this.error(node, 'angle: 0〜180 の範囲で指定してください'); return; }
            const duty = Math.round(6.4 + constVal * 24.32 / 180);
            this.emit({ op: 'PWM_DUTY', pin: varInfo.pin, duty });
            return;
          }
          const angleReg = this.getVarReg(argNode);
          if (angleReg === undefined) { this.error(node, 'angle: 引数はリテラルまたは数値変数のみ対応'); return; }
          if (!this.comps.Q1) { this.error(node, 'angle(変数): Q1 コンポーネントが必要です。チェックしてください。'); return; }
          // 演算順は「÷180 → ×24.32」— 旧ファーム（2026-07-12 以前）の MUL_Q16 は int16 キャストで
          // ±128 以上のオペランドが壊れるため、θ を先に割って ≤1.0 にする（修正済みファームでも正しい）
          const t1 = this.allocTempReg([angleReg]);
          const t2 = this.allocTempReg([angleReg, t1]);
          this.emit({ op: 'LOAD_Q16', reg: t1, value: 0 });         // t1 = θ のコピー（θ のレジスタは保持）
          this.emit({ op: 'ADD_Q16',  dst: t1, src: angleReg });
          this.emit({ op: 'LOAD_Q16', reg: t2, value: 180 << 8 });
          this.emit({ op: 'DIV_Q16',  dst: t1, src: t2 });          // θ/180 ≤ 1.0（int16 範囲内）
          this.emit({ op: 'LOAD_Q16', reg: t2, value: 6226 });      // × 24.32 (Q16.8)
          this.emit({ op: 'MUL_Q16',  dst: t1, src: t2 });
          this.emit({ op: 'LOAD_Q16', reg: t2, value: 1778 });      // + 6.4 + 0.5(>>8切り捨ての四捨五入化) + 0.05(÷180切り捨ての平均補正)
          this.emit({ op: 'ADD_Q16',  dst: t1, src: t2 });
          this.emit({ op: 'PWM_DUTY_REG', pin: varInfo.pin, reg: t1 });
          return;
        }
        default: this.error(node, `unsupported PWM method "${method}"`);
      }
      return;
    }
  }

  handleLoop(blockNode) {
    const loopStart = this.currentOffset, breakPatches = [];
    this.loopStack.push({ startOffset: loopStart, breakPatches });
    this.visitStatements(blockNode.body);
    this.loopStack.pop();
    const jmpIdx = this.emit({ op: 'JMP', relOffset: 0 });
    const jmpEnd = instrOffset(this.instructions, jmpIdx) + instrSize(this.instructions[jmpIdx]);
    this.instructions[jmpIdx].relOffset = loopStart - jmpEnd;
    for (const bIdx of breakPatches) this.patchJump(bIdx);
  }

  visitWhile(node) {
    const loopStart = this.currentOffset;
    const cond = this.evalCondition(node.predicate);
    if (!cond) return;
    const jumpToEnd = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
    for (const j of (cond.extraBodyJumps ?? [])) this.patchJump(j);
    const breakPatches = [];
    this.loopStack.push({ startOffset: loopStart, breakPatches });
    this.visitStatements(node.statements);
    this.loopStack.pop();
    const jmpIdx = this.emit({ op: 'JMP', relOffset: 0 });
    const jmpEnd = instrOffset(this.instructions, jmpIdx) + instrSize(this.instructions[jmpIdx]);
    this.instructions[jmpIdx].relOffset = loopStart - jmpEnd;
    for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
    this.patchJump(jumpToEnd);
    for (const bIdx of breakPatches) this.patchJump(bIdx);
  }

  visitUntil(node) {
    const loopStart = this.currentOffset;
    const cond = this.evalCondition(node.predicate);
    if (!cond) return;
    // until は条件反転（DeMorgan: extraBodyJumps ↔ extraElseJumps を入れ替え）
    const inv = {
      jumpOp: cond.jumpOp === 'JZ' ? 'JNZ' : 'JZ',
      reg: cond.reg,
      extraElseJumps: cond.extraBodyJumps ?? [],
      extraBodyJumps: cond.extraElseJumps ?? [],
    };
    const jumpToEnd = this.emit({ op: inv.jumpOp, reg: inv.reg ?? 0, relOffset: 0 });
    for (const j of inv.extraBodyJumps) this.patchJump(j);
    const breakPatches = [];
    this.loopStack.push({ startOffset: loopStart, breakPatches });
    this.visitStatements(node.statements);
    this.loopStack.pop();
    const jmpIdx = this.emit({ op: 'JMP', relOffset: 0 });
    const jmpEnd = instrOffset(this.instructions, jmpIdx) + instrSize(this.instructions[jmpIdx]);
    this.instructions[jmpIdx].relOffset = loopStart - jmpEnd;
    for (const j of inv.extraElseJumps) this.patchJump(j);
    this.patchJump(jumpToEnd);
    for (const bIdx of breakPatches) this.patchJump(bIdx);
  }

  handleTimes(callNode) {
    const count = Number(callNode.receiver.value);
    if (!Number.isInteger(count) || count < 0) { this.error(callNode, 'times requires a non-negative integer literal'); return; }

    if (this.comps.Q1) {
      // Q1 ランタイムループ: R2=カウンタ, R3=スクラッチ（R0/R1 はユーザ変数）
      const R2 = 2, R3 = 3;
      this.emit({ op: 'LOAD_Q16', reg: R2, value: 0 });             // R2 = 0
      const loopStart = this.currentOffset;
      this.emit({ op: 'LOAD_Q16', reg: R3, value: count << 8 });    // R3 = n
      this.emit({ op: 'CMP_LT_Q16', lhs: R2, rhs: R3, out: R3 });  // R3 = (R2 < n)
      const jzIdx = this.emit({ op: 'JZ', reg: R3, relOffset: 0 }); // R3==0 → exit
      const breakPatches = [], nextPatches = [];
      this.loopStack.push({ startOffset: loopStart, breakPatches, nextPatches });
      this.visitStatements(callNode.block.body);
      this.loopStack.pop();
      const continueTarget = this.currentOffset;
      this.emit({ op: 'LOAD_Q16', reg: R3, value: 1 << 8 });        // R3 = 1
      this.emit({ op: 'ADD_Q16',  dst: R2, src: R3 });               // R2 += 1
      for (const nIdx of nextPatches) {
        const instrEnd = instrOffset(this.instructions, nIdx) + instrSize(this.instructions[nIdx]);
        this.instructions[nIdx].relOffset = continueTarget - instrEnd;
      }
      const jmpIdx = this.emit({ op: 'JMP', relOffset: 0 });
      const jmpEnd = instrOffset(this.instructions, jmpIdx) + instrSize(this.instructions[jmpIdx]);
      this.instructions[jmpIdx].relOffset = loopStart - jmpEnd;
      this.patchJump(jzIdx);
      for (const bIdx of breakPatches) this.patchJump(bIdx);
    } else {
      for (let i = 0; i < count; i++) this.visitStatements(callNode.block.body);
    }
  }

  handleEachRange(callNode, rangeNode) {
    const block = callNode.block;
    const reqParams = block.parameters?.parameters?.requireds ?? [];
    if (reqParams.length === 0) { this.error(callNode, '.each: ブロックパラメータが必要です'); return; }
    const paramName = reqParams[0].name;
    if (!paramName) { this.error(callNode, '.each: ブロックパラメータ名が取得できません'); return; }
    const left = this.evalFloatConst(rangeNode.left), right = this.evalFloatConst(rangeNode.right);
    if (left === null || right === null) { this.error(rangeNode, '.each: レンジの両端は整数リテラルのみ対応しています'); return; }
    const start = Math.round(left), end = Math.round(right);
    const limit = rangeNode.isExcludeEnd() ? end - 1 : end;
    if (Math.abs(limit - start) > 255) { this.error(callNode, '.each: レンジが大きすぎます（最大 256 要素）'); return; }
    const step = start <= limit ? 1 : -1;
    this.forVars = this.forVars ?? {};
    for (let v = start; (step > 0 ? v <= limit : v >= limit); v += step) {
      this.forVars[paramName] = v;
      this.visitStatements(block.body);
    }
    delete this.forVars[paramName];
  }

  handleEachArray(callNode, arrayNode) {
    const block = callNode.block;
    const reqParams = block.parameters?.parameters?.requireds ?? [];
    if (reqParams.length === 0) { this.error(callNode, '.each: ブロックパラメータが必要です'); return; }
    const paramName = reqParams[0].name;
    if (!paramName) { this.error(callNode, '.each: ブロックパラメータ名が取得できません'); return; }
    const values = [];
    for (const elem of arrayNode.elements) {
      const val = this.evalFloatConst(elem);
      if (val === null) { this.error(elem, '.each: 配列要素は数値リテラルのみ対応しています'); return; }
      values.push(val);
    }
    this.forVars = this.forVars ?? {};
    for (const val of values) {
      this.forVars[paramName] = val;
      this.visitStatements(block.body);
    }
    delete this.forVars[paramName];
  }

  visitIf(node) {
    const cond = this.evalCondition(node.predicate);
    if (!cond) return;
    const jumpToElse = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
    for (const j of (cond.extraBodyJumps ?? [])) this.patchJump(j);
    this.visitStatements(node.statements);
    if (node.subsequent) {
      const jumpToEnd = this.emit({ op: 'JMP', relOffset: 0 });
      for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
      this.patchJump(jumpToElse);
      const sub = node.subsequent;
      if (sub.constructor.name === 'ElseNode') this.visitStatements(sub.statements);
      else if (sub.constructor.name === 'IfNode') this.visitIf(sub);
      this.patchJump(jumpToEnd);
    } else {
      for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
      this.patchJump(jumpToElse);
    }
  }

  evalCondition(node) {
    if (node.constructor.name === 'AndNode') {
      // a && b: 両方 true のとき条件成立
      const left = this.evalCondition(node.left);
      if (!left) return null;
      const jElse = this.emit({ op: left.jumpOp, reg: left.reg ?? 0, relOffset: 0 });
      const right = this.evalCondition(node.right);
      if (!right) return null;
      return {
        jumpOp: right.jumpOp, reg: right.reg,
        extraElseJumps: [jElse, ...(left.extraElseJumps ?? []), ...(right.extraElseJumps ?? [])],
        extraBodyJumps: [...(left.extraBodyJumps ?? []), ...(right.extraBodyJumps ?? [])],
      };
    }
    if (node.constructor.name === 'OrNode') {
      // a || b: どちらか true のとき条件成立
      const left = this.evalCondition(node.left);
      if (!left) return null;
      const trueOp = left.jumpOp === 'JZ' ? 'JNZ' : 'JZ';
      const jBody = this.emit({ op: trueOp, reg: left.reg ?? 0, relOffset: 0 });
      // left の extraElseJumps は「左辺が false → 次の条件へ」なので fall-through にする
      for (const j of (left.extraElseJumps ?? [])) this.patchJump(j);
      const right = this.evalCondition(node.right);
      if (!right) return null;
      return {
        jumpOp: right.jumpOp, reg: right.reg,
        extraElseJumps: [...(right.extraElseJumps ?? [])],
        extraBodyJumps: [jBody, ...(left.extraBodyJumps ?? []), ...(right.extraBodyJumps ?? [])],
      };
    }
    if (node.constructor.name !== 'CallNode') {
      this.error(node, `unsupported if condition "${node.constructor.name}"`); return null;
    }
    const recv = node.receiver;
    if (!recv && this.defs[node.name]) return this.evalDefCondition(node);
    // f.call(x) / ->(x){...}.call(x) を条件式として展開
    if (node.name === 'call' && recv) {
      const lam = this.lambdaOf(recv);
      if (lam === null && recv.constructor.name === 'LambdaNode') return null;  // 検証エラー済み
      if (lam) return this.withLambdaEnv(lam, node.arguments_?.arguments_ ?? [], node, () => this.evalCondition(lam.body));
    }
    const varInfo = recv?.constructor.name === 'LocalVariableReadNode' ? this.vars[recv.name]
      : (recv?.constructor.name === 'CallNode' && !recv.receiver && !(recv.arguments_?.arguments_?.length))
        ? this.vars[recv.name] : null;
    if (varInfo && varInfo.kind === 'Serial') {
      if (node.name === 'available?' || node.name === 'available') {
        this.emit({ op: 'SERIAL_AVAILABLE', reg: 0 });
        return { jumpOp: 'JZ' };
      }
      this.error(node, `if の条件に使える Serial のメソッドは available? だけです`); return null;
    }
    if (varInfo) {
      if (varInfo.kind !== 'GPIO') { this.error(node, `"${recv.name}" is not a GPIO variable`); return null; }
      if (node.name === 'low?')  { this.emit({ op: 'GPIO_READ', pin: varInfo.pin, reg: 0 }); return { jumpOp: 'JNZ' }; }
      if (node.name === 'high?') { this.emit({ op: 'GPIO_READ', pin: varInfo.pin, reg: 0 }); return { jumpOp: 'JZ'  }; }
    }
    // 文字 EEPROM 変数の == / != → VAR_STR_CMP / VAR_STR_CMP_V
    if (node.name === '==' || node.name === '!=') {
      const ln = this.numericVarName(node.receiver);
      const lsv = ln ? this.eeVars[ln] : null;
      if (lsv && (lsv.type & 0x7F) === 1) return this.evalStrCond(node, lsv);
    }
    // CMP は LT/GT/EQ の3種のみ（spec_q1.md）。GE/LE/NE は補命令 + JNZ に変換
    const Q168_CMP = {
      '<':  { op: 'CMP_LT_Q16', jump: 'JZ'  },
      '>':  { op: 'CMP_GT_Q16', jump: 'JZ'  },
      '==': { op: 'CMP_EQ_Q16', jump: 'JZ'  },
      '>=': { op: 'CMP_LT_Q16', jump: 'JNZ' },  // a>=b ⟺ !(a<b)
      '<=': { op: 'CMP_GT_Q16', jump: 'JNZ' },  // a<=b ⟺ !(a>b)
      '!=': { op: 'CMP_EQ_Q16', jump: 'JNZ' },  // a!=b ⟺ !(a==b)
    };
    if (Q168_CMP[node.name]) return this.evalQ168Cond(node, Q168_CMP[node.name]);
    this.error(node, `unsupported if condition "${node.name}"`); return null;
  }

  evalQ168Cond(node, cmp) {
    if (!this.comps.Q1) { this.error(node, `比較演算 "${node.name}" には Q1 コンポーネントが必要です。チェックしてください。`); return null; }
    const lhsReg = this.loadNumericVar(node.receiver);
    if (lhsReg === undefined) { this.error(node, 'Q16.8: 左辺は数値変数である必要があります'); return null; }
    const arg = node.arguments_?.arguments_?.[0];
    let rhsReg;
    const constVal = this.evalQ168(arg);
    if (constVal !== null) {
      rhsReg = this.allocTempReg([lhsReg]);
      this.emit({ op: 'LOAD_Q16', reg: rhsReg, value: constVal });
    } else {
      rhsReg = this.loadNumericVar(arg, [lhsReg]);
      if (rhsReg === undefined) { this.error(node, 'Q16.8: 右辺は数値定数または変数である必要があります'); return null; }
    }
    const resReg = this.allocTempReg([lhsReg, rhsReg]);
    this.emit({ op: cmp.op, lhs: lhsReg, rhs: rhsReg, out: resReg });
    return { jumpOp: cmp.jump, reg: resReg };
  }

  getVarReg(node) {
    if (!node) return undefined;
    const cn = node.constructor.name;
    if (cn === 'LocalVariableReadNode') return this.regs[node.name];
    if (cn === 'CallNode' && !node.receiver && !(node.arguments_?.arguments_?.length)) return this.regs[node.name];
    return undefined;
  }

  // 数値変数の参照ノードから変数名を取得（$グローバル / ローカル / 引数なしメソッド形）
  numericVarName(node) {
    if (!node) return null;
    const cn = node.constructor.name;
    if (cn === 'GlobalVariableReadNode') return node.name;
    if (cn === 'LocalVariableReadNode') return node.name;
    if (cn === 'CallNode' && !node.receiver && !(node.arguments_?.arguments_?.length)) return node.name;
    return null;
  }

  // EEPROM 変数を登録（既存ならそのまま返す）。type bit7 = 永続（$変数）
  eeVar(node, name, type, count) {
    if (name in this.eeVars) return this.eeVars[name];
    if (Object.keys(this.eeVars).length >= 6) { this.error(node, `EEPROM 変数は最大6個です（"${name}" を追加できません）`); return null; }
    const plain = name.startsWith('$') ? name.slice(1) : name;
    if (plain.length > 16) { this.error(node, `変数名 "${plain}" は16文字を超えています`); return null; }
    this.eeVars[name] = { idx: Object.keys(this.eeVars).length, type, count, name: plain };
    return this.eeVars[name];
  }

  // 数値変数（レジスタ or EEPROM）をレジスタに用意して reg 番号を返す。EEPROM 変数は VAR_LOAD を emit
  loadNumericVar(node, excludeRegs = []) {
    node = this.lambdaArgNode(node);  // ラムダ仮引数 → 束縛された実引数ノード
    const reg = this.getVarReg(node);
    if (reg !== undefined) return reg;
    const name = this.numericVarName(node);
    if (name !== null && name in this.eeVars) {
      const t = this.allocTempReg(excludeRegs);
      this.emit({ op: 'VAR_LOAD', varIdx: this.eeVars[name].idx, reg: t });
      return t;
    }
    // arr[i]（配列要素の読み取り）
    if (node?.constructor.name === 'CallNode' && node.name === '[]') {
      const evar = this.eeVars[this.numericVarName(node.receiver) ?? ''];
      if (evar) {
        const idxReg = this.loadIndexReg(node.arguments_?.arguments_?.[0], excludeRegs, node);
        if (idxReg === undefined) return undefined;
        const t = this.allocTempReg([...excludeRegs, idxReg]);
        this.emit({ op: 'VAR_LOAD_IDX', varIdx: evar.idx, idxReg, reg: t });
        return t;
      }
    }
    return undefined;
  }

  allocTempReg(excludeRegs = []) {
    const used = new Set([...Object.values(this.regs), ...excludeRegs]);
    for (let r = 0; r < 4; r++) { if (!used.has(r)) return r; }
    return 3;
  }

  visitFor(node) {
    const varName = node.index.name;
    if (!varName) { this.error(node, 'for: ループ変数が取得できません'); return; }
    const cn = node.collection.constructor.name;
    let values = [];
    if (cn === 'ArrayNode') {
      for (const elem of node.collection.elements) {
        const val = this.evalFloatConst(elem);
        if (val === null) { this.error(elem, 'for: 配列要素は数値リテラルのみ対応しています'); return; }
        values.push(val);
      }
    } else if (cn === 'RangeNode') {
      const left = this.evalFloatConst(node.collection.left), right = this.evalFloatConst(node.collection.right);
      if (left === null || right === null) { this.error(node.collection, 'for: レンジの両端は整数リテラルのみ対応しています'); return; }
      const start = Math.round(left), end = Math.round(right);
      const limit = node.collection.isExcludeEnd() ? end - 1 : end;
      if (Math.abs(limit - start) > 255) { this.error(node, 'for: レンジが大きすぎます（最大 256 要素）'); return; }
      const step = start <= limit ? 1 : -1;
      for (let v = start; step > 0 ? v <= limit : v >= limit; v += step) values.push(v);
    } else {
      this.error(node, 'for: コレクションは配列 [...] またはレンジ n..m / n...m のみ対応しています'); return;
    }
    this.forVars = this.forVars ?? {};
    for (const val of values) { this.forVars[varName] = val; this.visitStatements(node.statements); }
    delete this.forVars[varName];
  }

  visitUnless(node) {
    const cond = this.evalCondition(node.predicate);
    if (!cond) return;
    // unless は条件を反転（DeMorgan: extraBodyJumps ↔ extraElseJumps を入れ替え）
    const inv = {
      jumpOp: cond.jumpOp === 'JZ' ? 'JNZ' : 'JZ',
      reg: cond.reg,
      extraElseJumps: cond.extraBodyJumps ?? [],
      extraBodyJumps: cond.extraElseJumps ?? [],
    };
    const jumpToSkip = this.emit({ op: inv.jumpOp, reg: inv.reg ?? 0, relOffset: 0 });
    for (const j of inv.extraBodyJumps) this.patchJump(j);
    this.visitStatements(node.statements);
    if (node.elseClause) {
      const jumpPastElse = this.emit({ op: 'JMP', relOffset: 0 });
      for (const j of inv.extraElseJumps) this.patchJump(j);
      this.patchJump(jumpToSkip);
      this.visitStatements(node.elseClause.statements);
      this.patchJump(jumpPastElse);
    } else {
      for (const j of inv.extraElseJumps) this.patchJump(j);
      this.patchJump(jumpToSkip);
    }
  }

  visitDef(node) {
    const params = node.parameters?.requireds?.map(p => p.name) ?? [];
    if (params.length > 2) { this.error(node, 'def: 引数は最大 2 個までです'); return; }
    this.defs[node.name] = { body: node.body, params };
  }

  visitReturn(node) {
    if (this.returnPatchStack.length === 0) { this.error(node, 'return outside of def'); return; }
    const idx = this.emit({ op: 'JMP', relOffset: 0 });
    this.returnPatchStack[this.returnPatchStack.length - 1].push(idx);
  }

  visitCase(node) {
    const endJumps = [];
    for (const whenNode of node.conditions) {
      const vals = whenNode.conditions;
      if (vals.length === 0) continue;
      const orBodyJumps = [];
      for (let i = 0; i < vals.length; i++) {
        const isLast = i === vals.length - 1;
        const cond = node.predicate
          ? this.evalCaseWhenEquality(node.predicate, vals[i])
          : this.evalCondition(vals[i]);
        if (!cond) return;
        if (!isLast) {
          // OR: true → body へジャンプ、false → 次の値を確認
          const trueOp = cond.jumpOp === 'JZ' ? 'JNZ' : 'JZ';
          orBodyJumps.push(this.emit({ op: trueOp, reg: cond.reg ?? 0, relOffset: 0 }));
          for (const j of (cond.extraBodyJumps ?? [])) orBodyJumps.push(j);
          // extraElseJumps (&&由来) は次の値チェックへ fall-through
          for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
        } else {
          // 最後の値: false → 次の when/else へジャンプ
          const jumpToNext = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
          // OR body ジャンプ群をここ（body 先頭）へパッチ
          for (const j of [...orBodyJumps, ...(cond.extraBodyJumps ?? [])]) this.patchJump(j);
          this.visitStatements(whenNode.statements);
          endJumps.push(this.emit({ op: 'JMP', relOffset: 0 }));
          // jumpToNext と extraElseJumps を次の when 先頭へパッチ
          for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
          this.patchJump(jumpToNext);
        }
      }
    }
    if (node.elseClause) this.visitStatements(node.elseClause.statements);
    for (const j of endJumps) this.patchJump(j);
  }

  visitMultiWrite(node) {
    if (!this.comps.Q1) { this.error(node, '多重代入には Q1 コンポーネントが必要です。チェックしてください。'); return; }
    const lefts = node.lefts ?? [];
    const elems = node.value?.elements ?? [];
    if (lefts.length !== elems.length) { this.error(node, `多重代入: 左辺 ${lefts.length} 個、右辺 ${elems.length} 個が一致しません`); return; }
    for (let i = 0; i < lefts.length; i++) {
      const target = lefts[i];
      if (target.constructor.name !== 'LocalVariableTargetNode') { this.error(target, '多重代入: 左辺はローカル変数のみ対応しています'); return; }
      this.handleNumericAssign(target.name, elems[i], node);
    }
  }

  evalCaseWhenEquality(predNode, valNode) {
    if (!this.comps.Q1) { this.error(predNode, 'case/when（数値比較）には Q1 コンポーネントが必要です。チェックしてください。'); return null; }
    // when ->(x){ x < 15 } / when f — case 対象を引数に束縛して本体を条件式として展開
    const lam = this.lambdaOf(valNode);
    if (lam === null && valNode.constructor.name === 'LambdaNode') return null;  // 検証エラー済み
    if (lam) {
      if (lam.params.length !== 1) { this.error(valNode, 'when のラムダは仮引数1個が必要です'); return null; }
      return this.withLambdaEnv(lam, [predNode], valNode, () => this.evalCondition(lam.body));
    }
    const lhsReg = this.loadNumericVar(predNode);
    if (lhsReg === undefined) { this.error(predNode, 'case: 対象は数値変数のみ対応しています'); return null; }
    if (valNode.constructor.name === 'RangeNode') return this.evalCaseWhenRange(lhsReg, valNode);
    const constVal = this.evalQ168(valNode);
    let rhsReg;
    if (constVal !== null) {
      rhsReg = this.allocTempReg([lhsReg]);
      this.emit({ op: 'LOAD_Q16', reg: rhsReg, value: constVal });
    } else {
      rhsReg = this.loadNumericVar(valNode, [lhsReg]);
      if (rhsReg === undefined) { this.error(valNode, 'case when: 比較値は数値リテラル・変数・レンジ（a..b / a...b）・ラムダ（->(x){ 条件 }）のみ対応しています'); return null; }
    }
    const outReg = this.allocTempReg([lhsReg, rhsReg]);
    this.emit({ op: 'CMP_EQ_Q16', lhs: lhsReg, rhs: rhsReg, out: outReg });
    return { jumpOp: 'JZ', reg: outReg };
  }

  // when low..high / low...high → 範囲比較（CMP_LT/GT の既存オペコードのみ、VM 変更なし）
  // 下限チェックの不成立ジャンプは extraElseJumps で返し、上限チェックを最終条件にする
  evalCaseWhenRange(lhsReg, rangeNode) {
    const outReg = this.allocTempReg([lhsReg]);
    const boundReg = (bnode) => {
      const cv = this.evalQ168(bnode);
      if (cv !== null) {
        const t = this.allocTempReg([lhsReg, outReg]);
        this.emit({ op: 'LOAD_Q16', reg: t, value: cv });
        return t;
      }
      const r = this.loadNumericVar(bnode, [lhsReg, outReg]);
      if (r === undefined) this.error(bnode, 'case when: レンジの端は数値リテラルまたは変数のみ対応しています');
      return r;
    };
    const extraElseJumps = [];
    if (rangeNode.left) {
      const lowReg = boundReg(rangeNode.left);
      if (lowReg === undefined) return null;
      this.emit({ op: 'CMP_LT_Q16', lhs: lhsReg, rhs: lowReg, out: outReg });  // pred < low → 範囲外
      if (!rangeNode.right) return { jumpOp: 'JNZ', reg: outReg };             // 終端なし（low..）: これが最終条件
      extraElseJumps.push(this.emit({ op: 'JNZ', reg: outReg, relOffset: 0 }));
    }
    if (rangeNode.right) {
      const highReg = boundReg(rangeNode.right);
      if (highReg === undefined) return null;
      if (rangeNode.isExcludeEnd()) {
        this.emit({ op: 'CMP_LT_Q16', lhs: lhsReg, rhs: highReg, out: outReg }); // pred < high → 範囲内
        return { jumpOp: 'JZ', reg: outReg, extraElseJumps };
      }
      this.emit({ op: 'CMP_GT_Q16', lhs: lhsReg, rhs: highReg, out: outReg });   // pred > high → 範囲外
      return { jumpOp: 'JNZ', reg: outReg, extraElseJumps };
    }
    this.error(rangeNode, 'case when: レンジの両端がありません');
    return null;
  }

  // ------------------------------------------------------------
  //  ラムダ式（コンパイル時インライン展開 — VM にクロージャは存在しない）
  //  f = ->(x) { x < 15 } を登録し、f.call(d) / when f / when ->(x){...} の
  //  呼び出し位置で仮引数を実引数に束縛して本体を展開する
  // ------------------------------------------------------------

  // LambdaNode → { params, body }。本体は式1つのみ対応
  lambdaFromNode(node) {
    const reqs = node.parameters?.parameters?.requireds ?? [];
    const params = [];
    for (const r of reqs) {
      if (r.constructor.name !== 'RequiredParameterNode') { this.error(node, 'ラムダの仮引数は通常の引数のみ対応しています（デフォルト値等は不可）'); return null; }
      params.push(r.name);
    }
    if (node.parameters?.parameters?.optionals?.length || node.parameters?.parameters?.rest ||
        node.parameters?.parameters?.keywords?.length || node.parameters?.parameters?.block) {
      this.error(node, 'ラムダの仮引数は通常の引数のみ対応しています（デフォルト値・可変長・キーワード引数は不可）'); return null;
    }
    let body = node.body;
    if (body?.constructor.name === 'StatementsNode') body = (body.body?.length === 1) ? body.body[0] : null;
    if (!body) { this.error(node, 'ラムダ式の本体は式1つだけ対応しています'); return null; }
    return { params, body };
  }

  handleLambdaAssign(name, val, node) {
    if (name in this.regs || name in this.eeVars || name in this.vars) {
      this.error(node, `"${name}" は既に変数として使用済みです。ラムダには別名を使ってください`); return;
    }
    const lam = this.lambdaFromNode(val);
    if (lam) this.lambdas[name] = lam;  // バイトコードは出さない（呼び出し位置で展開）
  }

  // ノードがラムダなら { params, body } を返す（リテラル or 登録済み変数）
  lambdaOf(node) {
    if (!node) return null;
    if (node.constructor.name === 'LambdaNode') return this.lambdaFromNode(node);
    const name = this.numericVarName(node);
    return name !== null ? (this.lambdas[name] ?? null) : null;
  }

  // 展開中の仮引数参照を実引数ノードに置き換える（1段のみ — 束縛時に解決済みのため）
  lambdaArgNode(node) {
    if (node?.constructor.name === 'LocalVariableReadNode') {
      const b = this.lambdaArgs?.[node.name];
      if (b !== undefined) return b;
    }
    return node;
  }

  // 束縛時の解決: 呼び出し側環境で仮引数の連鎖をたどり、実体（実変数ノード等）まで潰す
  resolveArgDeep(node) {
    let n = node;
    for (let guard = 0; guard < 8; guard++) {
      const next = this.lambdaArgNode(n);
      if (next === n) break;
      n = next;
    }
    return n;
  }

  // 引数を束縛して fn() を実行（定数 → defParams、変数 → lambdaArgs。環境は入れ替えて復元）
  withLambdaEnv(lam, argNodes, errNode, fn) {
    if (argNodes.length !== lam.params.length) { this.error(errNode, `ラムダの引数は ${lam.params.length} 個必要です`); return null; }
    if (this.lambdaDepth >= 8) { this.error(errNode, 'ラムダの入れ子が深すぎます（再帰呼び出しは非対応）'); return null; }
    const constBind = {}, nodeBind = {};
    for (let i = 0; i < argNodes.length; i++) {
      const cv = this.evalFloatConst(argNodes[i]);  // 呼び出し側環境で定数評価
      if (cv !== null) { constBind[lam.params[i]] = cv; continue; }
      const rn = this.resolveArgDeep(argNodes[i]);
      if (this.numericVarName(rn) !== null || (rn?.constructor.name === 'CallNode' && rn.name === '[]')) {
        nodeBind[lam.params[i]] = rn; continue;
      }
      this.error(argNodes[i], 'ラムダの引数は数値リテラル（定数式）または変数のみ対応しています'); return null;
    }
    const savedD = this.defParams, savedL = this.lambdaArgs;
    this.defParams = constBind; this.lambdaArgs = nodeBind;
    this.lambdaDepth++;
    const r = fn();
    this.lambdaDepth--;
    this.defParams = savedD; this.lambdaArgs = savedL;
    return r;
  }

  inlineDef(name, callNode) {
    const def = this.defs[name];
    const args = callNode.arguments_?.arguments_ ?? [];
    const savedDefParams = this.defParams;
    if (def.params.length > 0) {
      if (args.length !== def.params.length) {
        this.error(callNode, `${name}: 引数は ${def.params.length} 個必要です`); return;
      }
      this.defParams = { ...(savedDefParams ?? {}) };
      for (let i = 0; i < def.params.length; i++) {
        const val = this.evalFloatConst(args[i]);
        if (val === null) { this.error(args[i], `${name}: 引数は数値リテラルのみ対応しています`); return; }
        this.defParams[def.params[i]] = val;
      }
    }
    this.returnPatchStack.push([]);
    this.visitStatements(def.body);
    const patches = this.returnPatchStack.pop();
    for (const idx of patches) this.patchJump(idx);
    this.defParams = savedDefParams;
  }

  evalDefCondition(callNode) {
    const def = this.defs[callNode.name];
    const stmts = def?.body?.body ?? [];
    if (stmts.length !== 1 || stmts[0].constructor.name !== 'ReturnNode') {
      this.error(callNode, `"${callNode.name}" is too complex for if condition`); return null;
    }
    const retVal = stmts[0].arguments_?.arguments_?.[0];
    if (!retVal) { this.error(callNode, `"${callNode.name}" has no return value`); return null; }
    return this.evalCondition(retVal);
  }

  visitBreak(node) {
    if (this.loopStack.length === 0) { this.error(node, 'break outside of loop'); return; }
    const frame = this.loopStack[this.loopStack.length - 1];
    const idx = this.emit({ op: 'JMP', relOffset: 0 });
    frame.breakPatches.push(idx);
  }

  visitNext(node) {
    if (this.loopStack.length === 0) { this.error(node, 'next outside of loop'); return; }
    const frame = this.loopStack[this.loopStack.length - 1];
    const idx = this.emit({ op: 'JMP', relOffset: 0 });
    if (frame.nextPatches) {
      // n.times Q1 ループ: increment ステップへの defer patch
      frame.nextPatches.push(idx);
    } else {
      const instrEnd = instrOffset(this.instructions, idx) + instrSize(this.instructions[idx]);
      this.instructions[idx].relOffset = frame.startOffset - instrEnd;
    }
  }

  handlePrint(node, flags) {
    const args = node.arguments_?.arguments_ ?? [], arg = args[0];
    if (!arg) {
      if (flags & 0x01) { this.emit({ op: 'PRINT_STR', flags, str: [] }); return; }
      this.error(node, 'print requires an argument'); return;
    }
    // print cond ? "O" : " " — 三項演算子（条件で出力を切り替え）
    if (arg.constructor.name === 'IfNode' && arg.statements && arg.subsequent) {
      const thenStmts = arg.statements?.body ?? [];
      const elseStmts = arg.subsequent?.statements?.body ?? [];
      if (thenStmts.length !== 1 || elseStmts.length !== 1) { this.error(node, 'print の三項演算子は単一の値のみ対応'); return; }
      const cond = this.evalCondition(arg.predicate);
      if (!cond) return;
      const jumpToElse = this.emit({ op: cond.jumpOp, reg: cond.reg ?? 0, relOffset: 0 });
      for (const j of (cond.extraBodyJumps ?? [])) this.patchJump(j);
      this.printValueNode(thenStmts[0], flags, node);
      const jumpToEnd = this.emit({ op: 'JMP', relOffset: 0 });
      for (const j of (cond.extraElseJumps ?? [])) this.patchJump(j);
      this.patchJump(jumpToElse);
      this.printValueNode(elseStmts[0], flags, node);
      this.patchJump(jumpToEnd);
      return;
    }
    this.printValueNode(arg, flags, node);
  }

  // warn 値 — レジスタ生値を DEVICE LOG へ送る（Ec 不要のデバッグ出力。文字列化はブラウザ側）
  handleWarn(node) {
    const args = node.arguments_?.arguments_ ?? [], arg = args[0];
    if (!arg || args.length !== 1) { this.error(node, 'warn は引数 1 個（数値変数または sensor.read）が必要です'); return; }
    // warn t.ms / warn t.us（Timer の経過時間をそのまま DEVICE LOG へ）
    if (arg.constructor.name === 'CallNode' && (arg.name === 'ms' || arg.name === 'us') && arg.receiver) {
      const rn = this.numericVarName(arg.receiver);
      const vi = rn ? this.vars[rn] : null;
      if (vi?.kind === 'Timer') {
        this.emit({ op: arg.name === 'us' ? 'TIMER_US' : 'TIMER_MS', slot: vi.slot, reg: 0 });
        this.emit({ op: 'WARN_REG', reg: 0 }); return;
      }
    }
    if (arg.constructor.name === 'CallNode' && arg.name === 'read') {
      const recv = arg.receiver;
      const recvName = recv?.constructor.name === 'LocalVariableReadNode' ? recv.name
        : (recv?.constructor.name === 'CallNode' && !recv.receiver ? recv.name : null);
      const varInfo = recvName ? this.vars[recvName] : null;
      if (varInfo?.kind === 'ADC') {
        this.emit({ op: 'ADC_READ', pin: varInfo.pin, reg: 0 });
        this.emit({ op: 'WARN_REG', reg: 0 }); return;
      }
      if (varInfo?.kind === 'Ultrasonic') {
        this.emit({ op: 'ULTRASONIC_READ', trig: varInfo.trig, echo: varInfo.echo, reg: 0 });
        this.emit({ op: 'WARN_REG', reg: 0 }); return;
      }
    }
    const varReg = this.loadNumericVar(arg);
    if (varReg !== undefined) { this.emit({ op: 'WARN_REG', reg: varReg }); return; }
    this.error(node, 'warn は数値変数・sensor.read・t.ms / t.us のみ対応しています');
  }

  // 単一の値ノードを出力する（文字列リテラル / 数値変数 / 文字 EEPROM 変数 / sensor.read 等）
  printValueNode(arg, flags, node) {
    if (arg.constructor.name === 'CallNode' && arg.name === 'read') {
      const recv = arg.receiver;
      const recvName = recv?.constructor.name === 'LocalVariableReadNode' ? recv.name
        : (recv?.constructor.name === 'CallNode' && !recv.receiver ? recv.name : null);
      const varInfo = recvName ? this.vars[recvName] : null;
      if (varInfo?.kind === 'ADC') {
        if (!this.comps.Ec) { this.error(node, '数値出力（PRINT_REG）: Ec コンポーネントが必要です。デバッグ用途なら warn が Ec なしで使えます（DEVICE LOG に表示）。'); return; }
        this.emit({ op: 'ADC_READ',  pin: varInfo.pin, reg: 0 });
        this.emit({ op: 'PRINT_REG', flags: flags & 0x01, reg: 0 }); return;
      }
      if (varInfo?.kind === 'Ultrasonic') {
        if (!this.comps.Ec) { this.error(node, '数値出力（PRINT_REG）: Ec コンポーネントが必要です。デバッグ用途なら warn が Ec なしで使えます（DEVICE LOG に表示）。'); return; }
        this.emit({ op: 'ULTRASONIC_READ', trig: varInfo.trig, echo: varInfo.echo, reg: 0 });
        this.emit({ op: 'PRINT_REG', flags: flags & 0x01, reg: 0 }); return;
      }
    }
    // 文字 EEPROM 変数 → VAR_PRINT
    {
      const an = this.numericVarName(arg);
      if (an && this.eeVars[an] && (this.eeVars[an].type & 0x7F) === 1) {
        this.emit({ op: 'VAR_PRINT', flags, varIdx: this.eeVars[an].idx });
        return;
      }
    }
    // 数値変数（レジスタ / EEPROM）→ PRINT_REG（Ec 必須）
    const varReg = this.loadNumericVar(arg);
    if (varReg !== undefined) {
      if (!this.comps.Ec) { this.error(node, '数値出力（PRINT_REG）: Ec コンポーネントが必要です。デバッグ用途なら warn が Ec なしで使えます（DEVICE LOG に表示）。'); return; }
      this.emit({ op: 'PRINT_REG', flags: flags & 0x01, reg: varReg });
      return;
    }
    const str = this.evalStringLiteral(arg);
    if (str === null) return;
    this.emit({ op: 'PRINT_STR', flags, str });
  }

  handlePutc(node) {
    const args = node.arguments_?.arguments_ ?? [], str = this.evalStringLiteral(args[0]);
    if (str === null) return;
    if (str.length === 0) { this.error(args[0] ?? node, 'putc requires a non-empty string'); return; }
    this.emit({ op: 'PRINT_STR', flags: 0x00, str: [str[0]] });
  }

  handleRaise(node) {
    const args = node.arguments_?.arguments_ ?? [];
    if (args.length > 0) { const str = this.evalStringLiteral(args[0]); if (str === null) return; this.emit({ op: 'PRINT_STR', flags: 0x01, str }); }
    this.emit({ op: 'HALT', errorCode: 1 });
  }

  isQ16Expr(node) {
    if (!node) return false;
    const cn = node.constructor.name;
    if (cn === 'FloatNode' || cn === 'IntegerNode') return true;
    if (cn === 'GlobalVariableReadNode') return true;  // $var（EEPROM 変数）
    if (cn === 'LocalVariableReadNode') return node.name in this.regs || node.name in this.eeVars || node.name in (this.forVars ?? {}) || node.name in (this.defParams ?? {});
    if (cn === 'CallNode' && !node.receiver && !(node.arguments_?.arguments_?.length)) return node.name in this.regs || node.name in this.eeVars;
    if (cn === 'CallNode') {
      if (node.name === 'call' && node.receiver &&
          (node.receiver.constructor.name === 'LambdaNode' || this.lambdas[this.numericVarName(node.receiver) ?? ''])) return true;
      const OPS = ['+', '-', '*', '/'];
      return OPS.includes(node.name) && this.isQ16Expr(node.receiver) && this.isQ16Expr(node.arguments_?.arguments_?.[0]);
    }
    return false;
  }

  evalFloatConst(node) {
    if (!node) return null;
    const cn = node.constructor.name;
    if (cn === 'FloatNode')   return Number(node.value);
    if (cn === 'IntegerNode') return Number(node.value);
    if (cn === 'LocalVariableReadNode' && this.forVars?.[node.name] !== undefined) return this.forVars[node.name];
    if (cn === 'LocalVariableReadNode' && this.defParams?.[node.name] !== undefined) return this.defParams[node.name];
    if (cn === 'CallNode') {
      const OPS = { '+': (a,b)=>a+b, '-': (a,b)=>a-b, '*': (a,b)=>a*b, '/': (a,b)=>a/b };
      const op = OPS[node.name];
      if (op) { const l = this.evalFloatConst(node.receiver), r = this.evalFloatConst(node.arguments_?.arguments_?.[0]); if (l !== null && r !== null) return op(l, r); }
      // f.call(定数) — 引数が全部定数ならコンパイル時に畳み込む
      if (node.name === 'call' && node.receiver && this.lambdaDepth < 8) {
        const lam = (node.receiver.constructor.name !== 'LambdaNode') ? this.lambdaOf(node.receiver)
          : null;  // リテラル .call の定数畳み込みは emit 系経路に任せる（エラー重複防止）
        if (lam && (node.arguments_?.arguments_ ?? []).length === lam.params.length) {
          const vals = (node.arguments_?.arguments_ ?? []).map(a => this.evalFloatConst(a));
          if (!vals.some(v => v === null)) {
            const savedD = this.defParams, savedL = this.lambdaArgs;
            this.defParams = Object.fromEntries(lam.params.map((p, i) => [p, vals[i]]));
            this.lambdaArgs = null; this.lambdaDepth++;
            const r = this.evalFloatConst(lam.body);
            this.lambdaDepth--; this.defParams = savedD; this.lambdaArgs = savedL;
            return r;
          }
        }
      }
    }
    return null;
  }

  evalQ168(node) { const f = this.evalFloatConst(node); return f !== null ? Math.round(f * 256) : null; }

  handleNumericAssign(name, val, node) {
    // 全経路が LOAD_Q16 / Q16 演算を emit するため Q1 必須（sensor.read / rand / 三項 0,1 の代入は別経路で Q1 不要）
    if (!this.comps.Q1) { this.error(node, `数値変数への代入（${name} = 数値）には Q1 コンポーネントが必要です。チェックしてください。`); return; }
    // 配置先の決定: $名 → EEPROM（永続） / 既存レジスタ・既存 EEPROM → そのまま / 新規 → レジスタ優先、R0/R1 が埋まったら EEPROM
    let sdEntry = null;
    const isGlobal = name.startsWith('$');
    if (isGlobal || name in this.eeVars) {
      if (!this.comps.Ec && !this.comps.Ev) { this.error(node, `${name}: EEPROM 変数には Ev（または Ec）コンポーネントが必要です。チェックしてください。`); return; }
      if (!this.comps.Q1) { this.error(node, `${name}: 数値 EEPROM 変数には Q1 コンポーネントが必要です。チェックしてください。`); return; }
      sdEntry = this.eeVar(node, name, isGlobal ? 0x80 : 0x00, 1);
      if (!sdEntry) return;
    } else if (!(name in this.regs)) {
      const n = Object.keys(this.regs).length;
      if (n < 2) {
        this.regs[name] = n;
      } else {
        // R0/R1 が埋まった → EEPROM 変数（揮発・実行ごとに0初期化）
        if ((!this.comps.Ec && !this.comps.Ev) || !this.comps.Q1) { this.error(node, 'Q16.8: 数値変数は最大 2 つです (R0/R1)。3つ目以降は Ev（または Ec）+ Q1 を選択すると EEPROM 変数として使えます'); return; }
        sdEntry = this.eeVar(node, name, 0x00, 1);
        if (!sdEntry) return;
      }
    }
    const dstReg = sdEntry ? this.allocTempReg([]) : this.regs[name];
    const finish = () => { if (sdEntry) this.emit({ op: 'VAR_STORE', varIdx: sdEntry.idx, reg: dstReg }); };
    const constVal = this.evalQ168(val);
    if (constVal !== null) { this.emit({ op: 'LOAD_Q16', reg: dstReg, value: constVal }); finish(); return; }
    if (val.constructor.name === 'CallNode' && (val.name === '+' || val.name === '-')) {
      const rhsConst = this.evalQ168(val.arguments_?.arguments_?.[0]);
      if (rhsConst !== null) {
        const lhsName = this.numericVarName(val.receiver);
        const lhsSd = lhsName !== null ? this.eeVars[lhsName] : undefined;
        if (lhsSd) {
          this.emit({ op: 'VAR_LOAD', varIdx: lhsSd.idx, reg: dstReg });
        } else {
          const lhsReg = this.getVarReg(val.receiver);
          if (lhsReg === undefined) { this.error(node, 'Q16.8: 定数畳み込みできない式です。g = 0.1 * 0.5 や g = g + 0.1 の形式を使ってください'); return; }
          if (lhsReg !== dstReg) { this.emit({ op: 'LOAD_Q16', reg: dstReg, value: 0 }); this.emit({ op: 'ADD_Q16', dst: dstReg, src: lhsReg }); }
        }
        const tempReg = this.allocTempReg([dstReg]);
        this.emit({ op: 'LOAD_Q16', reg: tempReg, value: rhsConst });
        this.emit({ op: val.name === '+' ? 'ADD_Q16' : 'SUB_Q16', dst: dstReg, src: tempReg });
        finish(); return;
      }
    }
    // 一般式（idx = r * 24 + c, g = h * x 等）— evalExprToReg で評価して代入（Q1 必須）
    const er = this.evalExprToReg(val, []);
    if (er === undefined) return;
    if (sdEntry) { this.emit({ op: 'VAR_STORE', varIdx: sdEntry.idx, reg: er }); return; }
    this.emit({ op: 'LOAD_Q16', reg: dstReg, value: 0 });
    this.emit({ op: 'ADD_Q16', dst: dstReg, src: er });
  }

  evalInt(node) {
    if (!node) return null;
    if (node.constructor.name === 'IntegerNode') return Number(node.value);
    const f = this.evalFloatConst(node);
    if (f !== null) return Math.round(f);
    this.error(node, 'expected integer literal'); return null;
  }

  evalFloat(node) {
    if (!node) return null;
    const cn = node.constructor.name;
    if (cn === 'FloatNode')   return Number(node.value);
    if (cn === 'IntegerNode') return Number(node.value);
    if (cn === 'LocalVariableReadNode' && this.forVars?.[node.name] !== undefined) return this.forVars[node.name];
    if (cn === 'LocalVariableReadNode' && this.defParams?.[node.name] !== undefined) return this.defParams[node.name];
    this.error(node, 'expected numeric literal'); return null;
  }

  evalStringLiteral(node) {
    if (!node) return null;
    if (node.constructor.name === 'StringNode') {
      const s = this.stringNodeText(node);  // エスケープ処理込み（\n 等）
      if (typeof s !== 'string') return [];
      return Array.from(new TextEncoder().encode(s));  // UTF-8 バイト列
    }
    this.error(node, 'print/puts/p/putc supports string literals only in UIAPruby v1'); return null;
  }
}

// ============================================================
//  URB1 エンコーダー
// ============================================================
function encodeUrb1(instructions, eeVars) {
  const code = [];
  for (const ins of instructions) {
    switch (ins.op) {
      case 'END':         code.push(0x00); break;
      case 'WAIT_MS':     code.push(0x01, ins.ms & 0xFF, (ins.ms >> 8) & 0xFF); break;
      case 'WAIT_MS_REG': code.push(0x12, ins.reg ?? 0, ins.mul & 0xFF, (ins.mul >> 8) & 0xFF); break;
      case 'GPIO_MODE':   code.push(0x02, ins.pin, ins.mode); break;
      case 'GPIO_WRITE':  code.push(0x03, ins.pin, ins.value); break;
      case 'GPIO_READ':   code.push(0x04, ins.pin, ins.reg ?? 0); break;
      case 'TONE_FREQ':    code.push(0x05, ins.pin, ins.freq & 0xFF, (ins.freq >> 8) & 0xFF); break;
      case 'JMP':         code.push(0x06, ins.relOffset & 0xFF, (ins.relOffset >> 8) & 0xFF); break;
      case 'JZ':          code.push(0x07, ins.reg ?? 0, ins.relOffset & 0xFF, (ins.relOffset >> 8) & 0xFF); break;
      case 'JNZ':         code.push(0x08, ins.reg ?? 0, ins.relOffset & 0xFF, (ins.relOffset >> 8) & 0xFF); break;
      case 'GPIO_TOGGLE': code.push(0x09, ins.pin); break;
      case 'LOAD_Q16':    code.push(0x0A, ins.reg, ins.value & 0xFF, (ins.value >> 8) & 0xFF, (ins.value >> 16) & 0xFF, (ins.value >> 24) & 0xFF); break;
      case 'ADD_Q16':     code.push(0x0B, ins.dst, ins.src); break;
      case 'SUB_Q16':     code.push(0x0C, ins.dst, ins.src); break;
      case 'MUL_Q16':     code.push(0x0D, ins.dst, ins.src); break;
      case 'DIV_Q16':     code.push(0x0E, ins.dst, ins.src); break;
      case 'CMP_LT_Q16':  code.push(0x0F, ins.lhs, ins.rhs, ins.out); break;
      case 'CMP_GT_Q16':  code.push(0x10, ins.lhs, ins.rhs, ins.out); break;
      case 'CMP_EQ_Q16':  code.push(0x11, ins.lhs, ins.rhs, ins.out); break;
      case 'LOAD_BOOL':   code.push(0x15, ins.reg, ins.value ? 1 : 0); break;
      case 'PRINT_STR':   code.push(0x16, ins.flags, ins.str.length, ...ins.str); break;
      case 'HALT':        code.push(0x17, ins.errorCode ?? 1); break;
      case 'ADC_READ':        code.push(0x18, ins.pin, ins.reg ?? 0); break;
      case 'PRINT_REG':       code.push(0x19, ins.flags, ins.reg ?? 0); break;
      case 'ULTRASONIC_READ': code.push(0x1A, ins.trig, ins.echo, ins.reg ?? 0); break;
      case 'WARN_REG':        code.push(0x32, ins.reg ?? 0); break;
      case 'RAND':          code.push(0x21, ins.min & 0xFF, ins.max & 0xFF, ins.reg ?? 0); break;
      case 'SRAND':         code.push(0x22, ins.seed & 0xFF, (ins.seed >> 8) & 0xFF); break;
      case 'EVERY_MS':      code.push(0x47, ins.slot & 7, ins.ms & 0xFF, (ins.ms >> 8) & 0xFF); break;
      case 'TIMER_RESET':   code.push(0x48, ins.slot & 7); break;
      case 'TIMER_MS':      code.push(0x49, ins.slot & 7, ins.reg ?? 0); break;
      case 'TIMER_US':      code.push(0x4A, ins.slot & 7, ins.reg ?? 0); break;
      case 'PWM_DUTY':      code.push(0x23, ins.pin, ins.duty & 0xFF); break;
      case 'PWM_DUTY_REG':  code.push(0x2A, ins.pin, ins.reg ?? 0); break;
      case 'PWM_BASE_FREQ': code.push(0x24, ins.pin, ins.freq & 0xFF, (ins.freq >> 8) & 0xFF); break;
      case 'VAR_LOAD':      code.push(0x25, ins.varIdx, ins.reg ?? 0); break;
      case 'VAR_STORE':     code.push(0x26, ins.varIdx, ins.reg ?? 0); break;
      case 'VAR_LOAD_IDX':  code.push(0x27, ins.varIdx, ins.idxReg ?? 0, ins.reg ?? 0); break;
      case 'VAR_STORE_IDX': code.push(0x28, ins.varIdx, ins.idxReg ?? 0, ins.reg ?? 0); break;
      case 'TO_S':          code.push(0x29, ins.reg ?? 0, ins.varIdx); break;
      case 'VAR_STR_SET':     code.push(0x2B, ins.varIdx, ins.str.length, ...ins.str); break;
      case 'VAR_STR_COPY':    code.push(0x2C, ins.dstIdx, ins.srcIdx); break;
      case 'VAR_STR_CAT':     code.push(0x2D, ins.dstIdx, ins.srcIdx); break;
      case 'VAR_STR_CAT_LIT': code.push(0x2E, ins.dstIdx, ins.str.length, ...ins.str); break;
      case 'VAR_PRINT':       code.push(0x2F, ins.flags, ins.varIdx); break;
      case 'VAR_STR_CMP':     code.push(0x30, ins.varIdx, ins.out, ins.str.length, ...ins.str); break;
      case 'VAR_STR_CMP_V':   code.push(0x31, ins.aIdx, ins.bIdx, ins.out); break;
      case 'I2C_MASTER_INIT': code.push(0x1E); break;
      case 'I2C_MASTER_GET':  code.push(0x1F, ins.addr, ins.reg, ins.dstReg ?? 0); break;
      case 'I2C_MASTER_SET':  code.push(0x20, ins.addr, ins.reg, ins.srcReg ?? 0); break;
      // Se: UART（0x33〜0x3B）
      case 'SERIAL_BEGIN':     code.push(0x33, ins.baud & 0xFF, (ins.baud >> 8) & 0xFF, (ins.baud >> 16) & 0xFF, (ins.baud >> 24) & 0xFF); break;
      case 'SERIAL_WRITE':     code.push(0x34, ins.val & 0xFF); break;
      case 'SERIAL_PRINT': {
        const bytes = new TextEncoder().encode(ins.text ?? '');
        if (bytes.length > 63) throw new Error(`ser.print の文字列は最大 63 バイトです（現在 ${bytes.length} バイト）`);
        code.push(0x35, ins.flags & 0xFF, bytes.length, ...bytes); break;
      }
      case 'SERIAL_AVAILABLE': code.push(0x36, ins.reg ?? 0); break;
      case 'SERIAL_READ':      code.push(0x37, ins.reg ?? 0); break;
      case 'SERIAL_WRITE_REG': code.push(0x38, ins.reg ?? 0); break;
      case 'SERIAL_READ_LINE': code.push(0x39, ins.varIdx ?? 0, ins.delim ?? 10, ins.timeout & 0xFF, (ins.timeout >> 8) & 0xFF); break;
      case 'SERIAL_PRINT_REG': code.push(0x3A, ins.flags & 0xFF, ins.reg ?? 0); break;
      case 'SERIAL_PRINT_VAR': code.push(0x3B, ins.flags & 0xFF, ins.varIdx ?? 0); break;
      // Np: NeoPixel（0x3C〜0x45）
      case 'NEO_BEGIN':      code.push(0x3C, ins.count & 0xFF); break;
      case 'NEO_SHOW':       code.push(0x3D); break;
      case 'NEO_BRIGHTNESS': code.push(0x3E, ins.val & 0xFF); break;
      case 'NEO_SET_RGB':    code.push(0x3F, ins.idx, ins.r, ins.g, ins.b); break;
      case 'NEO_FILL':       code.push(0x40, ins.start, ins.count, ins.r, ins.g, ins.b); break;
      case 'NEO_CLEAR':      code.push(0x41); break;
      case 'NEO_RAINBOW':    code.push(0x42, ins.offset & 0xFF); break;
      case 'NEO_SHIFT':      code.push(0x43, ins.step & 0xFF); break;
      case 'NEO_SHIFT_REG':  code.push(0x4B, ins.reg & 3); break;
      case 'NEO_ORDER':      code.push(0x4C, ins.pack & 0x3F); break;
      case 'NEO_DIM':        code.push(0x44, ins.scale & 0xFF); break;
      case 'NEO_SET_HSV':    code.push(0x45, ins.idx, ins.h, ins.s, ins.v); break;
    case 'NEO_AUTO':       code.push(0x46, ins.on); break;
    }
  }
  const codeSize = code.length;
  // ガード: instrSize 合計とエンコード結果が食い違うとジャンプオフセットが壊れる（fall-through 削除事故等）
  const expected = instructions.reduce((s, i) => s + instrSize(i), 0);
  if (codeSize !== expected) {
    throw new Error(`内部エラー: 命令サイズ表 (${expected}B) とエンコード結果 (${codeSize}B) が不一致です。instrSize とエンコーダの定義を確認してください`);
  }
  // EEPROM 変数があれば URB1 v2 ヘッダ（変数メタデータ付き）
  const vars = Object.values(eeVars ?? {}).sort((a, b) => a.idx - b.idx);
  const meta = [];
  if (vars.length) {
    meta.push(vars.length);
    for (const v of vars) {
      meta.push(v.type & 0xFF, v.count & 0xFF, (v.count >> 8) & 0xFF);
      for (const ch of v.name) meta.push(ch.charCodeAt(0) & 0xFF);
      meta.push(0);
    }
  }
  return new Uint8Array([
    0x55, 0x52, 0x42, 0x31,  // "URB1"
    vars.length ? 0x02 : 0x01, 0x00,
    codeSize & 0xFF, (codeSize >> 8) & 0xFF,
    ...meta,
    ...code
  ]);
}

function formatInstructions(instructions) {
  const MODES = ['INPUT','OUTPUT','INPUT_PULLUP','INPUT_PULLDOWN'];
  const REGS  = ['R0','R1','R2','R3'];
  let offset = 0;
  return instructions.map(ins => {
    const addr = offset.toString(16).toUpperCase().padStart(4,'0');
    let line;
    switch (ins.op) {
      case 'END':         line = `${addr}  END`; break;
      case 'WAIT_MS':     line = `${addr}  WAIT_MS ${ins.ms}`; break;
      case 'WAIT_MS_REG': line = `${addr}  WAIT_MS_REG ${REGS[ins.reg??0]} x ${ins.mul}ms`; break;
      case 'GPIO_MODE':   line = `${addr}  GPIO_MODE pin=${ins.pin} ${MODES[ins.mode]??ins.mode}`; break;
      case 'GPIO_WRITE':  line = `${addr}  GPIO_WRITE pin=${ins.pin} value=${ins.value}`; break;
      case 'GPIO_READ':   line = `${addr}  GPIO_READ pin=${ins.pin} -> ${REGS[ins.reg??0]}`; break;
      case 'GPIO_TOGGLE': line = `${addr}  GPIO_TOGGLE pin=${ins.pin}`; break;
      case 'TONE_FREQ':    line = `${addr}  TONE_FREQ pin=${ins.pin} freq=${ins.freq}`; break;
      case 'JMP':         line = `${addr}  JMP ${ins.relOffset>=0?'+':''}${ins.relOffset}`; break;
      case 'JZ':          line = `${addr}  JZ ${REGS[ins.reg??0]}, ${ins.relOffset>=0?'+':''}${ins.relOffset}`; break;
      case 'JNZ':         line = `${addr}  JNZ ${REGS[ins.reg??0]}, ${ins.relOffset>=0?'+':''}${ins.relOffset}`; break;
      case 'LOAD_Q16':    line = `${addr}  LOAD_Q16 ${REGS[ins.reg]}, ${ins.value} (≈${(ins.value/256).toFixed(3)})`; break;
      case 'ADD_Q16':     line = `${addr}  ADD_Q16 ${REGS[ins.dst]}, ${REGS[ins.src]}`; break;
      case 'SUB_Q16':     line = `${addr}  SUB_Q16 ${REGS[ins.dst]}, ${REGS[ins.src]}`; break;
      case 'MUL_Q16':     line = `${addr}  MUL_Q16 ${REGS[ins.dst]}, ${REGS[ins.src]}`; break;
      case 'DIV_Q16':     line = `${addr}  DIV_Q16 ${REGS[ins.dst]}, ${REGS[ins.src]}`; break;
      case 'CMP_LT_Q16': case 'CMP_GT_Q16': case 'CMP_EQ_Q16':
        line = `${addr}  ${ins.op} ${REGS[ins.lhs]}, ${REGS[ins.rhs]} -> ${REGS[ins.out]}`; break;
      case 'LOAD_BOOL':   line = `${addr}  LOAD_BOOL ${REGS[ins.reg]}, ${ins.value?1:0}`; break;
      case 'PRINT_STR': {
        const disp = new TextDecoder().decode(new Uint8Array(ins.str))
          .replace(/\\/g, '\\\\').replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/\t/g, '\\t');
        line = `${addr}  PRINT_STR flags=0x${ins.flags.toString(16).padStart(2,'0')} "${disp}"`; break;
      }
      case 'HALT':        line = `${addr}  HALT ${ins.errorCode??1}`; break;
      case 'ADC_READ':        line = `${addr}  ADC_READ pin=${ins.pin} -> ${REGS[ins.reg??0]}`; break;
      case 'ULTRASONIC_READ': line = `${addr}  ULTRASONIC_READ trig=${ins.trig} echo=${ins.echo} -> ${REGS[ins.reg??0]}`; break;
      case 'WARN_REG':        line = `${addr}  WARN_REG ${REGS[ins.reg??0]}`; break;
      case 'PRINT_REG':       line = `${addr}  PRINT_REG flags=0x${ins.flags.toString(16).padStart(2,'0')} ${REGS[ins.reg??0]}`; break;
      case 'RAND':          line = `${addr}  RAND min=${ins.min} max=${ins.max} -> ${REGS[ins.reg??0]}`; break;
      case 'SRAND':         line = `${addr}  SRAND seed=${ins.seed}`; break;
      case 'EVERY_MS':      line = `${addr}  EVERY_MS s${ins.slot} ${ins.ms}ms`; break;
      case 'TIMER_RESET':   line = `${addr}  TIMER_RESET s${ins.slot}`; break;
      case 'TIMER_MS':      line = `${addr}  TIMER_MS s${ins.slot} -> ${REGS[ins.reg??0]}`; break;
      case 'TIMER_US':      line = `${addr}  TIMER_US s${ins.slot} -> ${REGS[ins.reg??0]}`; break;
      case 'PWM_DUTY':      line = `${addr}  PWM_DUTY pin=${ins.pin} duty=${ins.duty}`; break;
      case 'PWM_DUTY_REG':  line = `${addr}  PWM_DUTY_REG pin=${ins.pin} ${REGS[ins.reg??0]}`; break;
      case 'PWM_BASE_FREQ': line = `${addr}  PWM_BASE_FREQ pin=${ins.pin} freq=${ins.freq}`; break;
      case 'VAR_LOAD':      line = `${addr}  VAR_LOAD var[${ins.varIdx}] -> ${REGS[ins.reg??0]}`; break;
      case 'VAR_STORE':     line = `${addr}  VAR_STORE var[${ins.varIdx}] <- ${REGS[ins.reg??0]}`; break;
      case 'VAR_LOAD_IDX':  line = `${addr}  VAR_LOAD_IDX var[${ins.varIdx}][${REGS[ins.idxReg??0]}] -> ${REGS[ins.reg??0]}`; break;
      case 'VAR_STORE_IDX': line = `${addr}  VAR_STORE_IDX var[${ins.varIdx}][${REGS[ins.idxReg??0]}] <- ${REGS[ins.reg??0]}`; break;
      case 'TO_S':          line = `${addr}  TO_S ${REGS[ins.reg??0]} -> var[${ins.varIdx}]`; break;
      case 'VAR_STR_SET':     line = `${addr}  VAR_STR_SET var[${ins.varIdx}] = "${new TextDecoder().decode(new Uint8Array(ins.str))}"`; break;
      case 'VAR_STR_COPY':    line = `${addr}  VAR_STR_COPY var[${ins.dstIdx}] <- var[${ins.srcIdx}]`; break;
      case 'VAR_STR_CAT':     line = `${addr}  VAR_STR_CAT var[${ins.dstIdx}] << var[${ins.srcIdx}]`; break;
      case 'VAR_STR_CAT_LIT': line = `${addr}  VAR_STR_CAT_LIT var[${ins.dstIdx}] << "${new TextDecoder().decode(new Uint8Array(ins.str))}"`; break;
      case 'VAR_PRINT':       line = `${addr}  VAR_PRINT flags=0x${ins.flags.toString(16).padStart(2,'0')} var[${ins.varIdx}]`; break;
      case 'VAR_STR_CMP':     line = `${addr}  VAR_STR_CMP var[${ins.varIdx}] == "${new TextDecoder().decode(new Uint8Array(ins.str))}" -> ${REGS[ins.out]}`; break;
      case 'VAR_STR_CMP_V':   line = `${addr}  VAR_STR_CMP_V var[${ins.aIdx}] == var[${ins.bIdx}] -> ${REGS[ins.out]}`; break;
      case 'SERIAL_BEGIN':     line = `${addr}  SERIAL_BEGIN ${ins.baud} baud`; break;
      case 'SERIAL_WRITE':     line = `${addr}  SERIAL_WRITE ${ins.val}`; break;
      case 'SERIAL_WRITE_REG': line = `${addr}  SERIAL_WRITE_REG ${REGS[ins.reg??0]}`; break;
      case 'SERIAL_PRINT':     line = `${addr}  SERIAL_PRINT ${JSON.stringify(ins.text ?? '')}${(ins.flags&1)?' +CRLF':''}`; break;
      case 'SERIAL_AVAILABLE': line = `${addr}  SERIAL_AVAILABLE -> ${REGS[ins.reg??0]}`; break;
      case 'SERIAL_READ':      line = `${addr}  SERIAL_READ -> ${REGS[ins.reg??0]}`; break;
      case 'SERIAL_READ_LINE': line = `${addr}  SERIAL_READ_LINE var[${ins.varIdx}] delim=${ins.delim} timeout=${ins.timeout}ms`; break;
      case 'SERIAL_PRINT_REG': line = `${addr}  SERIAL_PRINT_REG ${REGS[ins.reg??0]}`; break;
      case 'SERIAL_PRINT_VAR': line = `${addr}  SERIAL_PRINT_VAR var[${ins.varIdx}]`; break;
      case 'NEO_BEGIN':        line = `${addr}  NEO_BEGIN ${ins.count} LEDs`; break;
      case 'NEO_SHOW':         line = `${addr}  NEO_SHOW`; break;
      case 'NEO_CLEAR':        line = `${addr}  NEO_CLEAR`; break;
      case 'NEO_BRIGHTNESS':   line = `${addr}  NEO_BRIGHTNESS ${ins.val}`; break;
      case 'NEO_SET_RGB':      line = `${addr}  NEO_SET_RGB [${ins.idx}] = (${ins.r},${ins.g},${ins.b})`; break;
      case 'NEO_SET_HSV':      line = `${addr}  NEO_SET_HSV [${ins.idx}] = h${ins.h} s${ins.s} v${ins.v}`; break;
    case 'NEO_AUTO':         line = `${addr}  NEO_AUTO ${ins.on ? 'on' : 'off'}`; break;
      case 'NEO_FILL':         line = `${addr}  NEO_FILL (${ins.r},${ins.g},${ins.b})`; break;
      case 'NEO_RAINBOW':      line = `${addr}  NEO_RAINBOW offset=${ins.offset}`; break;
      case 'NEO_SHIFT':        line = `${addr}  NEO_SHIFT ${ins.step}${ins.raw !== ins.step ? ` (${ins.raw} を正規化)` : ''}`; break;
      case 'NEO_SHIFT_REG':    line = `${addr}  NEO_SHIFT_REG ${REGS[ins.reg ?? 0]}（歩数は実行時に正規化）`; break;
      case 'NEO_ORDER':        line = `${addr}  NEO_ORDER ${ins.order.join(',')}（赤→${ins.seen.split(',')[0]} 緑→${ins.seen.split(',')[1]} に見える）`; break;
      case 'NEO_DIM':          line = `${addr}  NEO_DIM ${ins.percent}% (x${ins.scale + 1}/256)`; break;
      case 'I2C_MASTER_INIT': line = `${addr}  I2C_MASTER_INIT`; break;
      case 'I2C_MASTER_GET':  line = `${addr}  I2C_MASTER_GET addr=0x${ins.addr.toString(16).toUpperCase().padStart(2,'0')} reg[${ins.reg}] -> ${REGS[ins.dstReg??0]}`; break;
      case 'I2C_MASTER_SET':  line = `${addr}  I2C_MASTER_SET addr=0x${ins.addr.toString(16).toUpperCase().padStart(2,'0')} reg[${ins.reg}] <- ${REGS[ins.srcReg??0]}`; break;
      default:              line = `${addr}  ???`; break;
    }
    offset += instrSize(ins);
    return line;
  }).join('\n');
}

function estimateTime(instructions) {
  // EVERY_MS も実時間を消費する（every_ms 1000 は sleep(1.0) と同じ 1 周 1000ms）
  return instructions.reduce((s,i) => (i.op === 'WAIT_MS' || i.op === 'EVERY_MS') ? s + i.ms : s, 0);
}

export {
  Compiler, encodeUrb1,
  instrSize, instrOffset,
  formatInstructions, estimateTime,
  lineFromOffset, neoMaxLeds, NEO_LED_CHOICES,
};

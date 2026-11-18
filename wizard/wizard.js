'use strict';
// Shared by index.html (Version A, main) and flex.html (Version B, flexible).
// Both pages use the same setup (names, keys, sound files); each keeps its own
// world on/off, color state and session log.
const VERSION = window.WIZARD_VERSION === 'B' ? 'B' : 'A';
const FLEX = VERSION === 'B';
const OTHER = FLEX ? {href: 'index.html', label: 'Switch to Version A (main)'} : {href: 'flex.html', label: 'Switch to Version B (flexible)'};

const $ = id => document.getElementById(id);
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const R = Math.random;
const SR = 44100;
const uid = p => p + Math.random().toString(36).slice(2, 8);
const keyLabel = k => k === ' ' ? 'Space' : (k || '').toUpperCase();
const esc = s => String(s ?? '').replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));
function toast(msg){ const t = $('toast'); t.textContent = msg; t.classList.add('on'); clearTimeout(toast.h); toast.h = setTimeout(() => t.classList.remove('on'), 2800); }
function load(k, d){ try{ const v = localStorage.getItem('tuiwiz2_' + k); return v ? JSON.parse(v) : d; }catch(e){ return d; } }
function save(k, v){ try{ localStorage.setItem('tuiwiz2_' + k, JSON.stringify(v)); }catch(e){ toast('Could not save: ' + e.message); } }

// =====================================================================
// Page
// =====================================================================
document.body.innerHTML = `
<div class="overlay" id="startOverlay"><div class="card">
  <h1 style="margin-bottom:8px">TUI Shoes <span>wizard · Version ${VERSION}${FLEX ? ' (flexible)' : ''}</span></h1>
  <p class="help">Follow the wearer with this laptop and press the matching key or button. Browsers only allow sound after a click, so start here.</p>
  <button class="primary" id="startBtn">Start sound</button>
  <p class="help" id="startMsg"></p>
</div></div>

<header>
  <h1>TUI Shoes <span>wizard · Version ${VERSION}${FLEX ? ' (flexible)' : ' (main)'}</span></h1>
  <nav class="tabs"><button data-tab="perform" class="on">Perform</button><button data-tab="setup">Setup</button></nav>
  <a class="vlink" href="${OTHER.href}">${OTHER.label}</a>
  <label class="help">Master <input type="range" id="masterVol" min="0" max="100" style="vertical-align:middle;width:110px"></label>
  <button class="panic" id="panicBtn" title="Stops every sound">STOP ALL <kbd>Esc</kbd></button>
</header>

<main id="perform">
  <section class="panel full">
    <h2>Background world
      <span class="tag">${FLEX
        ? 'plays under everything. Tap Walk or Run on every step: faster taps = louder. No taps for 2 s = back to 25%.'
        : 'plays quietly under everything at a steady level. Scanning a color switches it to that color.'}</span>
      ${FLEX ? '' : '<button class="small" id="clearColor" style="margin-left:auto">Back to default world</button>'}</h2>
    <div class="world">
      <div class="swatch" id="worldSwatch"></div>
      <div>
        <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap">
          <b style="font-size:18px" id="worldName">-</b>
          <span class="badge" id="worldState">ON</span>
        </div>
        <div class="bar" style="margin-top:8px"><i id="worldBar"></i></div>
        <div class="kv"><span>Level <b id="worldLevel">0%</b></span>${FLEX ? '<span>Pace <b id="worldPace">-</b></span>' : ''}<span id="worldDuck"></span></div>
      </div>
    </div>
    <div class="pads world" id="worldPads" style="margin-top:12px"></div>
  </section>

  ${FLEX ? `<section class="panel full">
    <h2>Color library <span class="tag">empty at the start. Each scan adds its color (max <span id="libMax"></span>, oldest out). Spin moves to the next one.</span>
      <button class="small" id="libClear" style="margin-left:auto">Empty the library</button></h2>
    <div class="lib" id="lib"></div>
  </section>` : ''}

  <section class="panel full">
    <h2>Movements <span class="tag">one sound at a time: a new movement cuts the last one. Drop an audio file on a button to give it a sound.</span></h2>
    <div class="pads moves" id="movePads"></div>
  </section>

  <section class="panel full">
    <h2>Scan a color <span class="tag">toe touches an object. Drop an audio file on a color to give it a sound.</span></h2>
    <div class="pads colors" id="colorPads"></div>
  </section>

  <section class="panel full">
    <h2>Session</h2>
    <div class="sessbar">
      <button class="primary" id="sessStart">Start session (time zero)</button>
      <span class="clock" id="sessClock">--:--.-</span>
      <input type="text" id="sessName" placeholder="participant / session name" style="min-width:220px">
      <button id="sessCsv">Export CSV</button>
      <span class="help" id="sessInfo"></span>
    </div>
    <div class="sessgrid">
      <div>
        <h3>Notes</h3>
        <textarea id="sessNotes" placeholder="Anything about this session. Saved with the CSV."></textarea>
        <div style="display:flex;gap:8px;margin-top:8px">
          <input type="text" id="quickNote" placeholder="Quick note, e.g. 'laughed', then Enter" style="flex:1">
          <button id="quickNoteBtn">Add</button>
        </div>
        ${FLEX ? '<label class="help" style="display:block;margin-top:8px"><input type="checkbox" id="logEveryStep"> log every step tap (otherwise only idle / walk / run changes)</label>' : ''}
      </div>
      <div>
        <h3>Log <span id="logCount"></span></h3>
        <div class="logwrap"><table class="log"><thead><tr><th>Time</th><th>Movement</th><th>Sound</th><th>Detail</th></tr></thead><tbody id="logBody"></tbody></table></div>
      </div>
    </div>
  </section>
</main>

<main id="setup" class="hidden">
  <section class="panel full">
    <h2>Sounds and inputs <span class="tag">shared by Version A and Version B. Drop an audio file on a Sound cell, or use "choose file". Saved in this browser.</span></h2>
    <div style="overflow-x:auto"><table class="setup" id="setupTable"></table></div>
    <div class="btns">
      <button id="addMove">+ Add movement</button>
      <button id="addColor">+ Add color</button>
      <button id="fillTest">Fill empty slots with test sounds</button>
    </div>
    <p class="help">Loop = keeps playing until another movement or Stand still (good for Walk and Run). Without loop the sound plays once.
      ${FLEX ? 'In this version Walk and Run are tap keys that set the world volume, so their sounds are not used here.' : ''}
      <kbd>Esc</kbd> is always STOP ALL. Click a key, then press the new key.</p>
  </section>

  <section class="panel">
    <h2>Background world</h2>
    <div class="grid2" id="worldSettings"></div>
  </section>

  <section class="panel">
    <h2>Save / load this setup</h2>
    <div class="btns" style="margin-top:0">
      <button id="exportCfg">Export setup (with sound files)</button>
      <label class="filelab" style="align-self:center"><button type="button" onclick="this.nextElementSibling.click()">Import setup</button><input type="file" id="importCfg" accept=".json,application/json"></label>
      <button id="resetCfg">Reset to defaults</button>
    </div>
    <p class="help">The export is one .json file with every sound file inside, so the whole setup moves to another laptop.</p>
  </section>
</main>

<div class="flash" id="flash"></div>
<div class="toast" id="toast"></div>`;

// =====================================================================
// Test sounds (made in the browser) for trying things before real files exist
// =====================================================================
function noiseBuf(oc, sec, brown){
  const b = oc.createBuffer(1, Math.round(oc.sampleRate * sec), oc.sampleRate), d = b.getChannelData(0);
  let last = 0;
  for (let i = 0; i < d.length; i++){ const w = R() * 2 - 1; if (brown){ last = (last + 0.02 * w) / 1.02; d[i] = last * 3.5; } else d[i] = w; }
  return b;
}
function out(oc, nodes){ for (let i = 0; i < nodes.length - 1; i++) nodes[i].connect(nodes[i + 1]); nodes[nodes.length - 1].connect(oc.destination); return nodes[0]; }
function filt(oc, type, f, q){ const n = oc.createBiquadFilter(); n.type = type; n.frequency.value = f; if (q) n.Q.value = q; return n; }
function gainN(oc, v){ const g = oc.createGain(); g.gain.value = v; return g; }
function panN(oc, p){ const n = oc.createStereoPanner(); n.pan.value = clamp(p, -1, 1); return n; }
function envG(oc, t, peak, attack, decay){
  const g = oc.createGain(); g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(peak, t + attack); g.gain.exponentialRampToValueAtTime(0.0001, t + attack + decay); return g;
}
function bufSrc(oc, buf){ const s = oc.createBufferSource(); s.buffer = buf; return s; }
function burst(oc, nb, t, dur, peak, type, freq, q, p = 0){
  const s = bufSrc(oc, nb); out(oc, [s, filt(oc, type, freq, q), envG(oc, t, peak, 0.001, dur), panN(oc, p)]);
  s.start(t, R() * 0.4); s.stop(t + dur + 0.03);
}
function tone(oc, {type = 'sine', f, f2, t, dur, peak, attack = 0.005, p = 0}){
  const o = oc.createOscillator(); o.type = type; o.frequency.setValueAtTime(f, t);
  if (f2) o.frequency.exponentialRampToValueAtTime(f2, t + attack + dur * 0.7);
  out(oc, [o, envG(oc, t, peak, attack, dur), panN(oc, p)]); o.start(t); o.stop(t + attack + dur + 0.05);
}
function noiseBed(oc, sec, type, f, q, level, brown){ const s = bufSrc(oc, noiseBuf(oc, sec + 1, brown)); out(oc, [s, filt(oc, type, f, q), gainN(oc, level)]); s.start(0); }
function curve(sec, fn, n = 600){ const c = new Float32Array(n); for (let k = 0; k < n; k++) c[k] = fn(k / (n - 1) * sec); return c; }
const midi = m => 440 * Math.pow(2, (m - 69) / 12);
function footstep(oc, nb, t, peak, p){
  burst(oc, nb, t, 0.12, peak, 'lowpass', 260, 0.8, p);
  burst(oc, nb, t + 0.01, 0.05, peak * 0.4, 'bandpass', 1500, 1, p);
  burst(oc, nb, t + 0.02, 0.03, peak * 0.15, 'highpass', 3000, 0.7, p);
}
// blend the end into the start so an ambient loop has no click
function loopable(buf, fade = 0.3){
  const N = Math.round(fade * buf.sampleRate), L = buf.length - N;
  const o = new AudioBuffer({numberOfChannels: buf.numberOfChannels, length: L, sampleRate: buf.sampleRate});
  for (let c = 0; c < buf.numberOfChannels; c++){
    const s = buf.getChannelData(c), d = o.getChannelData(c); d.set(s.subarray(0, L));
    for (let i = 0; i < N; i++){ const a = Math.sin(i / N * Math.PI / 2), b = Math.cos(i / N * Math.PI / 2); d[i] = s[i] * a + s[L + i] * b; }
  }
  return o;
}

const BUILTINS = {
  walk:    {label: 'Footsteps (walk)', sec: 4, make(oc){ const nb = noiseBuf(oc, 1); for (let i = 0; i < 8; i++) footstep(oc, nb, 0.02 + i * 0.5, 0.7, i % 2 ? 0.3 : -0.3); }},
  run:     {label: 'Footsteps (run)', sec: 2.4, make(oc){ const nb = noiseBuf(oc, 1); for (let i = 0; i < 8; i++) footstep(oc, nb, 0.02 + i * 0.3, 0.95, i % 2 ? 0.3 : -0.3); }},
  skip:    {label: 'Skipping boings', sec: 2.6, make(oc){
    const nb = noiseBuf(oc, 1);
    for (let i = 0; i < 4; i++){ const t = 0.02 + i * 0.6, p = i % 2 ? 0.3 : -0.3;
      footstep(oc, nb, t, 0.4, p); footstep(oc, nb, t + 0.18, 0.8, p); tone(oc, {f: 300, f2: 700, t: t + 0.18, dur: 0.18, peak: 0.25, p}); }
  }},
  fire:    {label: 'Campfire', crossfade: true, sec: 8, make(oc, sec){
    noiseBed(oc, sec, 'lowpass', 500, 0, 0.55, true);
    const nb = noiseBuf(oc, 1);
    for (let i = 0; i < 380; i++) burst(oc, nb, R() * sec, 0.002 + R() * 0.012, 0.03 + R() ** 3 * 0.6, 'bandpass', 800 + R() * 3500, 2, R() * 1.2 - 0.6);
  }},
  birds:   {label: 'Sunny birds', crossfade: true, sec: 8, make(oc, sec){
    noiseBed(oc, sec, 'highpass', 2500, 0.7, 0.015);
    for (let i = 0; i < 24; i++){
      const t = R() * (sec - 0.6), base = 2600 + R() * 2000, n = 2 + Math.floor(R() * 4), p = R() * 1.6 - 0.8;
      for (let j = 0; j < n; j++) tone(oc, {f: base * (0.9 + R() * 0.2), f2: base * (1.3 + R() * 0.4), t: t + j * 0.09, dur: 0.06, peak: 0.07 + R() * 0.06, attack: 0.004, p});
    }
  }},
  ocean:   {label: 'Ocean waves', crossfade: true, sec: 8, make(oc, sec){
    [[-0.5, 0], [0.5, 0.5]].forEach(([p, off]) => {
      const s1 = bufSrc(oc, noiseBuf(oc, sec + 1)), g1 = oc.createGain();
      g1.gain.setValueCurveAtTime(curve(sec, t => 0.06 + 0.55 * Math.sin(Math.PI * (((t % 4) / 4 + off / 4) % 1)) ** 2), 0, sec);
      out(oc, [s1, filt(oc, 'lowpass', 650), g1, panN(oc, p)]); s1.start(0);
    });
  }},
  wind:    {label: 'Soft wind', crossfade: true, sec: 8, make(oc, sec){
    [[-0.5, 0], [0.5, 1.7]].forEach(([p, ph]) => {
      const s = bufSrc(oc, noiseBuf(oc, sec + 1)), bp = filt(oc, 'bandpass', 600, 1.2), g = oc.createGain();
      bp.frequency.setValueCurveAtTime(curve(sec, t => 350 + 500 * (0.5 + 0.5 * Math.sin(2 * Math.PI * t / 8 + ph))), 0, sec);
      g.gain.setValueCurveAtTime(curve(sec, t => 0.12 + 0.25 * (0.5 + 0.5 * Math.sin(2 * Math.PI * t / 4 + ph))), 0, sec);
      out(oc, [s, bp, g, panN(oc, p)]); s.start(0);
    });
  }},
  stomp:   {label: 'Giant stomp', sec: 2.8, make(oc){
    const nb = noiseBuf(oc, 1);
    tone(oc, {f: 75, f2: 30, t: 0.02, dur: 1.2, peak: 1.0, attack: 0.005});
    burst(oc, nb, 0.01, 0.5, 1.0, 'lowpass', 150, 0.7);
    for (let i = 0; i < 30; i++){ const t = 0.08 + R() * 0.9; burst(oc, nb, t, 0.01 + R() * 0.03, (0.05 + R() * 0.15) * (1 - t), 'bandpass', 900 + R() * 2500, 1.5, R() * 1.4 - 0.7); }
  }},
  wave:    {label: 'Crashing wave', sec: 5, make(oc){
    const s1 = bufSrc(oc, noiseBuf(oc, 5)), lp = filt(oc, 'lowpass', 300), g1 = oc.createGain();
    lp.frequency.setValueAtTime(300, 0); lp.frequency.linearRampToValueAtTime(2800, 1.7);
    g1.gain.setValueAtTime(0.02, 0); g1.gain.linearRampToValueAtTime(0.6, 1.7); g1.gain.exponentialRampToValueAtTime(0.0001, 3.2);
    out(oc, [s1, lp, g1]); s1.start(0);
    const s2 = bufSrc(oc, noiseBuf(oc, 5)); out(oc, [s2, filt(oc, 'highpass', 150), envG(oc, 1.6, 1.0, 0.1, 3.2)]); s2.start(0);
    tone(oc, {f: 65, f2: 40, t: 1.65, dur: 1.0, peak: 0.6});
  }},
  ballerina: {label: 'Ballerina music box', sec: 8.5, make(oc){
    const mel = [76, 79, 84, 83, 79, 76, 77, 81, 86, 84, 81, 77, 76, 79, 84, 86, 84, 83, 81, 79, 77, 76, 72, 0];
    const bass = [48, 53, 48, 55, 48, 53, 55, 48], step = 0.3;
    mel.forEach((m, i) => { if (!m) return; const t = 0.05 + i * step, f = midi(m);
      tone(oc, {f, t, dur: 1.1, peak: 0.22, attack: 0.003}); tone(oc, {f: f * 3, t, dur: 0.4, peak: 0.05, attack: 0.002}); });
    bass.forEach((m, i) => tone(oc, {type: 'triangle', f: midi(m), t: 0.05 + i * 3 * step, dur: 0.8, peak: 0.12}));
  }},
  scratch: {label: 'DJ scratch', sec: 0.9, make(oc, sec){
    const fc = curve(sec, t => 150 + 650 * Math.abs(Math.sin(2 * Math.PI * t * 2.2)), 200);
    const o = oc.createOscillator(); o.type = 'sawtooth'; o.frequency.setValueCurveAtTime(fc, 0, sec);
    const bp = filt(oc, 'bandpass', 800, 1.5); bp.frequency.setValueCurveAtTime(fc.map(v => v * 3), 0, sec);
    const n = bufSrc(oc, noiseBuf(oc, 1)), ng = gainN(oc, 0.4), g = oc.createGain();
    g.gain.setValueCurveAtTime(curve(sec, t => Math.sqrt(Math.sin(Math.PI * t / sec)) * (0.35 + 0.65 * Math.abs(Math.sin(2 * Math.PI * t * 2.2))), 200), 0, sec);
    o.connect(bp); n.connect(ng); ng.connect(bp); out(oc, [bp, g, gainN(oc, 0.8)]);
    o.start(0); o.stop(sec); n.start(0); n.stop(sec);
  }},
};
const TEST_SOUNDS = {walk: 'walk', run: 'run', spin: 'scratch', kick: 'wave', giant: 'stomp', dancer: 'ballerina', skip: 'skip', red: 'fire', blue: 'ocean', yellow: 'birds', world: 'wind'};
const soundLabel = s => !s ? '' : s.file ? s.fileName : 'Test: ' + ((BUILTINS[s.builtin] || {}).label || s.builtin);

// =====================================================================
// Setup (shared by both versions)
// =====================================================================
function defaultConfig(){
  const move = (id, name, hint, key, extra = {}) => Object.assign({id, type: 'move', name, hint, key, sound: null, volume: 100, loop: false}, extra);
  const color = (id, name, hex, key) => ({id, type: 'color', name, hint: 'scan', key, color: hex, sound: null, volume: 80});
  return {
    version: 2,
    inputs: [
      move('walk', 'Walk', 'walking', 'w', {type: 'step', loop: true, volume: 80}),
      move('run', 'Run', 'running', 'r', {type: 'step', loop: true, volume: 100}),
      {id: 'still', type: 'still', name: 'Stand still', hint: 'no movement: pauses all sound', key: 's'},
      move('spin', 'Spin', '360 spin', ' ', {type: 'spin', volume: 90}),
      move('kick', 'Karate kick', 'kick', 'k'),
      move('giant', 'Giant step', 'big step / lunge', 'g'),
      move('dancer', "Dancer's pose", 'one leg / yoga balance', 'd', {volume: 90}),
      move('skip', 'Skipping', 'skipping along', 'j'),
      color('red', 'Red', '#e53935', '1'),
      color('blue', 'Blue', '#1e88e5', '2'),
      color('yellow', 'Yellow', '#fdd835', '3'),
      {id: 'worldToggle', type: 'world', name: 'World loop on/off', hint: 'background sound', key: 'l'},
    ],
    world: {sound: null, volume: 80, level: 25, walk: 60, run: 100, walkMs: 600, runMs: 350, stopMs: 2000, duck: 30},
    library: {size: 3},
  };
}
let config = load('config', null) || defaultConfig();
const inputById = id => config.inputs.find(i => i.id === id);
const inputsOf = (...types) => config.inputs.filter(i => types.includes(i.type));
const target = id => id === 'world' ? config.world : inputById(id);
const targetName = id => id === 'world' ? 'Default world' : (inputById(id) || {}).name;
// Version B uses Walk/Run as tap keys, so their sounds only matter in Version A
const usesSound = inp => ['move', 'spin', 'color'].includes(inp.type) || (inp.type === 'step' && !FLEX);
function saveConfig(){ save('config', config); }

// =====================================================================
// Sound files (IndexedDB, shared by both pages)
// =====================================================================
const idb = {
  db: null,
  open(){ return this.db ? Promise.resolve(this.db) : new Promise((res, rej) => {
    const r = indexedDB.open('tui-wizard', 1);
    r.onupgradeneeded = () => r.result.createObjectStore('sounds');
    r.onsuccess = () => { this.db = r.result; res(this.db); }; r.onerror = () => rej(r.error);
  }); },
  async op(mode, fn){ const db = await this.open(); return new Promise((res, rej) => { const tx = db.transaction('sounds', mode); const q = fn(tx.objectStore('sounds')); tx.oncomplete = () => res(q && q.result); tx.onerror = () => rej(tx.error); }); },
  put(id, v){ return this.op('readwrite', s => s.put(v, id)); },
  get(id){ return this.op('readonly', s => s.get(id)); },
  del(id){ return this.op('readwrite', s => s.delete(id)); },
};
const allSounds = () => [...config.inputs.map(i => i.sound), config.world.sound].filter(Boolean);

// =====================================================================
// Audio engine
// =====================================================================
let ctx = null, master, worldBus, duckGain, started = false;
const buffers = new Map();
const soundKey = s => !s ? '' : s.file ? 'f:' + s.file : 'b:' + s.builtin;

async function renderBuiltin(id){
  const def = BUILTINS[id]; if (!def) return null;
  const oc = new OfflineAudioContext(2, Math.round(SR * def.sec), SR);
  def.make(oc, def.sec);
  const buf = await oc.startRendering();
  return def.crossfade ? loopable(buf) : buf;
}
async function getBuffer(s){
  const k = soundKey(s); if (!k) return null;
  if (buffers.has(k)) return buffers.get(k);
  let buf = null;
  try{
    if (s.file){ const rec = await idb.get(s.file); if (rec) buf = await (ctx || new OfflineAudioContext(1, 1, SR)).decodeAudioData(rec.data.slice(0)); }
    else buf = await renderBuiltin(s.builtin);
  }catch(e){ toast('Could not load sound: ' + soundLabel(s)); }
  if (buf) buffers.set(k, buf);
  return buf;
}

async function start(){
  ctx = new (window.AudioContext || window.webkitAudioContext)({latencyHint: 'interactive'});
  const comp = ctx.createDynamicsCompressor(); comp.threshold.value = -8; comp.ratio.value = 4;
  master = ctx.createGain(); master.gain.value = load('master', 90) / 100;
  worldBus = ctx.createGain(); worldBus.gain.value = 0;
  duckGain = ctx.createGain();
  worldBus.connect(duckGain); duckGain.connect(master); master.connect(comp); comp.connect(ctx.destination);
  $('startMsg').textContent = 'Loading sounds...';
  await ctx.resume();
  await Promise.all(allSounds().map(getBuffer));
  started = true;
  $('startOverlay').classList.add('hidden');
  await updateWorldSound();
  setInterval(worldTick, 50);
}

// ---------- one channel for movement sounds: a new one cuts the last ----------
let moveVoice = null;   // {src, gain, inputId, loop}
function stopMoveVoice(fade = 0.04){
  if (!moveVoice) return;
  const v = moveVoice; moveVoice = null;
  const t = ctx.currentTime; v.gain.gain.cancelScheduledValues(t); v.gain.gain.setValueAtTime(v.gain.gain.value, t); v.gain.gain.linearRampToValueAtTime(0, t + fade);
  try{ v.src.stop(t + fade + 0.01); }catch(e){}
  unduck(); refreshPlaying();
}
async function playMove(inp){
  stopMoveVoice();
  const buf = inp.sound && await getBuffer(inp.sound);
  if (!buf) return null;
  const src = ctx.createBufferSource(); src.buffer = buf; src.loop = !!inp.loop;
  const g = ctx.createGain(); g.gain.value = (inp.volume ?? 100) / 100;
  src.connect(g); g.connect(master); src.start();
  const v = {src, gain: g, inputId: inp.id, loop: !!inp.loop}; moveVoice = v;
  if (!v.loop) duck();
  src.onended = () => { if (moveVoice === v){ moveVoice = null; unduck(); } refreshPlaying(); };
  refreshPlaying();
  return v;
}
function duck(){ duckGain.gain.setTargetAtTime(config.world.duck / 100, ctx.currentTime, 0.05); $('worldDuck').textContent = 'ducked under a movement sound'; }
function unduck(){ duckGain.gain.setTargetAtTime(1, ctx.currentTime, 0.25); $('worldDuck').textContent = ''; }
function syncBeep(){
  const t = ctx.currentTime, o = ctx.createOscillator(), g = ctx.createGain();
  o.frequency.value = 1000; g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.6, t + 0.01); g.gain.setValueAtTime(0.6, t + 0.25); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.3);
  o.connect(g); g.connect(master); o.start(t); o.stop(t + 0.35);
}

// ---------- background world ----------
// A: {worldOn, color}   B: {worldOn, library: [ids], libIndex}
let state = Object.assign({worldOn: true, color: null, library: [], libIndex: 0}, load('state_' + VERSION, {}));
function saveState(){ save('state_' + VERSION, state); }
let paused = false, worldVoice = null, taps = [], paceState = 'idle';

function currentWorld(){
  const id = FLEX ? state.library[state.libIndex] : state.color;
  const col = id && inputById(id);
  if (col) return {name: col.name, color: col.color, sound: col.sound, volume: col.volume ?? 80, id: col.id};
  return {name: 'Default world', color: '#3a3f4b', sound: config.world.sound, volume: config.world.volume, id: 'world'};
}
async function updateWorldSound(){
  if (!started) return;
  const cur = currentWorld(), k = soundKey(cur.sound);
  if (worldVoice && worldVoice.key === k){ worldVoice.gain.gain.setTargetAtTime(cur.volume / 100, ctx.currentTime, 0.1); return; }
  const old = worldVoice; worldVoice = null;
  if (old){ old.gain.gain.setTargetAtTime(0, ctx.currentTime, 0.12); setTimeout(() => { try{ old.src.stop(); }catch(e){} }, 900); }
  const buf = cur.sound && await getBuffer(cur.sound); if (!buf) return;
  const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
  const g = ctx.createGain(); g.gain.value = 0; g.gain.setTargetAtTime(cur.volume / 100, ctx.currentTime, 0.15);
  src.connect(g); g.connect(worldBus); src.start();
  worldVoice = {src, gain: g, key: k};
}

function paceInterval(now){
  const w = config.world;
  if (!taps.length || now - taps[taps.length - 1] > w.stopMs) return null;
  const d = []; for (let i = Math.max(1, taps.length - 3); i < taps.length; i++) d.push(taps[i] - taps[i - 1]);
  const ok = d.filter(x => x <= w.stopMs).sort((a, b) => a - b);
  return ok.length ? ok[Math.floor(ok.length / 2)] : Infinity;
}
function worldTick(){
  const now = performance.now(), w = config.world;
  let level = w.level, st = 'on', iv = null;
  if (FLEX){
    iv = paceInterval(now); st = 'idle';
    if (iv !== null){
      const f = iv === Infinity ? 0 : clamp((w.walkMs - iv) / Math.max(1, w.walkMs - w.runMs), 0, 1);
      level = w.walk + (w.run - w.walk) * f; st = f >= 0.5 ? 'run' : 'walk';
    }
  }
  if (!state.worldOn){ level = 0; st = 'off'; }
  else if (paused){ level = 0; st = 'paused'; }
  worldBus.gain.setTargetAtTime(level / 100, ctx.currentTime, 0.12);
  if (FLEX && st !== paceState && ['idle', 'walk', 'run'].includes(st) && ['idle', 'walk', 'run'].includes(paceState)){
    const spm = iv && iv !== Infinity ? Math.round(60000 / iv) : null;
    logRow({movement: st === 'idle' ? 'Stopped stepping' : st === 'walk' ? 'Walking pace' : 'Running pace', sound: soundLabel(currentWorld().sound),
      detail: st === 'idle' ? `world back to ${w.level}%` : `${spm ? spm + ' steps/min, ' : ''}world ${Math.round(level)}%`, kind: 'pace'});
  }
  paceState = st;
  $('worldBar').style.width = level + '%';
  $('worldLevel').textContent = Math.round(level) + '%';
  if (FLEX) $('worldPace').textContent = iv && iv !== Infinity ? Math.round(60000 / iv) + ' steps/min' : '-';
  const b = $('worldState'); b.textContent = st.toUpperCase(); b.className = 'badge ' + st;
}

// =====================================================================
// What each input does
// =====================================================================
function noSound(inp){ toast(`${inp.name} has no sound yet. Drop an audio file on its button.`); return '(no sound yet)'; }
async function trigger(inp, via){
  if (!started) return;
  flashPad(inp.id);
  if (inp.type === 'world'){
    state.worldOn = !state.worldOn; saveState(); renderWorldPads();
    logRow({movement: inp.name, sound: soundLabel(currentWorld().sound), detail: state.worldOn ? 'world loop ON' : 'world loop OFF', via, kind: 'sys'});
    return;
  }
  if (inp.type === 'still'){
    stopMoveVoice(0.15); paused = true; taps = [];
    logRow({movement: inp.name, detail: 'all sound paused until the next movement', via});
    return;
  }
  paused = false;
  if (inp.type === 'step' && FLEX){
    taps.push(performance.now()); if (taps.length > 6) taps.shift();
    if ($('logEveryStep').checked) logRow({movement: inp.name, detail: 'step tap', via, kind: 'step'});
    return;
  }
  if (inp.type === 'color'){
    let detail;
    if (FLEX){
      const at = state.library.indexOf(inp.id);
      if (at >= 0){ state.libIndex = at; detail = `already in library (slot ${at + 1})`; }
      else {
        state.library.push(inp.id);
        let dropped = null;
        while (state.library.length > config.library.size) dropped = state.library.shift();
        state.libIndex = state.library.length - 1;
        detail = dropped ? `added, replaced oldest: ${targetName(dropped) || dropped}` : `added to slot ${state.libIndex + 1}`;
      }
      renderLibrary();
    } else {
      state.color = inp.id; detail = 'world switched to ' + inp.name;
    }
    saveState(); renderWorld(); updateWorldSound();
    logRow({movement: 'Scan: ' + inp.name, sound: inp.sound ? soundLabel(inp.sound) : noSound(inp), detail, via});
    return;
  }
  // step (Version A), spin and moves share the one movement channel
  if (moveVoice && moveVoice.inputId === inp.id){
    if (moveVoice.loop) return;
    stopMoveVoice(0.15);
    logRow({movement: inp.name, sound: soundLabel(inp.sound), detail: 'stopped (pressed again)', via});
    return;
  }
  const cut = moveVoice && inputById(moveVoice.inputId);
  await playMove(inp);
  const sound = inp.sound ? soundLabel(inp.sound) : noSound(inp);
  let detail = cut ? `cut ${cut.name}` : '';
  if (inp.type === 'spin' && FLEX){
    if (state.library.length){
      const before = currentWorld();
      state.libIndex = (state.libIndex + 1) % state.library.length;
      saveState(); renderLibrary(); renderWorld();
      setTimeout(updateWorldSound, 250);
      detail = [detail, `${before.name} -> ${currentWorld().name} (${state.libIndex + 1} of ${state.library.length})`].filter(Boolean).join('; ');
    } else detail = [detail, 'library is empty'].filter(Boolean).join('; ');
  }
  logRow({movement: inp.name, sound, detail, via});
}
function stopAll(via){
  if (!started) return;
  stopMoveVoice(0.03); paused = true; taps = [];
  worldBus.gain.cancelScheduledValues(ctx.currentTime); worldBus.gain.setTargetAtTime(0, ctx.currentTime, 0.02);
  logRow({movement: 'STOP ALL', detail: 'everything silenced until the next movement', via, kind: 'sys'});
}

// =====================================================================
// Assigning sound files (drag and drop or file picker)
// =====================================================================
const isAudio = f => /^audio\//.test(f.type) || /\.(wav|mp3|m4a|aac|ogg|oga|opus|flac|webm|aif|aiff|caf)$/i.test(f.name);
async function assignFile(targetId, file){
  const tgt = target(targetId); if (!tgt || !file) return;
  if (!isAudio(file)){ toast(`${file.name} is not an audio file`); return; }
  const data = await file.arrayBuffer();
  let buf;
  try{ buf = await (ctx || new OfflineAudioContext(1, 1, SR)).decodeAudioData(data.slice(0)); }
  catch(e){ toast(`Could not read ${file.name}. Try WAV or MP3.`); return; }
  const id = uid('f');
  await idb.put(id, {name: file.name, type: file.type, data});
  buffers.set('f:' + id, buf);
  const old = tgt.sound;
  tgt.sound = {file: id, fileName: file.name};
  saveConfig();
  if (old && old.file && !allSounds().some(s => s.file === old.file)) idb.del(old.file);
  renderAll(); updateWorldSound();
  toast(`${file.name} is now the sound for ${targetName(targetId)}`);
}
function setSound(targetId, sound){
  const tgt = target(targetId), old = tgt.sound;
  tgt.sound = sound; saveConfig();
  if (old && old.file && !allSounds().some(s => s.file === old.file)) idb.del(old.file);
  if (moveVoice && moveVoice.inputId === targetId) stopMoveVoice();
  renderAll(); updateWorldSound();
}
function wireDrops(root){
  root.querySelectorAll('[data-drop]').forEach(el => {
    el.addEventListener('dragover', e => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; el.classList.add('dragover'); });
    el.addEventListener('dragleave', () => el.classList.remove('dragover'));
    el.addEventListener('drop', e => { e.preventDefault(); e.stopPropagation(); el.classList.remove('dragover'); assignFile(el.dataset.drop, e.dataTransfer.files[0]); });
  });
}
// a file dropped next to a target must not navigate the page away
addEventListener('dragover', e => e.preventDefault());
addEventListener('drop', e => { e.preventDefault(); if (e.dataTransfer.files.length) toast('Drop the file right on a button or Sound cell'); });
function pickFile(targetId){
  const i = document.createElement('input'); i.type = 'file'; i.accept = 'audio/*';
  i.onchange = () => assignFile(targetId, i.files[0]); i.click();
}

// =====================================================================
// Session log (one per version)
// =====================================================================
let session = load('session_' + VERSION, {t0: null, name: '', notes: '', rows: []});
function saveSession(){ save('session_' + VERSION, session); }
function fmtElapsed(ms){
  if (ms == null) return '--:--.-';
  const s = Math.max(0, ms) / 1000, m = Math.floor(s / 60);
  return `${String(m).padStart(2, '0')}:${(s % 60).toFixed(1).padStart(4, '0')}`;
}
function logRow({movement, sound = '', detail = '', via = '', kind = 'trigger'}){
  const now = Date.now();
  session.rows.push({elapsed: session.t0 ? now - session.t0 : null, wall: new Date(now).toISOString(), movement, sound, detail, via, kind});
  saveSession(); renderLog();
}
function renderLog(){
  $('logBody').innerHTML = session.rows.slice(-400).reverse().map(r =>
    `<tr class="${r.kind === 'note' ? 'note' : r.kind === 'sys' ? 'sys' : ''}"><td class="t">${fmtElapsed(r.elapsed)}</td><td>${esc(r.movement)}</td><td>${esc(r.sound)}</td><td>${esc(r.detail)}</td></tr>`).join('');
  $('logCount').textContent = `(${session.rows.length})`;
  $('sessInfo').textContent = session.t0 ? 'started ' + new Date(session.t0).toLocaleTimeString() : 'not started: press Start session, then clap on camera';
}
function startSession(){
  if (session.rows.length && !confirm('Start a new session? This clears the current log. Export the CSV first if you need it.')) return;
  session = {t0: Date.now(), name: $('sessName').value, notes: '', rows: []};
  $('sessNotes').value = '';
  if (started) syncBeep();
  const fl = $('flash'); fl.classList.add('on'); setTimeout(() => fl.classList.remove('on'), 120);
  logRow({movement: 'SESSION START', detail: `Version ${VERSION}. Time zero (beep + white flash): clap on camera now.`, kind: 'sys'});
}
function exportCsv(){
  const q = v => `"${String(v ?? '').replace(/"/g, '""')}"`;
  const lines = [['version', 'elapsed_s', 'elapsed', 'wall_time', 'movement', 'sound', 'detail', 'via', 'type'].join(',')];
  for (const r of session.rows) lines.push([VERSION, r.elapsed == null ? '' : (r.elapsed / 1000).toFixed(2), fmtElapsed(r.elapsed), r.wall, r.movement, r.sound, r.detail, r.via, r.kind].map(q).join(','));
  lines.push([VERSION, '', '', '', 'SESSION NOTES', '', session.notes, '', 'notes'].map(q).join(','));
  const name = (session.name || 'session').replace(/[^\w-]+/g, '_');
  const date = new Date(session.t0 || Date.now()).toISOString().slice(0, 16).replace(/[:T]/g, '-');
  download(new Blob([lines.join('\n')], {type: 'text/csv'}), `tui-shoes-${VERSION}-${name}-${date}.csv`);
}
function download(blob, name){
  const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = name; document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}
setInterval(() => { $('sessClock').textContent = session.t0 ? fmtElapsed(Date.now() - session.t0) : '--:--.-'; }, 100);

// =====================================================================
// Perform view
// =====================================================================
function isLight(hex){ const n = parseInt((hex || '#000').slice(1), 16); return ((n >> 16 & 255) * 0.299 + (n >> 8 & 255) * 0.587 + (n & 255) * 0.114) > 160; }
function padHtml(inp){
  const isColor = inp.type === 'color', needs = usesSound(inp), empty = needs && !inp.sound;
  const style = isColor ? `style="background:${inp.color};border-color:${empty ? '#fff' : inp.color}"` : '';
  const cls = ['pad', isColor ? 'color' : '', isColor && isLight(inp.color) ? 'light' : '', empty ? 'emptyslot' : '', inp.type === 'world' && !state.worldOn ? 'off' : ''].join(' ');
  let snd = '';
  if (needs) snd = inp.sound ? `<span class="snd">${esc(soundLabel(inp.sound))}${inp.loop ? ' · loops' : ''}</span>` : '<span class="snd empty">Empty: drop a sound file here</span>';
  else if (inp.type === 'step') snd = '<span class="snd">tap on every step: sets world volume</span>';
  else if (inp.type === 'still') snd = '<span class="snd">pauses all sound</span>';
  else if (inp.type === 'world') snd = `<span class="snd">${state.worldOn ? 'ON: click to turn off' : 'OFF: click to turn on'}</span>`;
  return `<button class="${cls}" data-pad="${inp.id}" ${needs ? `data-drop="${inp.id}"` : ''} ${style}>
    <span class="nm">${esc(inp.name)}</span><span class="hint">${esc(inp.hint || '')}</span>${snd}
    ${inp.key ? `<kbd>${esc(keyLabel(inp.key))}</kbd>` : ''}</button>`;
}
function renderPerform(){
  $('movePads').innerHTML = inputsOf('step', 'still', 'spin', 'move').map(padHtml).join('');
  $('colorPads').innerHTML = inputsOf('color').map(padHtml).join('');
  wireDrops($('movePads')); wireDrops($('colorPads'));
  ['movePads', 'colorPads'].forEach(id => $(id).querySelectorAll('[data-pad]').forEach(b => b.onclick = () => { const i = inputById(b.dataset.pad); if (i) trigger(i, 'click'); }));
  renderWorldPads(); renderLibrary(); renderWorld(); refreshPlaying();
}
function renderWorldPads(){
  const w = config.world;
  $('worldPads').innerHTML = inputsOf('world').map(padHtml).join('') +
    `<button class="pad ${w.sound ? '' : 'emptyslot'}" data-drop="world" id="worldSlot">
      <span class="nm">Default world sound</span><span class="hint">plays when no color is active${FLEX ? ' (library empty)' : ''}. Click to choose a file.</span>
      ${w.sound ? `<span class="snd">${esc(soundLabel(w.sound))}</span>` : '<span class="snd empty">Empty: drop a sound file here</span>'}</button>`;
  wireDrops($('worldPads'));
  $('worldPads').querySelectorAll('[data-pad]').forEach(b => b.onclick = () => { const i = inputById(b.dataset.pad); if (i) trigger(i, 'click'); });
  $('worldSlot').onclick = () => pickFile('world');
}
function renderWorld(){
  const cur = currentWorld();
  $('worldName').textContent = `${cur.name}: ${cur.sound ? soundLabel(cur.sound) : 'no sound yet'}`;
  $('worldSwatch').style.background = cur.color;
}
function renderLibrary(){
  if (!FLEX) return;
  state.library = state.library.filter(id => inputById(id));
  state.libIndex = clamp(state.libIndex, 0, Math.max(0, state.library.length - 1));
  $('libMax').textContent = config.library.size;
  const slots = [];
  for (let i = 0; i < config.library.size; i++){
    const inp = inputById(state.library[i]);
    slots.push(inp
      ? `<div class="slot ${i === state.libIndex ? 'cur' : ''}"><div class="sw" style="background:${inp.color}"></div><div><div class="n">${i + 1}${i === state.libIndex ? ' · playing' : ''}${i === 0 && state.library.length >= config.library.size ? ' · oldest' : ''}</div><b>${esc(inp.name)}</b><div class="n">${esc(soundLabel(inp.sound) || 'no sound yet')}</div></div></div>`
      : `<div class="slot empty"><div class="sw"></div><div class="n">${i + 1} · empty: scan a color</div></div>`);
  }
  $('lib').innerHTML = slots.join('');
}
function flashPad(id){ document.querySelectorAll(`[data-pad="${id}"]`).forEach(p => { p.classList.add('hit'); setTimeout(() => p.classList.remove('hit'), 140); }); }
function refreshPlaying(){ document.querySelectorAll('[data-pad]').forEach(p => p.classList.toggle('playing', !!moveVoice && moveVoice.inputId === p.dataset.pad)); }

// =====================================================================
// Setup view
// =====================================================================
let capturing = null;
function soundCell(targetId){
  const s = target(targetId).sound;
  const opts = Object.entries(BUILTINS).map(([id, d]) => `<option value="b:${id}" ${s && !s.file && s.builtin === id ? 'selected' : ''}>Test: ${d.label}</option>`).join('');
  return `<td class="soundcell" data-drop="${targetId}">
    ${s ? `<b>${esc(soundLabel(s))}</b>` : '<span class="empty">Empty: drop a file here</span>'}
    <label class="filelab">choose file<input type="file" accept="audio/*" data-file="${targetId}"></label>
    <select data-sound="${targetId}" title="Built-in test sounds"><option value="">${s ? 'No sound' : '(test sounds)'}</option>${s && s.file ? `<option value="keep" selected>File</option>` : ''}${opts}</select>
    ${s ? `<button class="small" data-test="${targetId}">play</button>` : ''}</td>`;
}
function renderSetup(){
  const head = `<tr><th>Type</th><th>Name</th><th>What the wearer does</th><th>Key</th><th>Color</th><th>Sound</th><th>Volume</th><th>Loop</th><th></th></tr>`;
  const order = ['step', 'still', 'spin', 'move', 'color', 'world'];
  const label = {step: 'walk/run', still: 'stand still', spin: 'spin', move: 'movement', color: 'color', world: 'world on/off'};
  const rows = [...config.inputs].sort((a, b) => order.indexOf(a.type) - order.indexOf(b.type)).map(inp => {
    const hasSound = inp.type in {step: 1, spin: 1, move: 1, color: 1};
    return `<tr>
    <td>${label[inp.type]}</td>
    <td><input type="text" value="${esc(inp.name)}" data-field="name" data-id="${inp.id}"></td>
    <td><input type="text" value="${esc(inp.hint || '')}" data-field="hint" data-id="${inp.id}"></td>
    <td><button class="keybtn ${capturing === inp.id ? 'capturing' : ''}" data-key="${inp.id}">${capturing === inp.id ? 'press...' : (keyLabel(inp.key) || 'none')}</button></td>
    <td>${inp.type === 'color' ? `<input type="color" value="${inp.color}" data-field="color" data-id="${inp.id}">` : ''}</td>
    ${hasSound ? soundCell(inp.id) : `<td class="help">${inp.type === 'still' ? 'pauses all sound' : inp.type === 'world' ? 'turns the world loop on/off' : ''}</td>`}
    <td>${hasSound ? `<input type="range" min="0" max="100" value="${inp.volume ?? 80}" data-field="volume" data-id="${inp.id}"> <span class="help">${inp.volume ?? 80}%</span>` : ''}</td>
    <td>${inp.type === 'step' || inp.type === 'move' ? `<input type="checkbox" ${inp.loop ? 'checked' : ''} data-field="loop" data-id="${inp.id}">` : ''}</td>
    <td><button class="small" data-del="${inp.id}">remove</button></td></tr>`;
  }).join('');
  const worldRow = `<tr><td>world</td><td colspan="4"><b>Default world sound</b> <span class="help">loops underneath when no color is active</span></td>${soundCell('world')}
    <td><input type="range" min="0" max="100" value="${config.world.volume}" id="worldVol"> <span class="help">${config.world.volume}%</span></td><td></td><td></td></tr>`;
  const t = $('setupTable');
  t.innerHTML = head + rows + worldRow;

  t.querySelectorAll('[data-field]').forEach(el => {
    const ev = el.type === 'range' || el.type === 'color' ? 'input' : 'change';
    el.addEventListener(ev, () => {
      const inp = inputById(el.dataset.id), f = el.dataset.field;
      inp[f] = el.type === 'range' ? +el.value : el.type === 'checkbox' ? el.checked : el.value;
      if (f === 'volume') el.nextElementSibling.textContent = el.value + '%';
      saveConfig(); renderPerform(); if (f === 'volume' || f === 'color') updateWorldSound();
    });
  });
  $('worldVol').oninput = e => { config.world.volume = +e.target.value; e.target.nextElementSibling.textContent = e.target.value + '%'; saveConfig(); updateWorldSound(); };
  t.querySelectorAll('[data-key]').forEach(b => b.onclick = () => { capturing = capturing === b.dataset.key ? null : b.dataset.key; renderSetup(); });
  t.querySelectorAll('[data-del]').forEach(b => b.onclick = () => {
    const inp = inputById(b.dataset.del);
    if (!confirm(`Remove "${inp.name}"?`)) return;
    config.inputs = config.inputs.filter(i => i !== inp);
    saveConfig(); renderAll(); updateWorldSound();
  });
  t.querySelectorAll('[data-sound]').forEach(sel => sel.onchange = () => {
    if (sel.value === 'keep') return;
    setSound(sel.dataset.sound, sel.value ? {builtin: sel.value.slice(2)} : null);
  });
  t.querySelectorAll('[data-file]').forEach(el => el.onchange = () => assignFile(el.dataset.file, el.files[0]));
  t.querySelectorAll('[data-test]').forEach(b => b.onclick = async () => {
    if (!started) return toast('Press Start sound first');
    const id = b.dataset.test, tgt = target(id);
    await playMove({id: 'test:' + id, sound: tgt.sound, volume: tgt.volume ?? 80});
  });
  wireDrops(t);
  renderWorldSettings();
}
function renderWorldSettings(){
  const w = config.world;
  const fields = FLEX
    ? [['level', 'Idle level (no taps)', 0, 100, '%'], ['walk', 'Walking level', 0, 100, '%'], ['run', 'Running level', 0, 100, '%'],
       ['walkMs', 'Walk: taps this far apart', 300, 1200, ' ms'], ['runMs', 'Run: taps this far apart', 150, 700, ' ms'],
       ['stopMs', 'No tap for this long = idle', 500, 5000, ' ms'], ['duck', 'World under a movement', 0, 100, '%']]
    : [['level', 'World level', 0, 100, '%'], ['duck', 'World under a movement', 0, 100, '%']];
  $('worldSettings').innerHTML = fields.map(([k, lab, min, max, unit]) =>
    `<div class="setrow"><span>${lab}</span><input type="range" min="${min}" max="${max}" value="${w[k]}" data-world="${k}"><span class="help">${w[k]}${unit}</span></div>`).join('')
    + (FLEX ? `<div class="setrow"><span>Library size</span><input type="range" min="1" max="6" value="${config.library.size}" id="libSize"><span class="help">${config.library.size}</span></div>
      <p class="help" style="grid-column:1/-1">Taps at or slower than the walk spacing play the world at the walking level; faster taps rise toward the running level. Walk and Run keys both count as taps.</p>`
      : '<p class="help" style="grid-column:1/-1">The world level is shared with Version B\'s idle level.</p>');
  $('worldSettings').querySelectorAll('[data-world]').forEach(el => el.oninput = () => {
    w[el.dataset.world] = +el.value; el.nextElementSibling.textContent = el.value + fields.find(f => f[0] === el.dataset.world)[4]; saveConfig();
  });
  if (FLEX) $('libSize').oninput = e => {
    config.library.size = +e.target.value; e.target.nextElementSibling.textContent = e.target.value;
    while (state.library.length > config.library.size) state.library.shift();
    saveConfig(); saveState(); renderLibrary(); renderWorld(); updateWorldSound();
  };
}
function addInput(type){
  const id = uid(type);
  const base = type === 'color'
    ? {id, type, name: 'New color', hint: 'scan', key: '', color: '#00bcd4', sound: null, volume: 80}
    : {id, type: 'move', name: 'New movement', hint: '', key: '', sound: null, volume: 100, loop: false};
  config.inputs.push(base); saveConfig(); capturing = id; renderAll();
  toast('Added. Press the key you want for it.');
}
function fillTestSounds(){
  let n = 0;
  for (const inp of config.inputs) if (inp.type in {step: 1, spin: 1, move: 1, color: 1} && !inp.sound){
    inp.sound = {builtin: TEST_SOUNDS[inp.id] || (inp.type === 'color' ? 'wind' : 'stomp')}; n++;
  }
  if (!config.world.sound){ config.world.sound = {builtin: TEST_SOUNDS.world}; n++; }
  saveConfig(); renderAll(); updateWorldSound();
  toast(n ? `Filled ${n} empty slots with test sounds` : 'No empty slots');
}

// ---------- export / import ----------
async function exportConfig(){
  const files = {};
  for (const s of allSounds().filter(s => s.file)){
    if (files[s.file]) continue;
    const rec = await idb.get(s.file); if (!rec) continue;
    const bytes = new Uint8Array(rec.data); let bin = '';
    for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    files[s.file] = {name: rec.name, type: rec.type, base64: btoa(bin)};
  }
  download(new Blob([JSON.stringify({app: 'tui-shoes-wizard', version: 2, config, files})], {type: 'application/json'}), 'tui-shoes-wizard-setup.json');
}
async function importConfig(file){
  try{
    const j = JSON.parse(await file.text());
    if (!j.config || !Array.isArray(j.config.inputs) || j.config.version !== 2) throw new Error('not a setup file from this version of the wizard');
    for (const [id, f] of Object.entries(j.files || {})){
      const bin = atob(f.base64), bytes = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
      await idb.put(id, {name: f.name, type: f.type, data: bytes.buffer});
      buffers.delete('f:' + id);
    }
    config = j.config; saveConfig();
    if (started){ await Promise.all(allSounds().map(getBuffer)); if (worldVoice) worldVoice.key = ''; updateWorldSound(); }
    renderAll(); toast('Setup imported');
  }catch(e){ toast('Import failed: ' + e.message); }
}

// =====================================================================
// Keyboard, tabs, wiring
// =====================================================================
addEventListener('keydown', e => {
  if (capturing){
    e.preventDefault();
    if (e.key !== 'Escape'){
      const k = e.key === ' ' ? ' ' : e.key.length === 1 ? e.key.toLowerCase() : null;
      if (!k){ toast('Use a letter, number, symbol or Space'); return; }
      const other = config.inputs.find(i => i.key === k && i.id !== capturing);
      if (other){ other.key = ''; toast(`"${keyLabel(k)}" was on ${other.name}. That one has no key now.`); }
      inputById(capturing).key = k; saveConfig();
    }
    capturing = null; renderAll(); return;
  }
  if (e.key === 'Escape'){ e.preventDefault(); stopAll('key'); return; }
  const tag = e.target.tagName;
  if ((tag === 'INPUT' && !['range', 'checkbox'].includes(e.target.type)) || tag === 'TEXTAREA' || tag === 'SELECT') return;
  if (e.repeat || e.metaKey || e.ctrlKey || e.altKey) return;
  const k = e.key === ' ' ? ' ' : e.key.toLowerCase();
  const inp = config.inputs.find(i => i.key === k);
  if (inp){ e.preventDefault(); trigger(inp, 'key'); }
});
function showTab(t){
  document.querySelectorAll('.tabs button').forEach(b => b.classList.toggle('on', b.dataset.tab === t));
  $('perform').classList.toggle('hidden', t !== 'perform'); $('setup').classList.toggle('hidden', t !== 'setup');
  if (t === 'setup') renderSetup();
}
function renderAll(){ renderPerform(); if (!$('setup').classList.contains('hidden')) renderSetup(); }

$('startBtn').onclick = () => start().catch(e => { $('startMsg').textContent = 'Could not start sound: ' + e.message; });
$('panicBtn').onclick = () => stopAll('click');
document.querySelectorAll('.tabs button').forEach(b => b.onclick = () => showTab(b.dataset.tab));
$('masterVol').value = load('master', 90);
$('masterVol').oninput = () => { save('master', +$('masterVol').value); if (master) master.gain.setTargetAtTime($('masterVol').value / 100, ctx.currentTime, 0.05); };
if (FLEX) $('libClear').onclick = () => { state.library = []; state.libIndex = 0; saveState(); renderLibrary(); renderWorld(); updateWorldSound(); logRow({movement: 'Library emptied', kind: 'sys'}); };
else $('clearColor').onclick = () => { state.color = null; saveState(); renderWorld(); updateWorldSound(); logRow({movement: 'Back to default world', sound: soundLabel(config.world.sound), kind: 'sys'}); };
$('sessStart').onclick = startSession;
$('sessCsv').onclick = exportCsv;
$('sessName').value = session.name || '';
$('sessName').oninput = () => { session.name = $('sessName').value; saveSession(); };
$('sessNotes').value = session.notes || '';
$('sessNotes').oninput = () => { session.notes = $('sessNotes').value; saveSession(); };
const addNote = () => { const v = $('quickNote').value.trim(); if (!v) return; logRow({movement: 'Note', detail: v, kind: 'note'}); $('quickNote').value = ''; };
$('quickNoteBtn').onclick = addNote;
$('quickNote').addEventListener('keydown', e => { if (e.key === 'Enter') addNote(); });
$('addMove').onclick = () => addInput('move');
$('addColor').onclick = () => addInput('color');
$('fillTest').onclick = fillTestSounds;
$('exportCfg').onclick = exportConfig;
$('importCfg').onchange = e => { if (e.target.files[0]) importConfig(e.target.files[0]); e.target.value = ''; };
$('resetCfg').onclick = () => {
  if (!confirm('Reset all inputs, keys and sounds to the defaults (empty sound slots)? This affects both versions.')) return;
  config = defaultConfig(); saveConfig(); state.color = null; state.library = []; state.libIndex = 0; saveState(); renderAll(); updateWorldSound();
};
renderPerform(); renderLog();
window.wizard = {VERSION, get config(){ return config; }, get state(){ return state; }, get session(){ return session; }, get paused(){ return paused; },
  get moveVoice(){ return moveVoice && moveVoice.inputId; }, get started(){ return started; },
  worldLevel: () => worldBus && worldBus.gain.value, duckLevel: () => duckGain && duckGain.gain.value, assignFile};

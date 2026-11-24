// Runs sampler/movements.js on made-up packets from both shoes (25 per second each).
// Run:  node tests/movement_test/movement_test.js       (VERBOSE=1 in front to see what the app logs)
const { MovementDetector } = require('../../sampler/movements.js');

let failures = 0;
const check = (cond, msg) => { console.log((cond ? '  ok    ' : '  FAIL  ') + msg); if (!cond) failures++; };

// One made-up shoe. Change its fields, then the runner sends a packet every 40 ms.
class Foot {
  constructor(){ this.down = true; this.pose = 0; this.state = 0; this.steps = 0; this.spins = 0; this.events = 0; this.event = 0; this.air10 = 0; this.upAt = 0; this.pitch = 0; this.roll = 0; }
  lift(t){ if (this.down){ this.down = false; this.upAt = t; } }
  land(t){ if (!this.down){ this.down = true; this.steps++; this.air10 = Math.round((t - this.upAt) / 10); } }
  fire(code){ this.event = code; this.events++; }
  packet(){ return { state:this.state, spins:this.spins, flags:(this.down ? 2 : 0) | 1 | 4, spinPct:0, fsr:this.down ? 2000 : 50,
    steps:this.steps, stepMs:0, pitch:this.pitch, roll:this.roll, peakG:1, gyro:0, yaw:0, event:this.event, events:this.events, pose:this.pose, air10:this.air10 }; }
}

class Scene {
  constructor(feet = 'LR'){
    this.log = [];
    this.det = new MovementDetector({ onLog: s => { this.log.push(s); if (process.env.VERBOSE) console.log('        ' + s); } });
    this.L = new Foot(); this.R = new Foot(); this.t = 0; this.feet = feet;
    this.seen = [];   // every movement that became "now", in order
    this.run(1500);
  }
  // advance ms; fn(t) can change the feet at each 10 ms step
  run(ms, fn){
    for (const end = this.t + ms; this.t < end; ){
      this.t += 10;
      if (fn) fn(this.t);
      if (this.t % 40 === 0){
        for (const id of this.feet) this.det.ingest(id, this[id].packet(), this.t + (id === 'R' ? 7 : 0));
        const cur = this.det.update(this.t + 8);
        if (!this.seen.length || this.seen[this.seen.length - 1].id !== cur.id) this.seen.push({key:cur.key, id:cur.id});
      }
    }
    return this;
  }
  now(){ return this.det.current.key; }
  keys(){ return this.seen.map(s => s.key); }
  has(k){ return this.keys().includes(k); }
  count(k){ return this.keys().filter(x => x === k).length; }
}
const quick = ['spin','jump','hop','kick','lunge','stomp','toetap'];
const noQuick = s => quick.every(k => !s.has(k));

console.log('Standing still');
{ const s = new Scene(); s.run(3000); check(s.now() === 'still', 'both feet down, nothing happening = STILL'); check(noQuick(s), 'no quick moves'); }

console.log('Walking (each foot down 600 ms, up 500 ms, taking turns)');
{ const s = new Scene();
  s.run(6000, t => {
    for (const [id, off] of [['L', 0], ['R', 550]]){
      const p = (t + 1100 - off) % 1100, f = s[id];
      if (p === 600) f.lift(t); if (p === 0) f.land(t);
      if (f.steps >= 2) f.state = 1;
    }
  });
  check(s.now() === 'walk', 'WALK'); check(noQuick(s), 'no jump / hop / lunge while walking'); }

console.log('Running (down 250 ms, up 450 ms: both feet in the air for a moment)');
{ const s = new Scene();
  s.run(6000, t => {
    for (const [id, off] of [['L', 0], ['R', 350]]){
      const p = (t + 700 - off) % 700, f = s[id];
      if (p === 250) f.lift(t); if (p === 0) f.land(t);
      if (f.steps >= 2) f.state = 2;
    }
  });
  check(s.now() === 'run', 'RUN'); check(noQuick(s), 'running flight is not a jump or hop'); }

// both feet taking turns; perFootMs = time between two steps of the same foot
const stepping = (s, perFootMs, ms) => s.run(ms, t => {
  for (const [id, off] of [['L', 0], ['R', perFootMs / 2]]){
    const p = (t + perFootMs - off) % perFootMs, f = s[id];
    if (p === Math.round(perFootMs * 0.55 / 10) * 10) f.lift(t); if (p === 0) f.land(t);
    if (f.steps >= 2) f.state = 1;
  }
});
const switchTime = (s, key, fn) => { const start = s.t; let at = null;
  s.run(6000, t => { fn(t); if (at === null && s.det.current.key === key) at = t; }); return at === null ? null : at - start; };

console.log('Step rate (steps per minute, both feet)');
{ const s = new Scene(); stepping(s, 1100, 5000); const r = s.det.rates(s.t);
  console.log(`        walking 1100 ms per foot: both=${r.both.toFixed(0)} L=${r.L.toFixed(0)} R=${r.R.toFixed(0)}`);
  check(Math.abs(r.both - 109) < 6 && Math.abs(r.L - 109) < 10, 'walking ~109 steps/min'); check(s.now() === 'walk', 'that is WALK at the default 150'); }
{ const s = new Scene(); stepping(s, 760, 5000); const r = s.det.rates(s.t);
  console.log(`        760 ms per foot: both=${r.both.toFixed(0)}`);
  check(s.now() === 'run', '158 steps/min is RUN'); }
{ const s = new Scene(); stepping(s, 1100, 3000); s.det.T.RUN_SPM = 100; s.run(500);
  check(s.now() === 'run', 'moving the run line down to 100 makes the same walking RUN, right away'); }
{ const s = new Scene(); const keep = ms => stepping(s, 900, ms); keep(5000);
  console.log(`        900 ms per foot: both=${s.det.rates(s.t).both.toFixed(0)}`);
  check(s.now() === 'walk', '133 steps/min = WALK');
  s.det.T.RUN_SPM = 125; keep(400); check(s.now() === 'run', 'line at 125: RUN');
  s.det.T.RUN_SPM = 140; keep(1500); check(s.now() === 'run', 'line at 140 (133 is within the 10 hysteresis): stays RUN');
  s.det.T.RUN_SPM = 150; keep(400); check(s.now() === 'walk', 'line at 150: back to WALK'); }
for (const [per, want] of [[1100, 'walk'], [700, 'run']]){
  const s = new Scene('L'); stepping(s, per, 5000); const r = s.det.rates(s.t);
  console.log(`        left shoe only, ${per} ms per foot: used=${r.used.toFixed(0)}`);
  check(s.now() === want, `one shoe only: its own rate x 2 decides (${want.toUpperCase()})`); }
{ // uneven walking: each step 1100 ms +-15%, and the sensor misses 1 step in 12
  let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const s = new Scene(), next = {L:s.t + 100, R:s.t + 650}; let k = 0; const t0 = s.t, both = [], foot = [];
  s.run(30000, t => {
    for (const id of ['L','R']){ const f = s[id];
      if (t >= next[id] - 500 && f.down) f.lift(t);
      if (t >= next[id]){ if ((k++ % 12) !== 5) f.land(t); else f.down = true; next[id] = t + 1100 * (0.85 + 0.3 * rnd()); }
      if (f.steps >= 2) f.state = 1; }
    if (t % 40 === 0 && t > t0 + 6000){ const r = s.det.rates(t); both.push(r.both); foot.push(r.L); }
  });
  const range = a => { a = [...a].sort((x, y) => x - y); return [a[Math.floor(a.length * 0.05)], a[Math.floor(a.length * 0.95)]]; };
  const [b0, b1] = range(both), [f0, f1] = range(foot);
  console.log(`        uneven walking: both feet ${b0.toFixed(0)}-${b1.toFixed(0)}, left ${f0.toFixed(0)}-${f1.toFixed(0)} steps/min (90% of the time)`);
  check(b0 > 85 && b1 < 125 && f0 > 85 && f1 < 125, 'uneven walking with missed steps: the rate stays steady (within ~85-125)');
  check(!s.has('run'), 'and never RUN'); }
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) { s.L.lift(t); s.R.lift(t); } if (t === t0 + 420) { s.L.land(t); s.R.land(t); } });
  check(s.det.rates(s.t).both < 60, 'a jump (both feet landing together) does not look like fast steps'); }
{ const s = new Scene(); stepping(s, 1100, 5000);
  let cyc = 0; const ms = switchTime(s, 'run', t => { cyc += 10; const p = cyc % 700, q = (cyc + 350) % 700;
    if (p === 380) s.L.lift(t); if (p === 0) s.L.land(t); if (q === 380) s.R.lift(t); if (q === 0) s.R.land(t); });
  console.log(`        walk (109/min) -> run (171/min): RUN after ${ms} ms`);
  check(ms !== null && ms < 2800, 'walk to run switches within ~2.5 s (about 3 fast steps per foot)');
  const back = switchTime(s, 'walk', t => { cyc += 10; const p = cyc % 1100, q = (cyc + 550) % 1100;
    if (p === 600) s.L.lift(t); if (p === 0) s.L.land(t); if (q === 600) s.R.lift(t); if (q === 0) s.R.land(t); });
  console.log(`        run -> walk: WALK after ${back} ms`);
  check(back !== null && back < 2800, 'run to walk switches within ~2.5 s');
  s.L.land(s.t); s.R.land(s.t);
  const stop = switchTime(s, 'still', () => {});
  console.log(`        walk -> stop: STILL ${stop} ms after the last step`);
  check(stop !== null && stop <= 3200, 'stopping: STILL within ~3 s'); }

console.log('Jump with both feet');
{ const s = new Scene();
  const t0 = s.t;
  s.run(1200, t => {
    if (t === t0 + 100) s.L.lift(t); if (t === t0 + 120) s.R.lift(t);
    if (t === t0 + 420) { s.L.land(t); } if (t === t0 + 450) { s.R.land(t); }
    if (t === t0 + 500) { s.L.fire(1); s.R.fire(1); }   // both shoes also report a hard hit
  });
  check(s.has('jump'), 'JUMP'); check(!s.has('stomp'), 'the landing hits are not stomps'); check(!s.has('hop'), 'not a hop'); }

console.log('Hop on one foot (right foot held up)');
{ const s = new Scene(); const t0 = s.t;
  s.run(2500, t => {
    if (t === t0 + 100) s.R.lift(t);
    if (t === t0 + 600) s.L.lift(t); if (t === t0 + 880) { s.L.land(t); s.L.fire(1); }
    if (t === t0 + 1500) s.L.lift(t); if (t === t0 + 1780) s.L.land(t);
  });
  check(s.count('hop') === 2, 'two HOPs'); check(!s.has('jump') && !s.has('stomp') && !s.has('skip'), 'not a jump, stomp or skip'); }

console.log('Skipping (step-hop on the left, step-hop on the right)');
{ const s = new Scene(); const t0 = s.t;
  s.run(4000, t => {
    // each foot: step down, hop, then the other foot steps
    const k = t - t0, cyc = k % 1200, left = Math.floor(k / 1200) % 2 === 0;
    const a = left ? s.L : s.R, b = left ? s.R : s.L;
    if (cyc === 0) { a.land(t); }
    if (cyc === 60) b.lift(t);
    if (cyc === 300) a.lift(t);
    if (cyc === 520) a.land(t);
    if (cyc === 1150) a.lift(t);
  });
  const k = s.keys(), afterFirst = k.slice(k.indexOf('skip'));
  check(s.has('skip'), 'SKIP'); check(s.now() === 'skip', 'still skipping at the end');
  check(afterFirst.every(x => x === 'skip'), 'stays SKIP between hops (no walk in between)'); check(!s.has('jump'), 'not a jump'); }

console.log('Karate kick');
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) s.R.lift(t); if (t === t0 + 300) s.R.fire(3); if (t === t0 + 700) s.R.land(t); });
  check(s.has('kick'), 'KICK while standing on the other foot'); }
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) { s.L.lift(t); s.R.lift(t); } if (t === t0 + 200) s.R.fire(3); if (t === t0 + 700) { s.L.land(t); s.R.land(t); } });
  check(!s.has('kick'), 'no kick when both feet are in the air'); }

console.log('Giant step / lunge');
{ const s = new Scene(); const t0 = s.t;
  s.run(2000, t => {
    if (t === t0 + 100) s.L.lift(t);
    if (t === t0 + 300) s.R.pose = 1;   // back foot up on its toes
    if (t === t0 + 750) s.L.land(t);
  });
  check(s.has('lunge'), 'LUNGE'); check(!s.has('hop'), 'not a hop'); }
{ const s = new Scene(); const t0 = s.t;
  s.run(2000, t => { if (t === t0 + 100) s.L.lift(t); if (t === t0 + 750) s.L.land(t); });
  check(!s.has('lunge'), 'a long step without the back foot on its toes is not a lunge'); }

console.log("Dancer's pose");
{ const s = new Scene(); const t0 = s.t;
  s.run(2500, t => { if (t === t0 + 100) { s.R.lift(t); s.R.pitch = 20; } });
  check(s.now() === 'dancer', "DANCER'S POSE after 1.5 s on the left foot"); }
{ const s = new Scene(); const t0 = s.t;
  s.run(2500, t => { if (t === t0 + 100) s.R.lift(t); if (t === t0 + 900) s.R.land(t); });
  check(!s.has('dancer'), 'lifting a foot for 0.8 s is not the pose'); }

console.log('Stomp');
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) s.R.lift(t); if (t === t0 + 300) s.R.land(t); if (t === t0 + 380) s.R.fire(1); });
  check(s.has('stomp'), 'STOMP with the other foot standing'); }

console.log('Toe tap');
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 400) s.L.fire(2); if (t === t0 + 800) s.L.fire(2); });
  check(s.count('toetap') === 2, 'TOE TAP (twice)'); }

console.log('Foot tilt');
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) { s.L.pose = 1; s.L.pitch = -30; } });
  check(s.now() === 'tilt', 'TILT: left shoe on tiptoe');
  check(/tiptoe/.test(s.det.current.why), 'says which foot and how: ' + s.det.current.why); }
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) { s.R.pose = 3; s.R.roll = 30; } });
  check(s.now() === 'tilt', 'TILT: right shoe rolled sideways'); }

console.log('Both feet to the sky');
{ const s = new Scene(); const t0 = s.t;
  s.run(2500, t => { if (t === t0 + 100) { s.L.lift(t); s.R.lift(t); s.L.pose = 4; s.R.pose = 4; } });
  check(s.now() === 'sky', 'SKY'); check(!s.has('dancer'), "never the dancer's pose");
  s.run(1500, t => { if (t === t0 + 2600) { s.L.pose = 0; s.R.pose = 0; s.L.land(t); s.R.land(t); } });
  check(!s.has('jump'), 'putting both feet down afterwards is not a jump'); }
{ const s = new Scene(); const t0 = s.t;
  s.run(2500, t => { if (t === t0 + 100) { s.L.lift(t); s.R.lift(t); s.L.pose = 4; } });
  check(s.now() !== 'sky', 'only one shoe pointing up is not SKY'); }

console.log('Shuffle / slide');
{ const s = new Scene(); const t0 = s.t;
  s.run(2000, t => { if ((t - t0) % 400 === 0 && t - t0 <= 1600) s.L.fire(4); });
  check(s.has('shuffle'), 'SHUFFLE'); }

console.log('Spin');
{ const s = new Scene(); const t0 = s.t;
  s.run(1000, t => { if (t === t0 + 100) s.L.spins++; });
  check(s.count('spin') === 1, 'SPIN'); }
{ const s = new Scene(); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) s.L.spins++; if (t === t0 + 400) s.R.spins++; });
  check(s.count('spin') === 1, 'both feet spinning together = one SPIN'); }

console.log('Only one shoe connected');
{ const s = new Scene('L'); const t0 = s.t;
  s.run(1500, t => { if (t === t0 + 100) s.L.lift(t); if (t === t0 + 400) { s.L.land(t); s.L.fire(1); } });
  check(s.has('stomp') && !s.has('hop') && !s.has('jump'), 'two-foot moves are off; a stomp still works'); }

console.log('Only one movement at a time, quick moves hold, then lasting ones come back');
{ const s = new Scene(); const t0 = s.t;
  s.run(3000, t => { if (t === t0 + 100) { s.L.pose = 1; } if (t === t0 + 1500) s.R.fire(2); if (t === t0 + 1700) s.R.fire(2); });
  const k = s.keys();
  check(k.indexOf('tilt') < k.indexOf('toetap'), 'tilt, then toe tap cuts in');
  check(s.now() === 'tilt', 'back to tilt after the toe tap'); }

console.log('Test buttons');
{ const s = new Scene(); s.det.test('jump', s.t); s.run(100); check(s.now() === 'jump', 'Test jump'); s.det.test('dancer', s.t); s.run(500); check(s.now() === 'dancer', 'Test dancer holds'); s.run(3000); check(s.now() === 'still', 'then back to still'); }

console.log(`\n${failures ? 'SOME TESTS FAILED' : 'ALL PASSED'} (${failures} failed)`);
process.exit(failures ? 1 : 0);

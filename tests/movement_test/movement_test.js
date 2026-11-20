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

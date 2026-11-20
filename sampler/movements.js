// Sound Effect Shoes: which movement is happening, from both shoes' packets.
// Used by index.html, and by tests/movement_test on a laptop (node).
//
// The shoes (xiao_foot) already report, per foot: steps (with time in the air
// before each), walk / run / still, spins, the pose (flat, tiptoe, heel, side,
// sky) and one-foot events (stomp, toe tap, kick, shuffle). This file combines
// both feet into the 15 movements and picks the ONE that plays.
(function (root) {
'use strict';

// =====================================================================
//  App-side thresholds, all in one place. Times in milliseconds.
//  One-foot thresholds (stomp, toe tap, kick, shuffle, tilt angles) are in
//  xiao_foot/config.h. None of these are tested on real feet yet.
// =====================================================================
const TUNING = {
  LIVE_MS: 1000,                // no packet from a shoe for this long = shoe not connected

  // Walking / running: with both shoes, the feet have to take turns
  TURN_MIN_MS: 150,             // a left step and a right step closer than this = both feet together, not a turn
  TURN_MAX_MS: 1500,            // further apart than this = not a turn
  TURN_WINDOW_MS: 3000,         // count turns in this window...
  TURNS_NEEDED: 2,              // ...this many = walking (running if a shoe says RUN)
  STOP_MS: 2000,                // no step for this long = not walking any more

  SPIN_TOGETHER_MS: 1200,       // both feet spinning this close together = one spin

  EVENT_HOLD_MS: 900,           // a quick move (spin, jump, hop, kick, lunge, stomp, toe tap) stays "now" this long

  // Jump with both feet: both in the air together, then both land together
  JUMP_LAND_TOGETHER_MS: 200,   // the two landings at most this far apart
  JUMP_MIN_AIR_MS: 120,         // both feet in the air together at least this long...
  JUMP_MAX_AIR_MS: 1000,        // ...and at most this long (longer = feet coming down from the sky pose, not a jump)

  // Hop on one foot: one foot lands while the other has stayed up the whole time
  HOP_MIN_AIR_MS: 120,          // the hopping foot was in the air at least this long...
  HOP_MAX_AIR_MS: 900,          // ...and at most this long
  HOP_OTHER_UP_SLACK_MS: 120,   // the other foot lifted no later than this after the hop took off

  // Skipping: hops that switch feet (step-hop, step-hop)
  SKIP_WINDOW_MS: 1500,         // a hop on one foot, then a hop on the other within this long = skipping
  SKIP_HOLD_MS: 1600,           // keeps skipping this long after each hop

  // Giant step / lunge: a long step, then the back foot stays up on its toes
  LUNGE_MIN_AIR_MS: 450,        // the stepping foot was in the air at least this long
  LUNGE_HOLD_MS: 400,           // after landing, the back foot stays TIPTOE this long (and doesn't step)

  // Dancer's pose: balance on one leg
  DANCER_HOLD_MS: 1500,         // one foot down and the other up, both for at least this long

  // Both feet to the sky: lying down, both shoes pointing up and not pressed
  SKY_HOLD_MS: 800,             // both shoes SKY pose and unloaded for this long

  // Foot tilt (tiptoe forward, heel back, rolled sideways)
  TILT_HOLD_MS: 500,            // a shoe in TIPTOE / HEEL / SIDE pose this long, while not walking...
  TILT_QUIET_MS: 1200,          // ...and that foot hasn't stepped for this long

  SHUFFLE_HOLD_MS: 900,         // keeps shuffling this long after each shuffle report from a shoe

  OTHER_FOOT_DOWN_MS: 300,      // a stomp only counts if the other foot has been down this long (else it's a jump/hop landing)

  TEST_STATE_MS: 2500,          // the Test button holds a lasting movement this long
};

// The 15 movements. kind 'state' = lasts while you do it (sound loops by default),
// 'event' = a quick move (sound plays once). feet 2 = needs both shoes connected.
const MOVES = [
  {key:'still',   n:1,  name:'Standing still', kind:'state', feet:1, how:'No other movement. Sound loops while you stand.'},
  {key:'walk',    n:2,  name:'Walking',        kind:'state', feet:1, how:'Left and right steps taking turns.'},
  {key:'run',     n:3,  name:'Running',        kind:'state', feet:1, how:'Fast steps (a shoe says RUN).'},
  {key:'spin',    n:4,  name:'Spin',           kind:'event', feet:1, how:'A full 360 turn, one or both feet.'},
  {key:'jump',    n:5,  name:'Jump (both feet)', kind:'event', feet:2, how:'Both feet leave the ground and land together.'},
  {key:'hop',     n:6,  name:'Hop on one foot', kind:'event', feet:2, how:'One foot lands while the other stays up.'},
  {key:'skip',    n:7,  name:'Skipping',       kind:'state', feet:2, how:'Hops that switch feet: step-hop, step-hop.'},
  {key:'kick',    n:8,  name:'Karate kick',    kind:'event', feet:1, how:'A fast, hard swing of a foot in the air.'},
  {key:'lunge',   n:9,  name:'Giant step / lunge', kind:'event', feet:2, how:'A long step, back foot stays up on its toes.'},
  {key:'dancer',  n:10, name:"Dancer's pose",  kind:'state', feet:2, how:'Balance on one leg, other foot up.'},
  {key:'stomp',   n:11, name:'Stomp',          kind:'event', feet:1, how:'A hard hit on the floor, other foot standing.'},
  {key:'toetap',  n:12, name:'Toe tap',        kind:'event', feet:1, how:'Toes tap the floor 2+ times, heel down.'},
  {key:'tilt',    n:13, name:'Foot tilt',      kind:'state', feet:1, how:'Tiptoe, heel back, or rolled sideways, held.'},
  {key:'sky',     n:14, name:'Feet to the sky', kind:'state', feet:2, how:'Lying down, both shoes pointing up, not pressed.'},
  {key:'shuffle', n:15, name:'Shuffle / slide', kind:'state', feet:1, how:'Sliding a foot along the floor without stepping.'},
];
const MOVE = Object.fromEntries(MOVES.map(m => [m.key, m]));

// Must match xiao_foot/motion.h
const STILL = 0, WALK = 1, RUN = 2, SPIN = 3;
const GAIT_NAMES = ['STILL', 'WALK', 'RUN', 'SPIN'];
const POSE_FLAT = 0, POSE_TIPTOE = 1, POSE_HEEL = 2, POSE_SIDE = 3, POSE_SKY = 4;
const POSE_NAMES = ['FLAT', 'TIPTOE', 'HEEL', 'SIDE', 'SKY'];
const EVENT_STOMP = 1, EVENT_TOE_TAP = 2, EVENT_KICK = 3, EVENT_SHUFFLE = 4;
const EVENT_NAMES = ['-', 'STOMP', 'TOE TAP', 'KICK', 'SHUFFLE'];
const FLAG_DOWN = 0x02;
const FOOT_NAME = {L:'left', R:'right'};
const OTHER = {L:'R', R:'L'};

function newFoot(id){
  return { id, last:-1e9, d:null, down:false, downSince:0, upSince:0, state:STILL, pose:POSE_FLAT, poseSince:0,
    steps:0, spins:0, events:0, lastLand:null, lastEvent:0 };
}

class MovementDetector {
  constructor(opts = {}){
    this.T = Object.assign({}, TUNING, opts.tuning || {});
    this.requireTurns = opts.requireTurns !== undefined ? opts.requireTurns : true;
    this.onLog = opts.onLog || (() => {});
    this.reset();
  }

  reset(){
    this.feet = {L:newFoot('L'), R:newFoot('R')};
    this.lastStep = null; this.turns = [];
    this.seq = 0;
    this.current = {key:'still', why:'start', since:0, isEvent:false, id:0};
    this.eventUntil = -1e9;
    this.lastSpin = {t:-1e9, foot:null};
    this.pending = [];
    this.lastHop = {L:-1e9, R:-1e9};
    this.skipUntil = -1e9; this.skipWhy = '';
    this.shuffleUntil = -1e9; this.shuffleWhy = '';
    this.forced = null;
    this.counts = Object.fromEntries(MOVES.map(m => [m.key, 0]));
  }

  isLive(id, now){ return now - this.feet[id].last < this.T.LIVE_MS; }
  bothLive(now){ return this.isLive('L', now) && this.isLive('R', now); }
  log(text){ this.onLog(text); }

  // ---------- every packet from a shoe comes in here ----------
  ingest(id, d, now){
    const f = this.feet[id], down = !!(d.flags & FLAG_DOWN);
    const fresh = !this.isLive(id, now);
    f.last = now; f.d = d; f.state = d.state;
    if (fresh){
      // first packet (or after a gap): take the counters as they are
      f.down = down; f.downSince = f.upSince = now;
      f.steps = d.steps; f.spins = d.spins; f.events = d.events;
      f.pose = d.pose; f.poseSince = now;
      return;
    }
    if (down !== f.down){ f.down = down; if (down) f.downSince = now; else f.upSince = now; }
    if (d.pose !== f.pose){ f.pose = d.pose; f.poseSince = now; }

    const newSteps = (d.steps - f.steps + 65536) % 65536; f.steps = d.steps;
    if (newSteps > 0 && newSteps < 10) this._landing(id, now, d.air10 * 10);
    const newSpins = (d.spins - f.spins + 256) % 256; f.spins = d.spins;
    if (newSpins > 0 && newSpins < 10) this._spin(id, now);
    const newEvents = (d.events - f.events + 256) % 256; f.events = d.events;
    if (newEvents > 0 && newEvents < 10) this._footEvent(id, d.event, now, d);
  }

  // ---------- a foot pressed down (a step or a landing) ----------
  _landing(id, now, air){
    this._recordTurn(id, now);
    const T = this.T, f = this.feet[id], o = this.feet[OTHER[id]];
    f.lastLand = {t:now, air, used:false};
    if (!this.bothLive(now)) return;
    const ol = o.lastLand;
    if (ol && !ol.used && now - ol.t <= T.JUMP_LAND_TOGETHER_MS){
      const together = Math.min(now, ol.t) - Math.max(now - air, ol.t - ol.air);
      if (together >= T.JUMP_MIN_AIR_MS && together <= T.JUMP_MAX_AIR_MS){
        f.lastLand.used = ol.used = true;
        this._fire('jump', now, `both feet landed ${Math.round(now - ol.t)} ms apart, ${Math.round(together)} ms in the air together`);
        return;
      }
    }
    this.pending.push({kind:'land', id, t:now, air});
  }

  _decidePending(now){
    const T = this.T, keep = [];
    for (const p of this.pending){
      const f = this.feet[p.id], o = this.feet[OTHER[p.id]], name = FOOT_NAME[p.id];
      if (p.kind === 'land'){
        if (now - p.t < T.JUMP_LAND_TOGETHER_MS){ keep.push(p); continue; }   // wait: the other foot may still land (jump)
        if (!f.lastLand || f.lastLand.t !== p.t || f.lastLand.used || !this.bothLive(now)) continue;
        const takeoff = p.t - p.air;
        const otherStayedUp = !o.down && o.upSince <= takeoff + T.HOP_OTHER_UP_SLACK_MS;
        if (p.air >= T.HOP_MIN_AIR_MS && p.air <= T.HOP_MAX_AIR_MS && otherStayedUp) this._hop(p.id, p.t, p.air);
        else if (p.air >= T.LUNGE_MIN_AIR_MS) keep.push({kind:'lunge', id:p.id, t:p.t, air:p.air});
      } else if (p.kind === 'lunge'){
        if (now - p.t < T.LUNGE_HOLD_MS){ keep.push(p); continue; }
        if (!this.bothLive(now)) continue;
        const stayed = f.down && f.lastLand && f.lastLand.t === p.t;
        const backOnToes = o.pose === POSE_TIPTOE && o.poseSince <= now - T.LUNGE_HOLD_MS / 2;
        const backStayed = !o.lastLand || o.lastLand.t < p.t;
        if (stayed && backOnToes && backStayed && this.gait(now) === 'still')
          this._fire('lunge', now, `${name} foot took a long step (${p.air} ms in the air), ${FOOT_NAME[o.id]} foot stayed on its toes`);
      }
    }
    this.pending = keep;
  }

  _hop(id, now, air){
    const T = this.T, other = OTHER[id];
    if (now - this.lastHop[other] <= T.SKIP_WINDOW_MS){
      if (now >= this.skipUntil) this.log(`skipping: ${FOOT_NAME[other]} hop, then ${FOOT_NAME[id]} hop`);
      this.skipUntil = now + T.SKIP_HOLD_MS;
      this.skipWhy = `hops switching feet (${FOOT_NAME[id]} just hopped, ${air} ms in the air)`;
    } else {
      this._fire('hop', now, `${FOOT_NAME[id]} foot hopped (${air} ms in the air), ${FOOT_NAME[other]} foot stayed up`);
    }
    this.lastHop[id] = now;
  }

  _spin(id, now){
    if (this.lastSpin.foot && this.lastSpin.foot !== id && now - this.lastSpin.t < this.T.SPIN_TOGETHER_MS){
      this.log(`${FOOT_NAME[id]} foot spun too: both feet together count as one spin`);
      this.lastSpin = {t:-1e9, foot:null};
      return;
    }
    this.lastSpin = {t:now, foot:id};
    this._fire('spin', now, `${FOOT_NAME[id]} foot turned 360`);
  }

  // ---------- one-foot events from the shoe, checked against the other foot ----------
  _footEvent(id, ev, now, d){
    const T = this.T, o = this.feet[OTHER[id]], both = this.bothLive(now);
    const name = FOOT_NAME[id], oname = FOOT_NAME[o.id], gait = this.gait(now);
    const ignore = why => this.log(`${name} ${EVENT_NAMES[ev] || ev} ignored: ${why}`);
    this.feet[id].lastEvent = ev;
    if (ev === EVENT_STOMP){
      if (both && !(o.down && now - o.downSince >= T.OTHER_FOOT_DOWN_MS)) return ignore(`${oname} foot was in the air too (a jump or hop landing)`);
      if (gait === 'run') return ignore('running');
      this._fire('stomp', now, `${name} foot hit the floor hard (${d.peakG ? d.peakG.toFixed(1) + ' g' : 'big hit'})`);
    } else if (ev === EVENT_TOE_TAP){
      if (both && !o.down) return ignore(`${oname} foot is in the air`);
      if (gait !== 'still') return ignore(gait === 'run' ? 'running' : 'walking');
      this._fire('toetap', now, `${name} toes tapped`);
    } else if (ev === EVENT_KICK){
      if (both && !o.down) return ignore(`${oname} foot is in the air too (a jump?)`);
      if (gait === 'run') return ignore('running');
      this._fire('kick', now, `${name} foot swung fast in the air`);
    } else if (ev === EVENT_SHUFFLE){
      if (gait !== 'still') return ignore(gait === 'run' ? 'running' : 'walking');
      this.shuffleUntil = now + T.SHUFFLE_HOLD_MS;
      this.shuffleWhy = `${name} foot sliding along the floor`;
    }
  }

  // ---------- walking / running: feet taking turns ----------
  _recordTurn(id, t){
    const T = this.T, last = this.lastStep;
    const gap = last ? t - last.t : Infinity, otherFoot = last && last.foot !== id;
    if (otherFoot && gap < T.TURN_MIN_MS){
      if (last.turn) this.turns.pop();   // both feet landed together: neither is a turn
      this.lastStep = {foot:id, t, turn:false, together:true};
      return;
    }
    const turn = otherFoot && !last.together && gap <= T.TURN_MAX_MS;
    if (turn) this.turns.push(t);
    this.lastStep = {foot:id, t, turn, together:false};
    while (this.turns.length && this.turns[0] < t - T.TURN_WINDOW_MS) this.turns.shift();
  }
  recentTurns(now){ return this.turns.filter(t => t > now - this.T.TURN_WINDOW_MS).length; }
  takingTurns(now){ return !!this.lastStep && now - this.lastStep.t < this.T.STOP_MS && this.recentTurns(now) >= this.T.TURNS_NEEDED; }
  usingTurns(now){ return this.requireTurns && this.bothLive(now); }
  footGait(id, now){ return this.isLive(id, now) ? this.feet[id].state : STILL; }

  gait(now){
    const s = ['L','R'].map(id => this.footGait(id, now));
    if (this.usingTurns(now)){
      if (!this.takingTurns(now)) return 'still';
      return s.includes(RUN) ? 'run' : 'walk';
    }
    if (s.includes(RUN)) return 'run';
    if (s.includes(WALK)) return 'walk';
    return 'still';
  }

  // ---------- lasting movements, most important first ----------
  _lasting(now){
    const T = this.T, L = this.feet.L, R = this.feet.R, both = this.bothLive(now);
    if (this.forced && now < this.forced.until) return [this.forced.key, 'test button'];
    if (both){
      const sky = f => !f.down && f.pose === POSE_SKY && now - f.poseSince >= T.SKY_HOLD_MS && now - f.upSince >= T.SKY_HOLD_MS;
      if (sky(L) && sky(R)) return ['sky', 'both shoes pointing up and not pressed'];
      for (const [a, b] of [[L, R], [R, L]]){
        if (a.down && now - a.downSince >= T.DANCER_HOLD_MS && !b.down && now - b.upSince >= T.DANCER_HOLD_MS && b.pose !== POSE_SKY)
          return ['dancer', `balancing on the ${FOOT_NAME[a.id]} foot, ${FOOT_NAME[b.id]} foot up`];
      }
    }
    if (now < this.skipUntil) return ['skip', this.skipWhy];
    if (now < this.shuffleUntil) return ['shuffle', this.shuffleWhy];
    const g = this.gait(now);
    if (g === 'run') return ['run', 'fast steps'];
    if (g === 'walk') return ['walk', this.usingTurns(now) ? `feet taking turns (${this.recentTurns(now)} in 3 s)` : 'steps'];
    for (const f of [L, R]){
      if (!this.isLive(f.id, now)) continue;
      const tilted = f.pose === POSE_TIPTOE || f.pose === POSE_HEEL || f.pose === POSE_SIDE;
      const quiet = !f.lastLand || now - f.lastLand.t >= T.TILT_QUIET_MS;
      if (tilted && quiet && now - f.poseSince >= T.TILT_HOLD_MS) {
        const what = {[POSE_TIPTOE]:'on tiptoe', [POSE_HEEL]:'on the heel, toes up', [POSE_SIDE]:'rolled sideways'}[f.pose];
        const angle = f.d ? ` (pitch ${f.d.pitch}°, roll ${f.d.roll}°)` : '';
        return ['tilt', `${FOOT_NAME[f.id]} foot ${what}${angle}`];
      }
    }
    return ['still', 'no other movement'];
  }

  _fire(key, now, why){
    this.current = {key, why, since:now, isEvent:true, id:++this.seq};
    this.eventUntil = now + this.T.EVENT_HOLD_MS;
    this.counts[key]++;
    this.forced = null;
    this.log(`${MOVE[key].n} ${MOVE[key].name.toUpperCase()}: ${why}`);
  }

  // Call often (every 20-50 ms). Returns the one movement that is "now".
  // A new id means a new sound should start.
  update(now){
    this._decidePending(now);
    if (now < this.eventUntil) return this.current;
    const [key, why] = this._lasting(now);
    if (this.current.isEvent || key !== this.current.key){
      this.current = {key, why, since:now, isEvent:false, id:++this.seq};
      this.counts[key]++;
      this.log(`${MOVE[key].n} ${MOVE[key].name.toUpperCase()}: ${why}`);
    } else this.current.why = why;
    return this.current;
  }

  // The Test button on each sound: pretend the movement happened.
  test(key, now){
    if (MOVE[key].kind === 'event') this._fire(key, now, 'test button');
    else { this.forced = {key, until:now + this.T.TEST_STATE_MS}; this.eventUntil = -1e9; this.log(`test: ${MOVE[key].name}`); }
  }
}

const api = { TUNING, MOVES, MOVE, MovementDetector, GAIT_NAMES, POSE_NAMES, EVENT_NAMES };
if (typeof module !== 'undefined' && module.exports) module.exports = api;
else root.Moves = api;
})(typeof window !== 'undefined' ? window : globalThis);

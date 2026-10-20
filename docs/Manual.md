# SB-64 – Manual

*Deutsch: [Handbuch](Handbuch.md)*

![SB-64 front panel](images/panel.png)

SB-64 is an analog-style monophonic acid bass synthesizer with a built-in 64-step sequencer. One
oscillator, a sub, noise, a nasty two-pole filter and a sequencer that knows gate, accent, slide, cutoff
offset, pitch envelope, probability and fine tuning per step – all on a single front panel, no
sub-pages to click through.

> **Status:** in development. Everything described here is built and tested; sound tuning is still to
> come.

---

## Contents

1. [Why SB-64?](#1-why-sb-64)
2. [Installation](#2-installation)
3. [First steps](#3-first-steps)
4. [How the sound is made](#4-how-the-sound-is-made)
5. [The front panel](#5-the-front-panel)
6. [The menu (SELECT / NOTE)](#6-the-menu-select--note)
7. [Sequencer](#7-sequencer)
8. [SHIFT functions](#8-shift-functions)
9. [Scales and user scales](#9-scales-and-user-scales)
10. [Arpeggiator](#10-arpeggiator)
11. [Patterns, banks and chains](#11-patterns-banks-and-chains)
12. [MIDI and host sync](#12-midi-and-host-sync)
13. [Things you have to try](#13-things-you-have-to-try)
14. [Parameter list](#14-parameter-list)
15. [Troubleshooting](#15-troubleshooting)

---

## 1. Why SB-64?

Most acid plugins want to recreate *that one* silver box from the 80s. SB-64 doesn't. It follows a
compact analog bassline box from 2020 that deliberately sounds different – and that's the point:

- **The filter bites.** Not a smooth ladder, but a two-pole state-variable filter after a rough 80s
  circuit: integrators that can only move so fast (slew limiting), a bit of crossover distortion,
  resonance up to self-oscillation – **without losing the bass**. Dirty, unstable, alive. Low-pass *and*
  band-pass.
- **DRIVE isn't a gain knob.** It crossfades from the clean signal into an overdrive stage: slightly
  rough at the bottom, completely broken at the top – and the lows get fatter, not thinner.
- **DETUNE isn't a second oscillator.** Two modulated delay lines in the style of old bucket-brigade
  chips sit on the signal. Not a clean beating but a pulsing swarm – from light chorus to supersaw mush.
- **The LFO goes into audio range.** Through FM LVL it modulates the pitch up to 2 kHz: metallic, bell-like,
  broken. Plus ten shapes, three of them odd rhythmic ones.
- **The slide has its own feel.** Not the classic glide but a curve that starts fast and lands softly –
  and how far it reaches depends on the gate.
- **The sequencer is a groovebox of its own:** 64 steps, probabilities and "only every eighth pass"
  conditions, five play directions, scales including your own microtonal ones, arpeggiator, random
  generator, pattern chains. Lines come from playing around, not from programming.
- **Drone mode.** VCA ENV all the way up – the note never ends. Perfect for filter tweaking.

In short: SB-64 is for everyone who loves acid but doesn't want yet another clone.

---

## 2. Installation

SB-64 comes as **AU** (macOS only), **VST3**, **CLAP** and as a **standalone app**.

| System | Folder |
|---|---|
| macOS AU | `~/Library/Audio/Plug-Ins/Components/` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| macOS standalone | `~/Applications/` |
| Windows VST3 | `C:\Program Files\Common Files\VST3\` |
| Windows CLAP | `C:\Program Files\Common Files\CLAP\` |
| Linux VST3 / CLAP | `~/.vst3/` / `~/.clap/` |

Then let your DAW rescan its plugins. SB-64 is an instrument (synth, mono).

### Unblocking plugins on macOS

macOS likes to block downloaded plugins that aren't notarized by Apple ("can't be opened", "is
damaged"), or the DAW silently skips them. Fix it once in **Terminal**, then restart the DAW:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/SB-64.component ~/Library/Audio/Plug-Ins/VST3/SB-64.vst3 ~/Library/Audio/Plug-Ins/CLAP/SB-64.clap
```

Only do this for sources you trust.

---

## 3. First steps

1. Put SB-64 on an instrument track.
2. Press **PLAY** (the triangle). The first pattern **A1** holds a short acid line.
   - In a plugin the clock defaults to **host**: PLAY blinks until the DAW runs. The standalone app
     plays right away at its own tempo.
3. Turn **CUTOFF**, **RESONANCE**, **VCF ENV** and **ENV/CV LVL** – that's the acid core.
4. Slowly bring up **DRIVE**. Then **DETUNE**.
5. Click a step key: the step comes on and blinks. **BACK/NOTE** turns its note, **SELECT/GATE** its
   length. Another click on the same step switches it off.

Mouse helpers:

- Knobs: drag; **Shift** for fine; **double-click** = default; the mouse wheel works too.
- Encoders (SELECT/GATE, BACK/NOTE): drag or wheel, **Shift** for fine, **click** = push button.
- The panel's **SHIFT** latches: press it once (it lights), then a step key. Or click a step key while
  holding the computer's Shift key.
- Touching a knob shows its value on the display for a moment.

---

## 4. How the sound is made

```
VCO (Square / Saw / Tri) ──► Detune ─┐
Sub (square, −1 oct)  ───────────────┼──► Filter (LP/BP) ──► VCA ──► Drive ──► Out
Noise ───────────────────────────────┘        ▲     ▲         ▲
                                    VCF ENV ──┘     │         └── VCA ENV
                         LFO ──► cutoff (LFO LVL)   └── MOD track / accent
                         LFO ──► pitch (FM LVL)
```

- **Accent** makes the note louder, opens the filter a bit further and stretches both envelopes. It's
  on or off, not an amount.
- **Slide** glides to the next note while the envelopes keep running. **Gate 100 %** joins two notes
  without a new attack (legato), but without gliding.
- The **pitch envelope** jumps up at the start of the note (up to three octaves) and falls back fast –
  the classic "peow".

---

## 5. The front panel

### Top row

| Control | What it does |
|---|---|
| **DRIVE** | Crossfade clean ↔ overdrive. |
| **DETUNE** | Amount of the two modulated delay lines (swarm/chorus). |
| **VCA ENV** | Amp decay, 40 ms to 5 s. Fully right **INF**: drone, the note holds forever. |
| **Wave switch** | Square / saw / triangle. |
| **Filter switch** | **BP** (band-pass) / **LP** (low-pass). |
| **VCF ENV** | Filter envelope decay, 30 ms to 3 s. |
| **RESONANCE** | Filter resonance up to self-oscillation. |
| **CUTOFF** | Filter frequency, about 30 Hz to 17.6 kHz. |

### Bottom row

| Control | What it does |
|---|---|
| **FM LVL** | How much the LFO modulates the pitch. |
| **SUB LVL** | Level of the sub oscillator (square one octave down). |
| **NOISE LVL** | White noise into the filter. |
| **SELECT/GATE** | Encoder: with a step selected, its gate (or probability/cents, see below); otherwise it scrolls the menu. |
| **BACK/NOTE** | Encoder: note of the selected step, otherwise the value of the menu item. **Click = BACK**. |
| **LFO RATE** | LFO speed, free 0.05 Hz to 2 kHz – or in note values with SYN on. |
| **LFO LVL** | How much the LFO modulates the filter. |
| **ENV/CV LVL** | How far the filter envelope opens the cutoff. |

### Display and LEDs

- The three-digit display normally shows the **tempo**; while turning, the value; in the menu, name and
  value; while editing a step, its note.
- **Red LED** left of the wave switch: gate. **Yellow LED** right of the filter switch: accent – and it
  flashes on clipping. **Red LED** at LFO RATE: blinks with the LFO.

### Buttons

| Button | Function |
|---|---|
| **SHIFT** | Second functions of the step keys (latches). |
| **▷ PLAY** | Start the sequencer. Blinks while waiting for the host. |
| **□ STOP** | Stop. Press again while stopped: back to the start. |
| **○ REC** | Recording on/off. **SHIFT + REC**: keyboard mode. |
| **◂ BAR ▸** | Which of the four bars (16 steps) is on the keys. |
| **−1 OCT / +1 OCT** | Moves the selected step by an octave – otherwise the octave of the key keyboard. |

---

## 6. The menu (SELECT / NOTE)

Everything without its own knob lives in the menu. Turn **SELECT/GATE** (with no step selected) to
scroll; the display shows the name for a moment (e.g. `SWG`), then the value. **BACK/NOTE** changes the
value. The menu closes after five seconds without input; a click on BACK/NOTE closes it at once.

Order: `BPM CLK SWG PLY TRP SCL KEY MSL ARP ADR AOC AGT ARH ASR DNS RAC RSL ROC RMD LFO SYN RNG RST PHS
SLD ACC PED PET BND TUN VOL`. What each one does: [parameter list](#14-parameter-list).

Many SHIFT functions jump straight to the right item (SHIFT + LFO → `LFO`, SHIFT + PLAY MODE → `PLY`
and so on).

---

## 7. Sequencer

### Writing steps

The 16 step keys show one bar of the selected pattern. Lit = the step plays, bright = playing now. Use
**◂ BAR ▸** for bars 2–4; while the sequencer runs, the view follows the playhead until you press BAR.

- **Click an empty step:** switches it on and selects it (blinks).
- **NOTE** turns its note (C0–C6), **±1 OCT** jumps octaves, **GATE** sets its length (10–100 %).
- **Click SELECT/GATE** to switch what GATE changes:
  `GAT` (gate) → `PRB` (probability) → `CNT` (fine tune ±50 cents) → back to `GAT`.
- **Click the selected step again:** switches it off.
- **BACK** leaves the step.

### Probability (PRB)

| Value | Meaning |
|---|---|
| `ALL` | always plays |
| `10` `25` `50` `75` `90` | plays with this probability in % |
| `1/8` … `7/8` | plays on the first 1…7 of every eight passes – `1/8` = only every eighth pass, `7/8` = rests on the eighth |

### Length, direction, tempo

- **SHIFT + LAST STEP**, then a step key: the pattern's last step (works in bars 2–4 too).
- **SHIFT + PLAY MODE**: direction `FWD` forward, `BWD` backward, `RND` random, `PP` ping-pong (ends
  once), `PP2` ping-pong with doubled ends.
- Tempo `BPM` 20–480 (own clock), `SWG` swing 0–50 % on every second 16th.

### Recording (REC)

With **REC** the step keys become a keyboard (from C2, ±1 OCT shifts it). Computer Shift while
clicking = accent. Notes from a MIDI keyboard work the same way.

- **Stopped:** step recording. Each note goes into the next step; the display shows the target (`R5`).
  Starts at the selected step.
- **Running:** live recording, quantised to the nearest step.
- With the **arpeggiator** on, its notes get recorded – write lines with the arp.

### Keyboard mode

**SHIFT + REC** turns the step keys into a keyboard without recording (`KBD`). SHIFT + REC again or
BACK ends it. Playing legato (hold a key, press the next) glides; how long is set by **MSL** (master
slide) in the menu: `OFF` = slide time `SLD`, 10–100 = share of a 16th at the tempo.

---

## 8. SHIFT functions

| Key | SHIFT + key |
|---|---|
| 1 **ACC** | Keys set/clear accents. BACK ends it. |
| 2 **SLIDE** | Keys set/clear slides. |
| 3 **MOD** | A key selects a step, **NOTE** sets its cutoff offset −64…+63 (SELECT in coarse steps). Steps with an offset light up. |
| 4 **PITCH ENV** | A key selects a step, **NOTE** = depth 1–9 (`PE5`), **SELECT** = decay 0–9 (`DC4`). Depth off = the menu's pitch envelope (`PED`/`PET`). |
| 5 **LAST STEP** | Then a step key: the last step. |
| 6 **PLAY MODE** | Opens `PLY`. |
| 7 **TRANSP** | Opens `TRP`: transposes the sequencer (±24 semitones) and the arp. |
| 8 **SCALE** | Factory scale: opens `SCL`. User scale: scale editor (see below). |
| 9 **ARPEGG** | Arpeggiator on/off. |
| 10 **LFO** | Opens the LFO settings (`LFO`, then scroll on). |
| 11 **RND PATT** | Rolls a new pattern (between first and last step). |
| 12 **CLEAR** | Then CLEAR again: clears the pattern. Any other key cancels. |
| 13 **COPY** | Copies the pattern. |
| 14 **PASTE** | Pastes the copied pattern. |
| 15 **BANK** | Then key 1–8: bank A–H. |
| 16 **PATTERN** | Then key 1–16: pattern of the bank. More keys right after build a chain. |

**BACK** (click on BACK/NOTE) steps out one level at a time: waiting function → menu → selected step →
key mode (ACC/SLIDE/MOD/…) → keyboard mode.

---

## 9. Scales and user scales

`SCL` picks the scale, `KEY` the root. All notes of the sequencer (after transposing), the random
generator and the arp (source `SCL`) move down to the nearest note of the scale.

Factory scales: `CHR` chromatic, `MAJ` major, `MIN` minor, `PMA` major pentatonic, `PMI` minor
pentatonic, `WHL` whole tone, `BMA` major blues, `BMI` minor blues.

### User scales U1–U16 (microtonal too)

1. Set `SCL` in the menu to `U1`…`U16`.
2. **SHIFT + SCALE**: keys 1–12 are now the twelve semitones from the root (key 1 = root). Lit = in
   the scale.
3. One press on a note picks it (blinks), a second press takes it out. A press on a note that's out
   puts it back. The root always stays in.
4. **NOTE** detunes the picked note by up to ±50 cents (Shift for 1-cent steps). Think maqam scales,
   just intonation, odd gamelan-like ladders …
5. BACK ends the editor.

User scales are saved with the project.

---

## 10. Arpeggiator

**SHIFT + ARPEGG** switches it on. It then plays held MIDI notes (or the step keys in keyboard mode) in
16ths at the sequencer tempo, starting with the first key press.

| Menu | Values |
|---|---|
| `ADR` direction | `UP`, `DN`, `UD` (up and down), `RND` |
| `AOC` octaves | 1–3 |
| `AGT` gate | 10–100 % |
| `ARH` rhythm | 1 every 16th · 2 eighths · 3 two on, one off · 4 3+3+2 · 5 three on, one off · 6 one off, three on |
| `ASR` source | `KEY` the held notes · `SCL` the selected scale from the lowest held note |

`TRP` transposes the arp too. With **REC** its notes go into the pattern.

---

## 11. Patterns, banks and chains

- 8 banks (A–H) × 16 patterns = 128 patterns, all **saved with the project**.
- **SHIFT + BANK**, key 1–8 changes the bank (same pattern number).
  **SHIFT + PATTERN**, key 1–16 changes the pattern. Lit keys = patterns with content.
- While the sequencer runs, the new pattern waits for the end of the playing one (its key blinks).
- **Chain:** after SHIFT + PATTERN and the first key, press more keys within 2.5 seconds (display `CHN`,
  `C2`, `C3` …). The patterns then play one after the other, round and round. A normal pattern change
  ends the chain. Chains aren't saved.
- **COPY / PASTE** copy whole patterns, across banks too.

### Random generator (RND PATT)

SHIFT + RND PATT refills the steps between the first and last step – with notes of the scale (`SCL`,
`KEY`) from C2. Set the mix in the menu first:

| Menu | Meaning |
|---|---|
| `DNS` | density: share of steps that play (10–90 %) |
| `RAC` | share with accent |
| `RSL` | share with slide |
| `ROC` | note range 1–3 octaves |
| `RMD` | random cutoff offset up to ± this value (0 = off) |

Tip: like a pattern? **COPY** it first – then keep rolling and go back.

---

## 12. MIDI and host sync

- **MIDI notes** play SB-64 directly: mono, last note wins, legato glides. Velocity from `ACC` (default
  100) = accent. **Pitch bend** with range `BND`.
- **Clock** `CLK`: `INT` = own tempo `BPM`; `HST` = follows the DAW's tempo and position (loops and
  locating included). With HST, PLAY waits until the DAW runs.
- With `SYN` on the **LFO** runs in time: `RNG LO` 1/64 to 8 cycles per quarter, `RNG HI` 1/16 to 32.
  `RST` restarts it with every new note, `PHS` sets the start phase (0–315°).

---

## 13. Things you have to try

1. **The drone.** VCA ENV fully right (`INF`), hold a note, then only CUTOFF, RESONANCE and DRIVE. This is
   where you hear what the filter can do.
2. **Whistling filter.** RESONANCE almost full, filter on BP, triangle wave, sweep CUTOFF slowly. The bass
   stays, the resonance sings on top.
3. **Metal FM.** LFO RATE far right (audio range), shape `SIN` or `SQR`, bring up FM LVL slowly. From bell
   to radio interference to industrial.
4. **Swarm bass.** Saw, DETUNE halfway, add SUB LVL, DRIVE a third. Fat and pulsing.
5. **Peow percussion.** SHIFT + PITCH ENV, depth 6–9 with a short decay on a few steps. Toms and zaps
   right inside the bassline.
6. **Fake hi-hat.** NOISE LVL up, CUTOFF wide open, DRIVE high, VCA ENV very short. One step with accent.
7. **Rhythmic filter LFO.** LFO shape `TAR`, `TDS` or `DST` (stepped ramps), `SYN` on, LFO LVL halfway,
   `RST` on. Every note gets its own filter movement.
8. **Living lines with probability.** A few steps on `50` or `75`, one on `1/8` (a fill only every
   eighth pass). Never the same twice, always recognisable.
9. **Polymeter.** LAST STEP on 13 or 7, plus `PP2` – against a 4/4 drum loop the bassline keeps shifting.
10. **Rolling with a scale.** `SCL MIN`, `KEY A`, `DNS 60`, `RSL 30`, then SHIFT + RND PATT until it hits.
    Don't forget COPY.
11. **Slide feel.** Slides on two steps in a row, then turn the first one's GATE: how far the slide reaches
    depends on the gate.
12. **Arp into the pattern.** ARPEGG on, `ARH 4` (3+3+2), REC on (stopped), hold a chord – instant
    syncopated line. Then edit single steps.
13. **Microtonal.** `SCL U1`, SHIFT + SCALE, take a few notes out, the third −14 cents, the seventh −31
    cents. Suddenly acid sounds like somewhere else.
14. **MOD track as filter accent.** SHIFT + MOD and +30…+60 on the offbeats – like a second accent just
    for the filter.

---

## 14. Parameter list

### Knobs and switches

| Name | Range |
|---|---|
| Drive, Detune, Resonance, FM Level, Sub Level, Noise Level, LFO Level, Env Amount | 0–100 % |
| VCA Env | 40 ms – 5 s, fully right INF |
| VCF Env | 30 ms – 3 s |
| Cutoff | about 30 Hz – 17.6 kHz |
| LFO Rate | 0.05 Hz – 2 kHz, synced in note values |
| Wave | Square, Saw, Triangle |
| Filter | Band-pass, Low-pass |

### Menu

| Display | Parameter | Range |
|---|---|---|
| `BPM` | tempo | 20–480 |
| `CLK` | clock | INT / HST |
| `SWG` | swing | 0–50 % |
| `PLY` | play mode | FWD, BWD, RND, PP, PP2 |
| `TRP` | transpose | ±24 semitones |
| `SCL` | scale | CHR … BMI, U1–U16 |
| `KEY` | root | C – B |
| `MSL` | master slide | OFF, 10–100 % of a 16th |
| `ARP` | arpeggiator | ON / OFF |
| `ADR` | arp direction | UP, DN, UD, RND |
| `AOC` | arp octaves | 1–3 |
| `AGT` | arp gate | 10–100 % |
| `ARH` | arp rhythm | 1–6 |
| `ASR` | arp source | KEY / SCL |
| `DNS` `RAC` `RSL` | random: density, accents, slides | % |
| `ROC` | random: octaves | 1–3 |
| `RMD` | random: cutoff offset | 0–64 |
| `LFO` | LFO shape | SIN, TRI, SAW, RMP, SQR, TDS, DST, TAR, S-H, NOS |
| `SYN` | LFO sync | ON / OFF |
| `RNG` | LFO range (synced) | LO / HI |
| `RST` | LFO restart per note | ON / OFF |
| `PHS` | LFO start phase | 0–315° |
| `SLD` | slide time (MIDI) | 10–500 ms |
| `ACC` | accent from velocity | 1–127 |
| `PED` | pitch envelope depth (MIDI / steps without their own) | 0–9 |
| `PET` | pitch envelope decay | 10–300 ms |
| `BND` | pitch bend range | 0–12 semitones |
| `TUN` | tuning | ±10 Hz around 440 Hz |
| `VOL` | volume | 0–100 |

### Per step

Note C0–C6 · gate 10–100 % · accent · slide · MOD −64…+63 · pitch envelope depth 0–9 and decay 0–9 ·
probability/condition · fine tune ±50 cents.

---

## 15. Troubleshooting

| Problem | Fix |
|---|---|
| PLAY blinks but nothing happens | The clock is on `HST` – start the DAW or set `CLK` to `INT`. |
| No sound from MIDI | Is the arp on? Then it only plays held notes. Check VOL. |
| The note never ends | VCA ENV is on `INF` (drone) – turn it back a little. |
| Keys do "strange" things | Still in a mode (ACC, MOD, scale, keyboard …)? Press **BACK** until the keys show steps again. |
| Yellow LED keeps flashing | Clipping – turn down VOL or DRIVE. |
| Pattern gone | Patterns live in the project. Project not saved? Then it's gone. Before RND PATT: COPY. |

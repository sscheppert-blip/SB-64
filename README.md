# SB-64

Analog-style acid bassline synthesizer plugin with a 64-step sequencer. AU, VST3, CLAP and standalone
for macOS, Windows and Linux, built with JUCE.

> **Status: early version (0.2.1).** Everything works; fine sound tuning by ear is still going on.
> Downloads under **Releases**.

**Manual:** [English](docs/Manual.md) · [Deutsch](docs/Handbuch.md)

![SB-64 front panel](docs/images/panel.png)

## Features

- **Voice:** one oscillator (saw, square, triangle) with a sub oscillator one octave down, noise and
  audio-rate FM from the LFO
- **Detune:** bucket-brigade style doubling for a wide, swarming unison sound
- **Filter:** aggressive two-pole low-pass/band-pass filter that self-oscillates without losing the bass
- **Drive:** blend from gentle saturation to raw distortion
- **Envelopes:** amp and filter envelopes, per-step pitch envelope for percussive "peow" hits
- **LFO:** ten shapes, free or tempo-synced, retrigger per step, start phase
- **Sequencer:** up to 64 steps with per-step gate, accent, slide, cutoff modulation, pitch envelope,
  probability and micro-tuning; play directions, swing, transpose, scales (incl. microtonal user scales),
  arpeggiator and a random pattern generator
- **Memory:** 8 banks × 16 patterns, each with its own play direction and first/last step; pattern chaining
- **Factory sounds:** 16 sounds, on the panel and as host programs
- **MIDI:** play it from a keyboard, sync to the host tempo, mod wheel onto the cutoff, patterns by bank
  select + program change; the sequencer and arpeggiator send their notes to MIDI out

## Installation

Installation is explained in the manual ([English](docs/Manual.md#2-installation) ·
[Deutsch](docs/Handbuch.md#2-installation)).

**macOS:** the plugins are not notarized by Apple, so macOS may block them. The manual explains the
one-line fix ([English](docs/Manual.md#unblocking-plugins-on-macos) ·
[Deutsch](docs/Handbuch.md#plugins-auf-macos-freigeben)).

## License

Public domain ([Unlicense](LICENSE)) – do whatever you like with it, no credit needed.
The plugins are built with [JUCE](https://juce.com), which has its own license terms.

<sub>Building from source: [docs/Development.md](docs/Development.md)</sub>

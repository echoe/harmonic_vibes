# Multihead Sequencer — JUCE Plugin

A MIDI-output step sequencer inspired by the Moog Subharmonicon, with **multiple independent playheads** running over a shared pitch sequence, each with its own step count and rhythm divisor.
All text here is written by Claude. Claude is taking the wheel here and I'm basically just sharing it.

---

## Concept

The Subharmonicon uses a shared pool of pitches and multiple sequencers that step through them at different rates derived from subharmonics. This plugin takes that idea further:

| Feature | Subharmonicon | Multihead Sequencer |
|---|---|---|
| Shared pitch pool | ✓ 4 pitches | ✓ 8 pitches |
| Number of playheads | 2 | 4 |
| Steps per playhead | Fixed | **1–16, per playhead** |
| Rhythm per playhead | Subharmonic dividers | **Freely choosable divisor** |
| MIDI output | ✗ | ✓ |

---

## Parameters

### Global
| Parameter | Range | Description |
|---|---|---|
| BPM | 20–300 | Fallback tempo when no DAW sync |

### Pitch Sequence (shared)
| Parameter | Range | Description |
|---|---|---|
| Pitch 1–8 | 0–127 | MIDI note numbers for each pitch slot |

### Per Playhead (×4)
| Parameter | Range | Description |
|---|---|---|
| Active | On/Off | Enable/disable this playhead |
| Steps | 1–16 | How many steps before looping |
| Rhythm | 8 choices | Step rate relative to quarter note: 1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16 |
| Volume | 0–1 | Controls MIDI velocity (0–127) |

---

## How the pitch pool works

All four playheads share a **single global pitch index** that advances each time *any* playhead fires. This creates polyrhythmic pitch sequences that evolve as playheads with different step counts and rhythms combine. The result is emergent melodic patterns much like the Subharmonicon's interplay between its two sequencers.

---

## Building

### Prerequisites
- CMake ≥ 3.22
- A C++17 compiler (Xcode / MSVC / Clang)
- (Optional) JUCE cloned locally at `../JUCE` to avoid FetchContent download

### Steps

```bash
git clone <this-repo> MultiheadSequencer
cd MultiheadSequencer

# CMake configure (downloads JUCE via FetchContent if not found locally)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# The plugin will be in:
#   build/MultiheadSequencer_artefacts/Release/VST3/
#   build/MultiheadSequencer_artefacts/Release/AU/      (macOS only)
#   build/MultiheadSequencer_artefacts/Release/Standalone/
```

### Using a local JUCE copy (faster, avoids downloads)

```bash
git clone https://github.com/juce-framework/JUCE.git ../JUCE

# Then edit CMakeLists.txt: comment out FetchContent and add instead:
# add_subdirectory(../JUCE JUCE)
```

---

## Usage

1. Load in your DAW as a MIDI instrument/effect plugin, or run the Standalone.
2. Route MIDI output to a synth of your choice.
3. Set your pitch pool (the 8 vertical sliders at the top).
4. Enable playheads and adjust:
   - **Steps**: try 3, 5, 7 for evolving patterns
   - **Rhythm**: use different divisors across playheads for polyrhythm (e.g. 1/2, 1/3, 1/4)
5. Hit play in your DAW — or set BPM in standalone mode.

---

## Architecture

```
PluginProcessor
├── apvts               AudioProcessorValueTreeState (all parameters + preset save/load)
├── pitchParams[8]      shared MIDI pitch pool (atomic float* → MIDI note 0-127)
├── playheads[4]        Playhead structs with accumulator, stepIndex, param pointers
├── currentSteps[4]     atomic<int> exposed to editor for LED display
└── pitchIndex          atomic<int> global pitch pool cursor

PluginEditor (30 Hz timer)
├── PitchStepComponent[8]    vertical sliders + highlight ring
└── PlayheadRowComponent[4]  LED strip + steps/rhythm/volume controls
```

---

## Extension Ideas

- **Independent pitch modes**: give each playhead its own pitch sequence instead of the shared pool
- **Gate length per step**: per-step note length control
- **Probability per step**: each step fires with a set probability
- **Transpose per playhead**: semitone offset per playhead
- **Step skip/mute**: toggle individual steps on/off per playhead
- **Clock input**: sync to incoming MIDI clock instead of host tempo
